// Private one-frame observations before and after renderer texture binding.
namespace terrain_probe {
using Prepare=void(*)(void*,void*,void*,void*);
Prepare original=nullptr;
using Flush=void(*)(void*);
Flush original_flush=nullptr;
bool in_flush=false;
unsigned flushes=0;
Json draw_records=Json::array();
bool contents_enabled=false;
Json contents=Json::object();
bool enabled=false,observing=false,attempted=false,complete=false;
uint64_t selected_step=0;
unsigned preparations=0;
unsigned map_flag_4a0=0;
std::string error;
Json programs=Json::object(),records=Json::array();
struct Api {
    void (APIENTRY *get)(unsigned,int*)=nullptr;
    unsigned (APIENTRY *get_error)()=nullptr;
    void (APIENTRY *program)(unsigned,unsigned,int*)=nullptr;
    int (APIENTRY *location)(unsigned,const char*)=nullptr;
    void (APIENTRY *uniform)(unsigned,int,int*)=nullptr;
    void (APIENTRY *active)(unsigned)=nullptr;
    void (APIENTRY *level)(unsigned,int,unsigned,int*)=nullptr;
    void (APIENTRY *tex_image)(unsigned,int,unsigned,unsigned,void*)=nullptr;
    void (APIENTRY *pixel_store)(unsigned,int)=nullptr;
    void (APIENTRY *bind_buffer)(unsigned,unsigned)=nullptr;
    void (APIENTRY *uniform_float)(unsigned,int,float*)=nullptr;
} gl;
void require(bool ok,const char* cause){if(!ok)throw std::runtime_error(cause);}
void check(const char* operation){auto e=gl.get_error();if(e)throw std::runtime_error(std::string("Terrain probe GL error at ")+operation+": "+std::to_string(e));}
void initialize(){
    if(gl.get)return;
    wait_render::resolve(gl.get,"glGetIntegerv");wait_render::resolve(gl.get_error,"glGetError");
    wait_render::resolve(gl.program,"glGetProgramiv");
    wait_render::resolve(gl.location,"glGetUniformLocation");wait_render::resolve(gl.uniform,"glGetUniformiv");
    wait_render::resolve(gl.active,"glActiveTexture");wait_render::resolve(gl.level,"glGetTexLevelParameteriv");
    if(contents_enabled){
        wait_render::resolve(gl.tex_image,"glGetTexImage");wait_render::resolve(gl.pixel_store,"glPixelStorei");
        wait_render::resolve(gl.bind_buffer,"glBindBuffer");wait_render::resolve(gl.uniform_float,"glGetUniformfv");
    }
}
std::string fragment_source(uintptr_t variant,unsigned program){
    const auto object=variant+0xf0;
    const auto pointer=read<uintptr_t>(object),length=read<uint64_t>(object+8);
    require(pointer&&length>0&&length<131072,"Terrain probe native fragment source pointer/length outside bound");
    std::string source(static_cast<size_t>(length)+1,'\0');SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(pointer),source.data(),source.size(),&copied)||copied!=source.size())
        throw std::runtime_error("Terrain probe native fragment read failed program="+std::to_string(program)+" bytes="+std::to_string(length+1));
    require(source.back()=='\0'&&source.find('\0')==length,"Terrain probe native fragment terminator differs");
    require(read<uintptr_t>(object)==pointer&&read<uint64_t>(object+8)==length,"Terrain probe native fragment changed during read");
    source.resize(static_cast<size_t>(length));return source;
}
const Json& classify(uintptr_t variant,unsigned program){
    const auto key=std::to_string(program);
    if(programs.contains(key)){
        require(programs.at(key).at("native_variant").get<uintptr_t>()==variant,"Terrain probe native program owner changed within frame");
        return programs.at(key);
    }
    require(programs.size()<128,"Terrain probe unique program limit exceeded");
    int count=-1;gl.program(program,0x8b85,&count);
    programs[key]={{"native_variant",variant},{"program",program},{"attached_shader_count",count}};
    check("attached shader count");
    if(count<0||count>8)throw std::runtime_error("Terrain probe attached shader count outside 0..8 program="+std::to_string(program)+" count="+std::to_string(count));
    // Flush passes these retained strings directly to the renderer compiler.
    // Linked programs can detach their shader objects; attachment is not identity.
    const auto source=fragment_source(variant,program);
    const bool terrain=source.find("terrainColorTexture")!=std::string::npos&&source.find("terrainNormalTexture")!=std::string::npos;
    programs[key]["terrain"]=terrain;
    programs[key]["fragment_source_sha256"]=digest(source);
    programs[key]["source_owner"]="native variant fragment passed to renderer Compile";
    if(terrain)programs[key]["fragment_source"]=source;
    return programs.at(key);
}
struct ActiveUnit {
    int saved=0;
    ActiveUnit(){gl.get(0x84e0,&saved);}
    ~ActiveUnit(){gl.active(static_cast<unsigned>(saved));}
};
Json sampler(unsigned program,const char* name,int units){
    const int location=gl.location(program,name);
    if(location<0)return {{"name",name},{"active",false}};
    int unit=-1;gl.uniform(program,location,&unit);
    require(unit>=0&&unit<units,"Terrain probe sampler unit outside GL limit");
    ActiveUnit restore;gl.active(0x84c0u+static_cast<unsigned>(unit));
    int texture=0,width=0,height=0,format=0;gl.get(0x8069,&texture);
    if(texture){gl.level(0x0de1,0,0x1000,&width);gl.level(0x0de1,0,0x1001,&height);gl.level(0x0de1,0,0x1003,&format);}
    check("sampler storage read");
    return {{"name",name},{"active",true},{"unit",unit},{"texture_2d",texture},{"width",width},{"height",height},{"internal_format",format}};
}
struct PackState {
    static constexpr unsigned keys[8]={0x0d00,0x0d01,0x0d02,0x0d03,0x0d04,0x0d05,0x806b,0x806c};
    int values[8]{},buffer=0;
    PackState(){
        gl.get(0x88ed,&buffer);
        for(unsigned i=0;i<8;++i)gl.get(keys[i],&values[i]);
        gl.bind_buffer(0x88eb,0);
        for(unsigned i=0;i<8;++i)gl.pixel_store(keys[i],keys[i]==0x0d05?1:0);
    }
    ~PackState(){
        for(unsigned i=0;i<8;++i)gl.pixel_store(keys[i],values[i]);
        gl.bind_buffer(0x88eb,static_cast<unsigned>(buffer));
    }
};
Json float_value(float value){
    uint32_t bits=0;std::memcpy(&bits,&value,sizeof bits);
    return {{"bits",bits},{"value",(bits&0x7f800000u)==0x7f800000u?Json(nullptr):Json(value)}};
}
Json uniform_value(unsigned program,const char* name,unsigned count){
    const auto location=gl.location(program,name);
    if(location<0)return {{"name",name},{"active",false}};
    float values[16]{};gl.uniform_float(program,location,values);check("content uniform read");
    Json result=Json::array();for(unsigned i=0;i<count;++i)result.push_back(float_value(values[i]));
    return {{"name",name},{"active",true},{"components",result}};
}
Json texture_content(const Json& binding){
    require(binding.at("active").get<bool>(),"Terrain content requested inactive sampler");
    const auto texture=binding.at("texture_2d").get<int>();
    const auto width=binding.at("width").get<int>(),height=binding.at("height").get<int>();
    require(texture>0&&width>0&&height>0,"Terrain content texture storage absent");
    const uint64_t count=uint64_t(width)*uint64_t(height)*4;
    require(count<=16777216,"Terrain content exceeds 64 MiB per texture");
    std::vector<float> pixels(static_cast<size_t>(count));
    ActiveUnit active_restore;
    gl.active(0x84c0u+binding.at("unit").get<unsigned>());
    int actual=0;gl.get(0x8069,&actual);require(actual==texture,"Terrain content binding changed");
    int saved[8]{},buffer=0;gl.get(0x88ed,&buffer);
    for(unsigned i=0;i<8;++i)gl.get(PackState::keys[i],&saved[i]);
    {
        PackState restore;check("content pack setup");
        gl.tex_image(0x0de1,0,0x1908,0x1406,pixels.data());check("texture content read");
    }
    int restored=0;gl.get(0x88ed,&restored);require(restored==buffer,"Terrain content pack buffer restoration differs");
    for(unsigned i=0;i<8;++i){gl.get(PackState::keys[i],&restored);require(restored==saved[i],"Terrain content pack state restoration differs");}
    check("content pack restoration");
    uint64_t finite[4]{},nonfinite[4]{},zero[4]{};
    double sum[4]{};float low[4]{},high[4]{};
    for(size_t i=0;i<pixels.size();++i){
        const auto channel=i%4;const auto value=pixels[i];uint32_t bits=0;std::memcpy(&bits,&value,sizeof bits);
        if((bits&0x7f800000u)==0x7f800000u){++nonfinite[channel];continue;}
        if(!finite[channel])low[channel]=high[channel]=value;
        else{if(value<low[channel])low[channel]=value;if(value>high[channel])high[channel]=value;}
        ++finite[channel];zero[channel]+=value==0;sum[channel]+=value;
    }
    Json channels=Json::array(),grid=Json::array();
    for(unsigned i=0;i<4;++i)channels.push_back({{"finite",finite[i]},{"nonfinite",nonfinite[i]},{"zero",zero[i]},
        {"minimum",finite[i]?Json(low[i]):Json(nullptr)},{"maximum",finite[i]?Json(high[i]):Json(nullptr)},
        {"mean",finite[i]?Json(sum[i]/static_cast<double>(finite[i])):Json(nullptr)}});
    for(int y=0;y<8;++y)for(int x=0;x<8;++x){
        const auto px=(uint64_t(2*x+1)*uint64_t(width))/16,py=(uint64_t(2*y+1)*uint64_t(height))/16;
        Json rgba=Json::array();for(unsigned c=0;c<4;++c)rgba.push_back(float_value(pixels[static_cast<size_t>((py*uint64_t(width)+px)*4+c)]));
        grid.push_back({{"x",px},{"y",py},{"rgba",rgba}});
    }
    return {{"binding",binding},{"transfer","level0 RGBA FLOAT"},{"bytes",count*sizeof(float)},
        {"sha256",digest(std::string(reinterpret_cast<const char*>(pixels.data()),pixels.size()*sizeof(float)))},
        {"channels",channels},{"sample_grid",grid}};
}
Json generator_state(){
    const auto map=presentation_probe::typed(read<uintptr_t>(image+0x13df8c0),0x1742860);
    const auto data=read<uintptr_t>(map+0x1a8);require(data!=0,"Terrain content MapData absent");
    const auto gpu=read<uintptr_t>(data+8);require(gpu!=0,"Terrain content MapData_GPU absent");
    const auto blender=read<uintptr_t>(gpu+0x398);require(blender!=0,"Terrain content color blender absent");
    Json flags=Json::object();for(uintptr_t i=0xf40;i<=0xf46;++i)flags[std::to_string(i)]=read<uint8_t>(gpu+i);
    Json targets=Json::array();
    for(uintptr_t slot:{uintptr_t(0x40),uintptr_t(0x48),uintptr_t(0x50)}){
        const auto target=read<uintptr_t>(blender+slot);
        Json entry={{"slot",slot},{"render_target",target}};
        if(target){
            const auto textures=read<uintptr_t>(target+0x40);entry["texture_array"]=textures;
            if(textures){const auto texture=read<uintptr_t>(textures);entry["texture"]=texture;
                if(texture){const auto internal=read<uintptr_t>(texture+8);entry["internal"]=internal;
                    if(internal)entry["gl_texture"]=read<uint32_t>(internal+0x30);}}
        }
        targets.push_back(entry);
    }
    return {{"map",map},{"map_data",data},{"gpu",gpu},{"state_f48",read<int32_t>(gpu+0xf48)},
        {"flags",flags},{"color_blender",blender},{"elapsed_bits_428",read<uint32_t>(blender+0x428)},
        {"fraction_bits_42c",read<uint32_t>(blender+0x42c)},{"duration_bits_434",read<uint32_t>(blender+0x434)},
        {"targets",targets}};
}
void capture_contents(unsigned program,const Json& samplers){
    if(!contents_enabled||!contents.empty()||!samplers.at(0).at("active").get<bool>())return;
    contents={{"attempted",true},{"complete",false},{"program",program},{"generator",generator_state()}};
    Json uniforms=Json::array();
    uniforms.push_back(uniform_value(program,"gain",1));
    uniforms.push_back(uniform_value(program,"fogColor",3));
    uniforms.push_back(uniform_value(program,"lightSource[0].position",3));
    contents["uniforms"]=uniforms;contents["textures"]=Json::array();
    int active=0;gl.get(0x84e0,&active);
    for(const auto& binding:samplers)contents["textures"].push_back(texture_content(binding));
    int restored=0;gl.get(0x84e0,&restored);require(restored==active,"Terrain content active texture restoration differs");
    check("content completion");contents["complete"]=true;
}

