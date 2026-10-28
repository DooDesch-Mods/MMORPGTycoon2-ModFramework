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
thread_local core::Local respawn_local;
thread_local core::RespawnQuery respawn_query;
core::Counters respawn_count;
using FindRespawn=void*(*)(void*);
FindRespawn original_find_respawn=nullptr;
bool respawn_installed=false;
__declspec(noinline) void* find_respawn(void* manager){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool previous=respawn_query.active;
    respawn_query.active=armed.load(std::memory_order_acquire)&&caller==image+0x5c3dc5;
    __try {return original_find_respawn(manager);}
    __finally {respawn_query.active=previous;}
}
thread_local core::Local puppet_local;
thread_local core::BlueprintPuppet puppet_scope;
core::Counters puppet_count;
using PuppetConstructor=void(*)(void*,void*,bool,bool);
PuppetConstructor original_puppet_constructor=nullptr;
bool puppet_installed=false;
__declspec(noinline) void puppet_constructor(void* puppet,void* actor,bool preview,bool in_world){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool previous=puppet_scope.active;
    puppet_scope.active=armed.load(std::memory_order_acquire)&&caller==image+0x4008f9;
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

void verify(uintptr_t offset,size_t size,const char* expected){
    std::string bytes(size,'\0');SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+offset),bytes.data(),size,&copied)||copied!=size)
        throw std::runtime_error("Presentation RNG code read failed rva="+std::to_string(offset));
    if(digest(bytes)!=expected)throw std::runtime_error("Presentation RNG original code differs rva="+std::to_string(offset));
}
void verify_digital(){
    verify(0x16e0f0,432,"9446082a9b7476b35ce56be9953311d8f56af52fdc9a75a9a0812de105afcc80");
    verify(0x16f4e0,2608,"e2ae82831a421c13459f20498381209beaf2b0620c356110c12efb2fbbf71a07");
    verify(0x16bf10,1760,"16e3cd546d9379ea9b37e5b12799f4b3d2570eabe7fb135b918ed0417714867c");
    verify(0x16cad0,208,"c81bd219118ed1cb6e2494a9f67a6db3fa3adb01c388fa5eec2f514ed5f0e64d");
    verify(0x16cba0,2864,"b3e0fea6b120660dd31a5d6bd884ef4fae5849749df94db9f87cc9c53df482a0");
    verify(0x178bf0,3776,"aa67dd1bb03f54e7c61c60e00b41caebe731c4f1fffdbec03e44ac0fafc261f1");
    verify(0x16df20,240,"9da5e6bbcb1ab64df96fab62e21f246913578dd30d5799237c07c36464b939a6");
    verify(0x167700,112,"3a32f6687e24518e5d1e920efcebcfbf2b4940073c6ffb3828a64da13e5e76dd");
}
core::Selection select_digital(uintptr_t helper,uintptr_t caller,void* source){
    const auto caller_rva=caller>=image?caller-image:0;
    if(respawn_installed&&armed.load(std::memory_order_acquire)&&respawn_query.matches(helper,caller_rva)){
        try{
            if(read<uintptr_t>(image+0x164b250)!=image+0x1c4e7a0)
                throw std::runtime_error("NPC validity RNG Default object differs");
        }catch(const std::exception& problem){
            std::lock_guard lock(failure_mutex);if(failure.empty())failure=problem.what();
            throw;
        }
        return core::select_respawn(true,generation.load(std::memory_order_acquire),respawn_query,
            helper,caller_rva,source,reinterpret_cast<void*>(image+0x1c4e7a0),respawn_local,respawn_count,
            [](void* target,uint64_t seed){original_seed(target,seed);});
    }
    if(puppet_installed&&armed.load(std::memory_order_acquire)&&puppet_scope.matches(helper,caller_rva)){
        try{
            if(read<uintptr_t>(image+0x164b250)!=image+0x1c4e7a0)
                throw std::runtime_error("Blueprint Puppet RNG Default object differs");
        }catch(const std::exception& problem){
            std::lock_guard lock(failure_mutex);if(failure.empty())failure=problem.what();
            throw;
        }
        return core::select_blueprint_puppet(true,generation.load(std::memory_order_acquire),puppet_scope,
            helper,caller_rva,source,reinterpret_cast<void*>(image+0x1c4e7a0),puppet_local,puppet_count,
            [](void* target,uint64_t seed){original_seed(target,seed);});
    }
    if(scenery_pattern_installed&&armed.load(std::memory_order_acquire)&&scenery_pattern_scope.matches(helper,caller_rva)){
        try{
            if(read<uintptr_t>(image+0x164b250)!=image+0x1c4e7a0)
                throw std::runtime_error("Scenery pattern RNG Default object differs");
        }catch(const std::exception& problem){
            std::lock_guard lock(failure_mutex);if(failure.empty())failure=problem.what();
            throw;
        }
        return core::select_scenery_pattern(true,generation.load(std::memory_order_acquire),scenery_pattern_scope,
            helper,caller_rva,source,reinterpret_cast<void*>(image+0x1c4e7a0),scenery_pattern_local,scenery_pattern_count,
            [](void* target,uint64_t seed){original_seed(target,seed);});
    }
    if(character_jump_installed&&armed.load(std::memory_order_acquire)&&core::character_jump_site(helper,caller_rva)){
        try{
            if(read<uintptr_t>(image+0x164b250)!=image+0x1c4e7a0)
                throw std::runtime_error("Character jump RNG Default object differs");
        }catch(const std::exception& problem){
            std::lock_guard lock(failure_mutex);if(failure.empty())failure=problem.what();
            throw;
        }
        return core::select_character_jump(true,generation.load(std::memory_order_acquire),helper,caller_rva,
            source,reinterpret_cast<void*>(image+0x1c4e7a0),character_jump_local,character_jump_count,
            [](void* target,uint64_t seed){original_seed(target,seed);});
    }
    const auto index=caller>=image?core::digital_site(helper,caller-image):16;
    if(!digital_enabled||!armed.load(std::memory_order_acquire)||index==16)return {source};
    auto& count=digital_counts[index];++count.matched;
    if(reinterpret_cast<uintptr_t>(source)!=image+0x1c4e7a0){++count.nondefault;return {source};}
    try{
        if(read<uintptr_t>(image+0x164b250)!=image+0x1c4e7a0)
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
    verify(0x81b210,1056,"2f69d9bf7965213a610ea9c2bf94340f83b9a3931c47d81e10aa39dfe68f9658");
    verify(0x841010,336,"5ffc0ae6d8befcb828e54b8647657e14c4685a5bfff447848527b22fb21d1ec5");
    verify(0x9fc090,192,"7a7ab329d2eb747de62d61f791723eef44f3e850a468747f41708e6d08b981e5");
}
__declspec(noinline) float get_float(void* source,float limit){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto selected=caller>=image?core::site(caller-image):core::Site::Other;
    if(!armed.load(std::memory_order_acquire)||selected==core::Site::Other)return rng_trace::invoke(0x9fc430,caller,source,original_float,limit);
    uintptr_t current=0;
    try{
        current=read<uintptr_t>(image+0x164b250);
        if(current!=image+0x1c4e7a0)throw std::runtime_error("Presentation RNG Default object differs");
    }catch(const std::exception& problem){
        {std::lock_guard lock(failure_mutex);if(failure.empty())failure=problem.what();}
        // Admission failed before any redirected native call. Preserve vanilla
        // behavior once and fail the experiment at its next held observation.
        return rng_trace::invoke(0x9fc430,caller,source,original_float,limit);
    }
    const auto epoch=generation.load(std::memory_order_acquire);
    auto& count=counts[selected==core::Site::Window?0:1];
    return core::dispatch(true,epoch,selected,source,reinterpret_cast<void*>(current),limit,local,count,
        [](void* state,uint64_t seed){original_seed(state,seed);},
        [caller](void* state,float max){return rng_trace::invoke(0x9fc430,caller,state,original_float,max);});
}
void install(const Json& config){
    digital_enabled=config.value("digital_rng",false);
    if(digital_enabled&&(!config.value("presentation_rng",false)||!config.value("rng_trace",false)))
        throw std::runtime_error("Digital RNG requires presentation_rng and rng_trace hooks");
    if(!config.value("presentation_rng",false))return;
    if(digital_enabled)verify_digital();
    verify_callers();
    verify(0x9fc430,128,"5f2e7755f953a9e5755c9af6e38b4b86c8c134223a76525845487a3187ca39e0");
    original_seed=reinterpret_cast<Seed>(image+0x9fc090);
    hook(reinterpret_cast<void*>(image+0x9fc430),reinterpret_cast<void*>(&get_float),reinterpret_cast<void**>(&original_float));
    if(config.value("rng_trace",false)){
        verify(0x24e790,208,"2f485cb1039a25e9dd20e8fd9b970b0a25dc6d2a1d0747608de50a784655bbfa");
        verify(0x5c3d80,368,"462f80355467d9fb90a4d2c9c6278fc83f7aa8645ea5a2741d1cbf79960ef614");
        verify(0x5c4210,992,"8fb0187211bf62aa9985c6e9d4de015e0f64a068c04cb107daf4d40009905bf4");
        hook(reinterpret_cast<void*>(image+0x24e790),reinterpret_cast<void*>(&find_respawn),
            reinterpret_cast<void**>(&original_find_respawn));
        respawn_installed=true;
        verify(0x34980,192,"3073d793c1e2149e07487db8e4afbdf2ceccd24c08e48360dd6e2dfc5b363c1f");
        verify(0x400830,672,"49fa0a863a565751254e2c0c4b2dc311c28e7a79d3e0d180c96b92e325d32db0");
        verify(0x3b3cd0,1536,"862fc78da9db15614bfda44651f0287421bb9e891306a2fe7dcaa4bc658e9af6");
        hook(reinterpret_cast<void*>(image+0x34980),reinterpret_cast<void*>(&puppet_constructor),
            reinterpret_cast<void**>(&original_puppet_constructor));
        puppet_installed=true;
        verify(0x5d3520,3024,"75a655abfab68c1ae9ca54429ec5ee7a6a00900f61929b9046e08bbf7134c9af");
        verify(0x18e3b0,64,"032bd5ac7bd37c80d7db821b1a364c354fb070fb7ce8a25735783a50626b3910");
        verify(0x18ecb0,64,"143c94ce419aae00a141e3f3976d94d3ca89e6fd8d6f6a5ff04828238bbd8cb6");
        verify(0x18f8f0,64,"e8df3649827ff3def597140ef869ee80f5df3e9429bc5556c42633ebfdc4d7a7");
        verify(0x9fa720,336,"50ed5d2329f66a2ea85a589635ada3fa228dab9810ae71ebb5e7a2ef3d6c7315");
        hook(reinterpret_cast<void*>(image+0x9fa720),reinterpret_cast<void*>(&perlin_constructor),
            reinterpret_cast<void**>(&original_perlin_constructor));
        scenery_pattern_installed=true;
        verify(0x235f0,384,"7a4f6af8d3a08e81f68fb4f990d997dca3f1821b276ba5fd5aefca062f642847");
        verify(0x23a30,272,"5bd9cd4ba26c721eedb3b0d61cad16f4450f63ce73488fb10d3644359d0c19e6");
        verify(0x257a0,32,"501d9889ec223b3669f63221dc9cec0f3080e55d9bc779a6b07516522220fc64");
        verify(0x257c0,16,"5b609f249ba3ad6b38453541b3877326f5688e9f5446347f4ab2a05b0db6fd14");
        verify(0x28730,16,"ab959da4e1674818e4fe45d08d20cffb350efc7cc7c22adb8f392806bbb78352");
        verify(0x28750,16,"cdad0737793e845c4cfac4b6dfbf1144119d7cd8d3fa915dbb8fe1cd93be6142");
        verify(0x3a0350,6816,"e994c1ef7cf8ff10fe463e8def99af65798238dce88481dbd3dc82838c828b79");
        character_jump_installed=true;
    }
    if(digital_enabled||respawn_installed||puppet_installed||scenery_pattern_installed||character_jump_installed)rng_trace::select_source=&select_digital;
    installed=true;
}
void arm(){
    if(!installed)return;
    if(armed.load(std::memory_order_acquire))throw std::runtime_error("Presentation RNG already armed");
    verify_callers();
    if(read<uintptr_t>(image+0x164b250)!=image+0x1c4e7a0)throw std::runtime_error("Presentation RNG arm Default object differs");
    if(digital_enabled)verify_digital();
    generation.fetch_add(1,std::memory_order_release);
    armed.store(true,std::memory_order_release);
}
void cancel(){armed.store(false,std::memory_order_release);}
void check(){
    {std::lock_guard lock(failure_mutex);if(!failure.empty())throw std::runtime_error(failure);}
    if(respawn_count.attempted.load()!=respawn_count.completed.load())
        throw std::runtime_error("NPC validity RNG call has no confirmed completion; no replay");
    if(puppet_count.attempted.load()!=puppet_count.completed.load())
        throw std::runtime_error("Blueprint Puppet RNG call has no confirmed completion; no replay");
    if(scenery_pattern_count.attempted.load()!=scenery_pattern_count.completed.load())
        throw std::runtime_error("Scenery pattern RNG call has no confirmed completion; no replay");
    if(character_jump_count.attempted.load()!=character_jump_count.completed.load())
        throw std::runtime_error("Character jump RNG call has no confirmed completion; no replay");
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
    rows.push_back({{"site","npcValidityRespawn"},{"matched",respawn_count.matched.load()},
        {"nondefault",respawn_count.nondefault.load()},{"initialized",respawn_count.initialized.load()},
        {"attempted",respawn_count.attempted.load()},{"completed",respawn_count.completed.load()}});
    rows.push_back({{"site","blueprintPuppet"},{"matched",puppet_count.matched.load()},
        {"nondefault",puppet_count.nondefault.load()},{"initialized",puppet_count.initialized.load()},
        {"attempted",puppet_count.attempted.load()},{"completed",puppet_count.completed.load()}});
    rows.push_back({{"site","sceneryPattern"},{"matched",scenery_pattern_count.matched.load()},
        {"nondefault",scenery_pattern_count.nondefault.load()},{"initialized",scenery_pattern_count.initialized.load()},
        {"attempted",scenery_pattern_count.attempted.load()},{"completed",scenery_pattern_count.completed.load()}});
    rows.push_back({{"site","characterJump"},{"matched",character_jump_count.matched.load()},
        {"nondefault",character_jump_count.nondefault.load()},{"initialized",character_jump_count.initialized.load()},
        {"attempted",character_jump_count.attempted.load()},{"completed",character_jump_count.completed.load()}});
    std::string cause;{std::lock_guard lock(failure_mutex);cause=failure;}
    return {{"installed",installed},{"armed",armed.load()},{"generation",generation.load()},{"error",cause},
        {"digital_enabled",digital_enabled},{"respawn_query_installed",respawn_installed},{"blueprint_puppet_installed",puppet_installed},{"scenery_pattern_installed",scenery_pattern_installed},{"character_jump_installed",character_jump_installed},{"scope","two exact Character jump animation calls; exact Scenery pattern Perlin constructor; exact Blueprint Puppet constructor; exact NPC validity respawn query; two shader constructors; optional sixteen exact visual line/dot/block/camera-target calls in eight functions"},{"sites",rows}};
}
}
