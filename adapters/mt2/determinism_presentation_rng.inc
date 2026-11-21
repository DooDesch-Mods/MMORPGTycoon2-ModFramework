// Include inside lab after read<T>; this module is private experiment code.
namespace presentation_rng {
namespace core=mt2::determinism::presentation;
using Float=float(*)(void*,float);
using Seed=void(*)(void*,uint64_t);
Float original_float=nullptr;
Seed original_seed=nullptr;
std::atomic<bool> armed=false;
std::atomic<uint64_t> generation=0;
bool installed=false,digital_enabled=false;
std::array<core::Counters,16> digital_counts;
constexpr std::array<const char*,16> digital_names={"dottedDirection","dottedLength","solidDirection","solidLength","lineDepth","lineXY","dotRefresh","dotOffset","blockInnerRadius","blockRingXY","blockRingDepth","blockXY","blockDepth","cameraRotationTarget","cameraDirectionX","cameraDirectionY"};
std::array<core::Counters,2> counts;
thread_local core::Local local;
thread_local core::Local building_widget_local;
thread_local core::BuildingWidgetQuery building_widget_scope;
core::Counters building_widget_count;
using BuildingWidgetOwner=bool(*)(const void*,std::string&);
BuildingWidgetOwner building_widget_owner=nullptr;
using GetPadEntrance=void*(*)(void*,const void*,int,int);
GetPadEntrance original_get_pad_entrance=nullptr;
bool building_widget_installed=false;
thread_local core::Local puppet_local;
thread_local core::BlueprintPuppet puppet_scope;
core::Counters puppet_count;
using PuppetConstructor=void(*)(void*,void*,bool,bool);
PuppetConstructor original_puppet_constructor=nullptr;
bool puppet_installed=false;
__declspec(noinline) void puppet_constructor(void* puppet,void* actor,bool preview,bool in_world){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool previous=puppet_scope.active;
    puppet_scope.active=armed.load(std::memory_order_acquire)&&caller==image+0x4054b9;
    __try {original_puppet_constructor(puppet,actor,preview,in_world);}
    __finally {puppet_scope.active=previous;}
}
thread_local core::Local scenery_pattern_local;
thread_local core::SceneryPattern scenery_pattern_scope;
core::Counters scenery_pattern_count;
using PerlinConstructor=void(*)(void*,int,float,float);
PerlinConstructor original_perlin_constructor=nullptr;
bool scenery_pattern_installed=false;
__declspec(noinline) void perlin_constructor(void* perlin,int octaves,float persistence,float bias){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool previous=scenery_pattern_scope.active;
    // All three pattern constructors tail-call Perlin, preserving the exact
    // Scenery::_SetupInstances return address on the native stack.
    scenery_pattern_scope.active=armed.load(std::memory_order_acquire)&&
        caller>=image&&core::SceneryPattern::constructor(caller-image);
    __try {original_perlin_constructor(perlin,octaves,persistence,bias);}
    __finally {scenery_pattern_scope.active=previous;}
}

thread_local core::Local character_jump_local;
core::Counters character_jump_count;
bool character_jump_installed=false;

std::mutex failure_mutex;
std::string failure;
std::mutex building_widget_failure_mutex;
std::string building_widget_failure;

bool admit_building_widget(const void* building,uintptr_t caller){
    if(!armed.load(std::memory_order_acquire)||caller!=image+0x40e614||!building_widget_owner)return false;
    try{std::string cause;const bool admitted=building_widget_owner(building,cause);
        if(!admitted&&!cause.empty()){std::lock_guard lock(building_widget_failure_mutex);if(building_widget_failure.empty())building_widget_failure=cause;}
        return admitted;
    }catch(const std::exception& problem){std::lock_guard lock(building_widget_failure_mutex);if(building_widget_failure.empty())building_widget_failure=problem.what();return false;}
    catch(...){std::lock_guard lock(building_widget_failure_mutex);if(building_widget_failure.empty())building_widget_failure="Building widget owner raised unknown exception";return false;}
}
__declspec(noinline) void* get_pad_entrance(void* output,const void* building,int pad,int entrance){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto previous=building_widget_scope;
    building_widget_scope.active=admit_building_widget(building,caller);
    __try{return original_get_pad_entrance(output,building,pad,entrance);}
    __finally{building_widget_scope=previous;}
}

void verify(uintptr_t offset,size_t size,const char* expected){
    std::string bytes(size,'\0');SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+offset),bytes.data(),size,&copied)||copied!=size)
        throw std::runtime_error("Presentation RNG code read failed rva="+std::to_string(offset));
    if(digest(bytes)!=expected)throw std::runtime_error("Presentation RNG original code differs rva="+std::to_string(offset));
}
void verify_digital(){
    verify(0x16fac0,432,"20e8bebe65da285f929449f89b763e5afe4e19a10f61c7d18b1a5eb24f323fc2");
    verify(0x170eb0,2608,"01960afef08d26d400395426e7a2a96690d57145f83ffec40edc1f82b7bc0db5");
    verify(0x16d830,1760,"a6166024bd3173e40ec626688af27c01752ecd87f2f03f9a97e07e25c487ce93");
    verify(0x16e3f0,208,"1c3ac6900715f0dc752d25b111d2f42d123c40dac632c221702cd30b004b274b");
    verify(0x16e4c0,3040,"4bfce55dba844b51b5d96183bac758e537d147b9c208188ab2e18f13efa889c2");
    verify(0x17a610,3856,"70d7feb2b3e47993fa8b2191493ffe58f02087af0dc83deebfaa1da038241132");
    verify(0x16f8f0,240,"2612d82a8f6620a8d2e84360783e51d124dcdc71b9a9a76c370c726b6b02997d");
    verify(0x169020,112,"4661bcdd9ba9ff1fe6c65f85621c52cf2d77e9aecee824bfa4ea9e8c6f44a81f");
}
core::Selection select_digital(uintptr_t helper,uintptr_t caller,void* source){
    const auto caller_rva=caller>=image?caller-image:0;
    if(building_widget_installed&&armed.load(std::memory_order_acquire)&&building_widget_scope.matches(helper,caller_rva)){
        try{
            if(read<uintptr_t>(image+0x1650830)!=image+0x1c557a0)
                throw std::runtime_error("Building widget RNG Default object differs");
        }catch(const std::exception& problem){std::lock_guard lock(building_widget_failure_mutex);if(building_widget_failure.empty())building_widget_failure=problem.what();throw;}
        return core::select_building_widget(true,generation.load(std::memory_order_acquire),building_widget_scope,
            helper,caller_rva,source,reinterpret_cast<void*>(image+0x1c557a0),building_widget_local,building_widget_count,
            [](void* target,uint64_t seed){original_seed(target,seed);});
    }
    if(puppet_installed&&armed.load(std::memory_order_acquire)&&puppet_scope.matches(helper,caller_rva)){
        try{
            if(read<uintptr_t>(image+0x1650830)!=image+0x1c557a0)
                throw std::runtime_error("Blueprint Puppet RNG Default object differs");
        }catch(const std::exception& problem){
            std::lock_guard lock(failure_mutex);if(failure.empty())failure=problem.what();
            throw;
        }
        return core::select_blueprint_puppet(true,generation.load(std::memory_order_acquire),puppet_scope,
            helper,caller_rva,source,reinterpret_cast<void*>(image+0x1c557a0),puppet_local,puppet_count,
            [](void* target,uint64_t seed){original_seed(target,seed);});
    }
    if(scenery_pattern_installed&&armed.load(std::memory_order_acquire)&&scenery_pattern_scope.matches(helper,caller_rva)){
        try{
            if(read<uintptr_t>(image+0x1650830)!=image+0x1c557a0)
                throw std::runtime_error("Scenery pattern RNG Default object differs");
        }catch(const std::exception& problem){
            std::lock_guard lock(failure_mutex);if(failure.empty())failure=problem.what();
            throw;
        }
        return core::select_scenery_pattern(true,generation.load(std::memory_order_acquire),scenery_pattern_scope,
            helper,caller_rva,source,reinterpret_cast<void*>(image+0x1c557a0),scenery_pattern_local,scenery_pattern_count,
            [](void* target,uint64_t seed){original_seed(target,seed);});
    }
    if(character_jump_installed&&armed.load(std::memory_order_acquire)&&core::character_jump_site(helper,caller_rva)){
        try{
            if(read<uintptr_t>(image+0x1650830)!=image+0x1c557a0)
                throw std::runtime_error("Character jump RNG Default object differs");
        }catch(const std::exception& problem){
            std::lock_guard lock(failure_mutex);if(failure.empty())failure=problem.what();
            throw;
        }
        return core::select_character_jump(true,generation.load(std::memory_order_acquire),helper,caller_rva,
            source,reinterpret_cast<void*>(image+0x1c557a0),character_jump_local,character_jump_count,
            [](void* target,uint64_t seed){original_seed(target,seed);});
    }
    const auto index=caller>=image?core::digital_site(helper,caller-image):16;
    if(!digital_enabled||!armed.load(std::memory_order_acquire)||index==16)return {source};
    auto& count=digital_counts[index];++count.matched;
    if(reinterpret_cast<uintptr_t>(source)!=image+0x1c557a0){++count.nondefault;return {source};}
    try{
        if(read<uintptr_t>(image+0x1650830)!=image+0x1c557a0)
            throw std::runtime_error("Digital RNG Default object differs");
    }catch(const std::exception& problem){
        std::lock_guard lock(failure_mutex);if(failure.empty())failure=problem.what();
        return {source};
    }
    const auto epoch=generation.load(std::memory_order_acquire);
    if(!epoch)throw std::runtime_error("Digital RNG generation is zero");
    if(local.generation!=epoch){
        original_seed(&local.random,uint64_t{0x4d543250524553});
        local.generation=epoch;++count.initialized;
    }
    ++count.attempted;return {&local.random,&count};
}
void verify_callers(){
    verify(0x823800,1056,"691c6c37dd91e81a65cc345bb1bff9ceec3a1b161c5eb5c3949757eed1c90bc2");
    verify(0x8496d0,336,"1d769709877af7bc542c2a940cca47628aa5e4ad056516e47a2eedac197fc99d");
    verify(0xa06510,240,"bc363d294fbe60db8a6d564e9adeea6187051b306efa10c8b88064155fe8da1c");
}
__declspec(noinline) float get_float(void* source,float limit){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto selected=caller>=image?core::site(caller-image):core::Site::Other;
    if(!armed.load(std::memory_order_acquire)||selected==core::Site::Other)return rng_trace::invoke(0xa069b0,caller,source,original_float,limit);
    uintptr_t current=0;
    try{
        current=read<uintptr_t>(image+0x1650830);
        if(current!=image+0x1c557a0)throw std::runtime_error("Presentation RNG Default object differs");
    }catch(const std::exception& problem){
        {std::lock_guard lock(failure_mutex);if(failure.empty())failure=problem.what();}
        // Admission failed before any redirected native call. Preserve vanilla
        // behavior once and fail the experiment at its next held observation.
        return rng_trace::invoke(0xa069b0,caller,source,original_float,limit);
    }
    const auto epoch=generation.load(std::memory_order_acquire);
    auto& count=counts[selected==core::Site::Window?0:1];
    return core::dispatch(true,epoch,selected,source,reinterpret_cast<void*>(current),limit,local,count,
        [](void* state,uint64_t seed){original_seed(state,seed);},
        [caller](void* state,float max){return rng_trace::invoke(0xa069b0,caller,state,original_float,max);});
}
void install(const Json& config){
    digital_enabled=config.value("digital_rng",false);
    if(digital_enabled&&(!config.value("presentation_rng",false)||!config.value("rng_trace",false)))
        throw std::runtime_error("Digital RNG requires presentation_rng and rng_trace hooks");
    if(!config.value("presentation_rng",false))return;
    if(digital_enabled)verify_digital();
    verify_callers();
    verify(0xa069b0,176,"21ffc7dcfd7a41a3cf567bbc6e451f4202f5773d0e6baac23072e9072c52a09a");
    original_seed=reinterpret_cast<Seed>(image+0xa06510);
    hook(reinterpret_cast<void*>(image+0xa069b0),reinterpret_cast<void*>(&get_float),reinterpret_cast<void**>(&original_float));
    if(config.value("rng_trace",false)){
        if(building_widget_owner){
            verify(0x40e3c0,720,"c83326c0ff184f077069d7cc9c99c2d464a3e0363ead78329f06ad3d1856c9ad");
            verify(0x40d680,1216,"104451abf60152865c2a81158e7a157b12eb852cde3a86208b48d491fbc84360");
            verify(0xdb690,208,"e5c7f84c914dbbcd15d457260beb0482f0a71f5948f07abfff2f4044eda52bc4");
            verify(0x481f60,16,"936e2840e93a6f4f62da4c5a7876b41f3a3c06898ceb0649636619f82794d3a0");
            native_routes->catalog("framework.rng.building-widget-query",0x40d680,true);
            native_services->reserve("framework.rng.building-widget-query");
            hook(reinterpret_cast<void*>(image+0x40d680),reinterpret_cast<void*>(&get_pad_entrance),reinterpret_cast<void**>(&original_get_pad_entrance));
            building_widget_installed=true;
        }
        verify(0x34b60,192,"882fb64f69170f94d00a9d976932c162434eac97e5f354dd5fb1e23f0fb7b5fa");
        verify(0x4053f0,672,"cc8439a8360821887c2f717e058a9b31ad46cdc370d6619678c966d1eda491e9");
        verify(0x3b87e0,1536,"f94aa85df15091fa474d2d8f7992c789a46891e3689b2873051236bdcd1c16b9");
        hook(reinterpret_cast<void*>(image+0x34b60),reinterpret_cast<void*>(&puppet_constructor),
            reinterpret_cast<void**>(&original_puppet_constructor));
        puppet_installed=true;
        verify(0x5d9c70,3024,"f56c4cc831dd024f61abcb7ba6ba815a011826b6b6fe258b31b7447371e587e5");
        verify(0x18fe70,64,"b2f448db96cc5d8babf5d27907e7a470672888c1dd19df1e2f06fb258741cdea");
        verify(0x190770,64,"7c493147c1de0e71d3cd8d97c87462251273a6cf9f4de964dad624a51d626b5a");
        verify(0x1913b0,64,"fb21bcf59dd6911ea91042fc2a65a77ff66870f106013c852b77131a92adbfbd");
        verify(0xa04b70,336,"8f5d850caee46d9ddde3703377d3a680d09adde1dda1e2577cccf26e42da6747");
        hook(reinterpret_cast<void*>(image+0xa04b70),reinterpret_cast<void*>(&perlin_constructor),
            reinterpret_cast<void**>(&original_perlin_constructor));
        scenery_pattern_installed=true;
        verify(0x23790,384,"c0a59fc275248c75745c462511a255ce34a8f071294216a849194db8d65df02d");
        verify(0x23bd0,272,"a3f19ba56e1c77a3a9e4cb19508318c4253ede582dac2c6660ff97603a5f75dc");
        verify(0x25940,32,"501d9889ec223b3669f63221dc9cec0f3080e55d9bc779a6b07516522220fc64");
        verify(0x25960,16,"5b609f249ba3ad6b38453541b3877326f5688e9f5446347f4ab2a05b0db6fd14");
        verify(0x288d0,16,"ab959da4e1674818e4fe45d08d20cffb350efc7cc7c22adb8f392806bbb78352");
        verify(0x288f0,16,"cdad0737793e845c4cfac4b6dfbf1144119d7cd8d3fa915dbb8fe1cd93be6142");
        verify(0x3a4e50,6816,"2c83236299d0f9f98cda64d612d4ddf02d3aab8b8543e25b6744c2628e56cfc5");
        character_jump_installed=true;
    }
    if(digital_enabled||puppet_installed||scenery_pattern_installed||character_jump_installed||building_widget_installed)rng_trace::select_source=&select_digital;
    installed=true;
}
void arm(){
    if(!installed)return;
    if(armed.load(std::memory_order_acquire))throw std::runtime_error("Presentation RNG already armed");
    verify_callers();
    if(read<uintptr_t>(image+0x1650830)!=image+0x1c557a0)throw std::runtime_error("Presentation RNG arm Default object differs");
    if(digital_enabled)verify_digital();
    generation.fetch_add(1,std::memory_order_release);
    armed.store(true,std::memory_order_release);
}
void cancel(){armed.store(false,std::memory_order_release);}
void check_building_widget(){
    {std::lock_guard lock(building_widget_failure_mutex);if(!building_widget_failure.empty())throw std::runtime_error(building_widget_failure);}
    if(building_widget_count.attempted.load()!=building_widget_count.completed.load())
        throw std::runtime_error("Building widget RNG call has no confirmed completion; no replay");
}
void check(){
    {std::lock_guard lock(failure_mutex);if(!failure.empty())throw std::runtime_error(failure);}
    if(puppet_count.attempted.load()!=puppet_count.completed.load())
        throw std::runtime_error("Blueprint Puppet RNG call has no confirmed completion; no replay");
    if(scenery_pattern_count.attempted.load()!=scenery_pattern_count.completed.load())
        throw std::runtime_error("Scenery pattern RNG call has no confirmed completion; no replay");
    if(character_jump_count.attempted.load()!=character_jump_count.completed.load())
        throw std::runtime_error("Character jump RNG call has no confirmed completion; no replay");
    check_building_widget();
    for(const auto& c:digital_counts)if(c.attempted.load()!=c.completed.load())
        throw std::runtime_error("Digital RNG call has no confirmed completion; no replay");
    for(const auto& c:counts)if(c.attempted.load()!=c.completed.load())
        throw std::runtime_error("Presentation RNG call has no confirmed completion; no replay");
}
Json status(){
    Json rows=Json::array();
    for(size_t i=0;i<counts.size();++i){const auto& c=counts[i];rows.push_back({
        {"site",i==0?"windowRandom":"timeOffset"},{"matched",c.matched.load()},
        {"nondefault",c.nondefault.load()},{"initialized",c.initialized.load()},
        {"attempted",c.attempted.load()},{"completed",c.completed.load()}});}
    for(size_t i=0;i<digital_counts.size();++i){const auto& c=digital_counts[i];rows.push_back({
        {"site",digital_names[i]},{"matched",c.matched.load()},{"nondefault",c.nondefault.load()},
        {"initialized",c.initialized.load()},{"attempted",c.attempted.load()},{"completed",c.completed.load()}});}
    rows.push_back({{"site","blueprintPuppet"},{"matched",puppet_count.matched.load()},
        {"nondefault",puppet_count.nondefault.load()},{"initialized",puppet_count.initialized.load()},
        {"attempted",puppet_count.attempted.load()},{"completed",puppet_count.completed.load()}});
    rows.push_back({{"site","sceneryPattern"},{"matched",scenery_pattern_count.matched.load()},
        {"nondefault",scenery_pattern_count.nondefault.load()},{"initialized",scenery_pattern_count.initialized.load()},
        {"attempted",scenery_pattern_count.attempted.load()},{"completed",scenery_pattern_count.completed.load()}});
    rows.push_back({{"site","characterJump"},{"matched",character_jump_count.matched.load()},
        {"nondefault",character_jump_count.nondefault.load()},{"initialized",character_jump_count.initialized.load()},
        {"attempted",character_jump_count.attempted.load()},{"completed",character_jump_count.completed.load()}});
    rows.push_back({{"site","buildingWidgetEntrance"},{"matched",building_widget_count.matched.load()},
        {"nondefault",building_widget_count.nondefault.load()},{"initialized",building_widget_count.initialized.load()},
        {"attempted",building_widget_count.attempted.load()},{"completed",building_widget_count.completed.load()}});
    std::string cause;{std::lock_guard lock(failure_mutex);cause=failure;}
    return {{"installed",installed},{"armed",armed.load()},{"generation",generation.load()},{"error",cause},
        {"digital_enabled",digital_enabled},{"blueprint_puppet_installed",puppet_installed},{"scenery_pattern_installed",scenery_pattern_installed},{"character_jump_installed",character_jump_installed},{"scope","two exact Character jump animation calls; exact Scenery pattern Perlin constructor; exact Blueprint Puppet constructor; two shader constructors; optional sixteen exact visual line/dot/block/camera-target calls in eight functions"},{"sites",rows}};
}
}