void capture(void* variant,void* material,void* target,bool after_flush=false){
    require(thread==GetCurrentThreadId()&&control.entered&&control.phase==mt2::determinism::Gate::Phase::Stepping&&control.attempted==selected_step,
        "Terrain probe preparation escaped selected original frame");
    if(!after_flush)require(++preparations<=8192,"Terrain probe preparation limit exceeded");
    check("preparation precondition");
    const auto native_variant=reinterpret_cast<uintptr_t>(variant);
    const auto program=read<uint32_t>(native_variant+0x114);
    int current_program=0;gl.get(0x8b8d,&current_program);
    if(!program||program==UINT32_MAX)throw std::runtime_error("Terrain probe native prepared program invalid program="+std::to_string(program)+" current="+std::to_string(current_program));
    if(!classify(native_variant,program).at("terrain").get<bool>())return;
    auto& destination=after_flush?draw_records:records;
    require(destination.size()<(after_flush?4096u:512u),"Terrain probe terrain record limit exceeded");
    if(after_flush&&current_program!=static_cast<int>(program))
        throw std::runtime_error("Terrain probe post-flush program differs prepared="+std::to_string(program)+" current="+std::to_string(current_program));
    int draw=0,read_fb=0,units=0,active=0;gl.get(0x8ca6,&draw);gl.get(0x8caa,&read_fb);gl.get(0x8b4d,&units);gl.get(0x84e0,&active);
    require(units>0&&units<=1024,"Terrain probe texture unit limit differs");
    Json samplers=Json::array();
    for(auto name:{"terrainColorTexture","directionalLightTexture","ambientLightTexture"})samplers.push_back(sampler(static_cast<unsigned>(program),name,units));
    int restored=0;gl.get(0x84e0,&restored);require(active==restored,"Terrain probe active texture restoration differs");check("preparation observation/restore");
    if(after_flush)capture_contents(program,samplers);
    destination.push_back({{"boundary",after_flush?"after_renderer_flush_before_draw":"after_prepare_before_texture_binding"},{"flush",flushes},{"preparation",preparations},{"program",program},{"current_program",current_program},{"variant_bits",read<uint32_t>(reinterpret_cast<uintptr_t>(variant)+0x118)},
        {"material",reinterpret_cast<uintptr_t>(material)},{"render_target",reinterpret_cast<uintptr_t>(target)},
        {"draw_framebuffer",draw},{"read_framebuffer",read_fb},{"samplers",samplers}});
}
void prepare(void* variant,void* material,void* values,void* target){
    original(variant,material,values,target);
    if(!observing||!error.empty())return;
    try{capture(variant,material,target);}
    catch(const std::exception& problem){error=problem.what();}
    catch(...){error="Terrain probe observation raised an unknown exception";}
}
void flush(void* renderer){
    if(!observing||!error.empty()){original_flush(renderer);return;}
    // FlushRenderState prepares the shader, then binds its textures. Its caller
    // next dispatches the display-list draw. Do not sample between those steps.
    if(in_flush){
        error="Terrain probe nested renderer flush";original_flush(renderer);return;
    }
    struct Scope {
        Scope(){in_flush=true;}
        ~Scope(){in_flush=false;}
    } scope;
    const auto before=preparations;
    ++flushes;
    original_flush(renderer);
    if(!error.empty())return;
    try{
        require(flushes<=8192,"Terrain probe renderer flush limit exceeded");
        // Cached shader/material/values can skip Prepare inside this flush.
        // Read the renderer's actual current variant, never the last callback.
        const auto object=reinterpret_cast<uintptr_t>(renderer);
        const auto shader=read<uintptr_t>(object+0x900);
        require(shader!=0,"Terrain probe post-flush renderer shader absent");
        const auto variant=read<uintptr_t>(shader+0xa0);
        require(variant!=0,"Terrain probe post-flush current shader variant absent");
        const auto count=draw_records.size();
        capture(reinterpret_cast<void*>(variant),reinterpret_cast<void*>(read<uintptr_t>(object+0x8f0)),
            reinterpret_cast<void*>(read<uintptr_t>(object+0x160)),true);
        if(draw_records.size()!=count)draw_records.back()["prepare_called_in_flush"]=preparations!=before;
    }
    catch(const std::exception& problem){error=problem.what();}
    catch(...){error="Terrain probe post-flush observation raised an unknown exception";}
}
void begin(){
    if(!enabled||control.attempted!=selected_step||control.phase!=mt2::determinism::Gate::Phase::Stepping)return;
    require(!attempted&&!observing,"Terrain probe selected frame was already attempted");attempted=true;
    require(thread==GetCurrentThreadId()&&control.entered,"Terrain probe requires entered game-thread frame");
    auto map=presentation_probe::typed(read<uintptr_t>(image+0x13df8c0),0x1742860);
    map_flag_4a0=presentation_probe::flag(map+0x4a0);
    require(read<int32_t>(map+0x3d4)==7&&presentation_probe::flag(map+0x47f)==1&&presentation_probe::flag(map+0x488)==0,
        "Terrain probe selected frame requires ready map state 7 with completed rez");
    initialize();check("frame precondition");observing=true;
}
void finish(){
    if(!observing)return;observing=false;
    if(!error.empty())throw std::runtime_error(error);
    require(!draw_records.empty(),"Terrain probe selected frame had no recognized terrain post-flush observations");
    require(!contents_enabled||contents.value("complete",false),"Terrain content selected frame lacked completed normal-pass readback");
    complete=true;
}
void abandon(){if(observing){observing=false;if(error.empty())error="Terrain probe original frame did not complete";}}
Json status(){return {{"enabled",enabled},{"contents_enabled",contents_enabled},{"contents",contents},{"selected_step",selected_step},{"attempted",attempted},{"complete",complete},
    {"preparations",preparations},{"flushes",flushes},{"draw_records",draw_records},{"map_flag_4a0",map_flag_4a0},{"error",error},{"programs",programs},{"records",records}};}
