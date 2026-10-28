// Included inside lab. Only the explicit private experiment marker installs it.
void isolated();
namespace building_receipt {
using Core=mt2::determinism::BuildingReceipt;
using Load=void(*)(void*,void*,void*);
Load original_load=nullptr;
std::unique_ptr<Core> observer;
bool installed=false;
void load(void* array,void* reader,void* context){
    observer->invoke(reinterpret_cast<uintptr_t>(array),reinterpret_cast<uintptr_t>(reader),reinterpret_cast<uintptr_t>(context),
        GetCurrentThreadId(),fixture_opened.load(std::memory_order_acquire),[&]{original_load(array,reader,context);});
}
void install(const Json& config){
    if(!config.value("saved_building_identity",false))return;
    isolated();
    std::string body(0x460,'\0');SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+0xc0ab50),body.data(),body.size(),&copied)||copied!=body.size())
        throw std::runtime_error("Building receipt LoadFromStream body read failed");
    if(digest(body)!="344763380e642478d438df35309efae03225982400882e631b5ba96fe71f5ba9")
        throw std::runtime_error("Building receipt exact LoadFromStream body differs");
    observer=std::make_unique<Core>([](uintptr_t address,size_t size){
        Core::Bytes bytes(size);SIZE_T count=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),bytes.data(),size,&count)||count!=size)
            throw std::runtime_error("Building receipt read failed address="+std::to_string(address)+" required="+std::to_string(size)+" actual="+std::to_string(count));
        return bytes;
    },image);
    std::string cause;
    if(!native_routes||!native_routes->reserve_building_observer(*runtime,reinterpret_cast<void*>(&load),cause))
        throw std::runtime_error("Building receipt exclusive reservation failed: "+cause);
    // prepare() calls this before loading a DLL, Lua entry or content. Keep the
    // reservation and this gate for the process lifetime, including cancellation.
    auto previous_notice=runtime->mod_notice;
    runtime->mod_notice=[previous_notice](const auto& manifest,const char* state){
        if(std::string_view(state)=="Loading")throw std::runtime_error("Plugin loading is blocked by the private saved Building observer");
        if(previous_notice)previous_notice(manifest,state);
    };
    hook(reinterpret_cast<void*>(image+0xc0ab50),reinterpret_cast<void*>(&load),reinterpret_cast<void**>(&original_load));installed=true;
}
void arm(){if(installed)observer->arm("user/"+fixture_path,fixture_sha);}
void validate_pin(const std::string& sha){if(installed&&!Core::supported_fixture(sha))throw std::runtime_error("Building identity marker requires one of the two pinned Building fixture SHA256 values");}
void cancel(){if(installed)observer->cancel();}
Core::Keys snapshot(){return installed?observer->snapshot():Core::Keys{};}
}
