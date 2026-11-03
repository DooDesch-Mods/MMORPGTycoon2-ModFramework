namespace polling_delay {
using Delay=void(SDLCALL*)(Uint32);
Delay original=nullptr;
uintptr_t resolved=0;
bool installed=false;
constexpr const char* key="framework.step.finish-poll-delay";
__declspec(noinline) void SDLCALL entry(Uint32 milliseconds);
void proof(){
    std::string cause;
    require(native_routes->prove_framework_hook(key,reinterpret_cast<void*>(&entry),reinterpret_cast<void*>(original),cause),"Ordered polling route changed: "+cause);
    require(read<uintptr_t>(image+0x1c52028)==resolved,"Ordered polling SDL_Delay IAT identity changed");
}
__declspec(noinline) void SDLCALL entry(Uint32 milliseconds){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    mt2::poll_delay::invoke(milliseconds,caller,GetCurrentThreadId(),image,[](uint32_t selected){original(selected);});
}
Json status(){
    auto counters=[](const mt2::poll_delay::Counters& c){return Json{{"entered",c.entered.load()},{"returned",c.returned.load()},
        {"failed",c.failed.load()},{"requested_ms",c.requested_ms.load()},{"selected_ms",c.selected_ms.load()},{"elapsed_ns",c.elapsed_ns.load()}};};
    return {{"installed",installed},{"selected",counters(mt2::poll_delay::selected)},{"forwarded",counters(mt2::poll_delay::forwarded)},
        {"first_failure",mt2::poll_delay::failure_ready.load(std::memory_order_acquire)?mt2::poll_delay::first_failure:""},
        {"scope","process-lifetime exact EXE thunk calls; elapsed wall time, not requested sleep or CPU; no profiler epoch reset"}};
}
void install(){
    if(installed)return;
    for(const auto& [rva,size,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,2>{{
        {0x526d50,466,"37568995d5b181f89f60bd9c0bc43b04f3336bb19f2164edafc945489dac34fa"},
        {0xa7f528,6,"0b7794983a054d6f460d751a2aaeb1907af6a6496e2f6283eae932cd5c9118bf"}}}){
        std::string cause;require(native_routes->verify_body(rva,size,sha,cause),"Ordered polling native bytes differ RVA="+std::to_string(rva)+": "+cause);
    }
    auto module=GetModuleHandleW(L"SDL3.dll");require(module!=nullptr,"Ordered polling SDL3 module is absent");
    resolved=reinterpret_cast<uintptr_t>(GetProcAddress(module,"SDL_Delay"));
    require(resolved&&read<uintptr_t>(image+0x1c52028)==resolved,"Ordered polling SDL_Delay import/export identity differs");
    HMODULE pinned=nullptr;
    require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(resolved),&pinned)&&pinned==module,
        "Ordered polling SDL_Delay provider module pin differs");
    native_routes->catalog(key,0xa7f528,true);native_services->reserve(key);
    auto* target=reinterpret_cast<void*>(image+0xa7f528);auto* detour=reinterpret_cast<void*>(&entry);
    hook(target,detour,reinterpret_cast<void**>(&original));const auto enabled=MH_EnableHook(target);
    require(enabled==MH_OK,std::string("Ordered polling hook enable failed: ")+MH_StatusToString(enabled));
    std::string cause;require(native_routes->publish_framework_hook(target,detour,reinterpret_cast<void*>(original),cause),"Ordered polling hook publication failed: "+cause);
    installed=true;proof();
}
}