void install(const Json& config){
    enabled=config.value("terrain_probe",false);if(!enabled)return;
    contents_enabled=config.value("terrain_probe_contents",false);
    require(config.value("presentation_probe",false),"Terrain probe requires private presentation probe");
    require(config.contains("terrain_probe_step")&&config.at("terrain_probe_step").is_number_unsigned(),"Terrain probe requires explicit unsigned frame index");
    selected_step=config.at("terrain_probe_step").get<uint64_t>();require(selected_step>0,"Terrain probe frame index must be positive");
    presentation_rng::verify(0x9d8600,6048,"14930cf7bca1a63b5cc098feea46149d371ed092c2c6b1217d9538a54d01e648");
    presentation_rng::verify(0x9cd480,16,"c08284fc39d8e46ba538628fc6b6ef689099dd976e46300226badd0aef62233c");
    presentation_rng::verify(0x32fd30,1088,"e28e495258186807f024b764b44d8abe13d2aa9d612d871f683869a7440ec87a");
    presentation_rng::verify(0x9d6140,9408,"decd984108da1dce7b662dd298a70de16c803bfe97ca73ce36c18c9734a3808d");
    presentation_rng::verify(0x9dd970,624,"e365e318415cf8c21dba61b3e61c8730f672dd2696a86d422449f26047e70ba2");
    presentation_rng::verify(0x9bb5c0,3472,"fdefdd38c6fc5005954efd5b3b7bd7054b4f8243e3a7067c5d6a0f244bed0a84");
    presentation_rng::verify(0x9be1b0,3744,"6c79f2f2d36b0deb3a5b2b24e67956e8d3fa89b9f716e6165a4660071c264c86");
    presentation_rng::verify(0x9bf050,7136,"73b5d8110fcd06ee67a8c8920c455d5bbb08545cfd9e4bf69b60e1cd82700254");
    presentation_rng::verify(0x9cd470,16,"900895f2ed8006c8decb5e65aa7b4d4a33e00c9496934b039f7b9fa906945e95");
    presentation_rng::verify(0x32e890,5264,"5e6f092c80fc48bbb0fedc56b386af43f47040ccf3829989c25d9e97a2852db7");
    presentation_rng::verify(0x336f70,240,"e5d2ab70e87d8560cb9377eff2f4aa90f304e9a7964711d9f80485643cc37b89");
    presentation_rng::verify(0x33b0f0,32,"7358c59298d3334bbf47c5b357aeb3c2f9ae6a42f9e4e37637b124a0519abecb");
    presentation_rng::verify(0x33c690,1168,"8e16898e542a4079d6c0f4a9979086d2d6c18b844e3798e88e9010f9a86e7035");
    presentation_rng::verify(0x1e85c0,912,"e25cedfc984b7d01c370901a89cae7d348a64ab39366e08c543d56a6bd0fbd06");
    presentation_rng::verify(0x1e8d00,928,"d9348e70b127f775af2a21addf20472c8a926d95024c8aaa2a488d1137d8e623");
    hook(reinterpret_cast<void*>(image+0x9d8600),reinterpret_cast<void*>(&prepare),reinterpret_cast<void**>(&original));
    hook(reinterpret_cast<void*>(image+0x9be1b0),reinterpret_cast<void*>(&flush),reinterpret_cast<void**>(&original_flush));
}
}
