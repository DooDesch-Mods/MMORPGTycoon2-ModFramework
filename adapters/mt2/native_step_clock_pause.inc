// Included after the guarded current Mode/Clock accessors.
namespace clock_pause {
using Set=void(*)(void*,float);
Set original=nullptr;
uint64_t attempted=0,returned=0,local_pause_overrides=0,last_tick=0,last_invocation=0;
uint64_t last_lease=0,last_session=0,last_epoch=0;
uint32_t last_selected=0;
float last_incoming=1.0f;
bool installed=false;
void set_call(void* receiver,float incoming,uintptr_t caller){
    if(caller!=image+0x4a1c7d||!control.entered||control.phase!=Control::Phase::Stepping){
        original(receiver,incoming);return;
    }
    require(thread&&thread==GetCurrentThreadId()&&timer_step::frame.active,
        "Ordered Clock pause requires its active original Step/thread");
    const auto receipt=native_steps->snapshot();
    require(receipt.phase==MT2FW_MT2_STEP_ENTERED&&receipt.lease&&receipt.session&&receipt.world_epoch&&
        receipt.tick&&receipt.invocation&&receipt.thread==thread&&receipt.dt_bits==MT2FW_MT2_STEP_FIXED_DT_BITS,
        "Ordered Clock pause lacks its admitted native Step receipt");
    require(receipt.lease!=last_lease||receipt.session!=last_session||receipt.world_epoch!=last_epoch||
        receipt.tick!=last_tick||receipt.invocation!=last_invocation,"Ordered Clock pause repeated within one admitted Step");
    // This accessor retains the independent SavesInProgress == 0 guard.
    const auto mode=snapshot_prepare::mode();
    const auto clock=read<uintptr_t>(image+0x13df9a0),state=read<uintptr_t>(mode+0x58);
    require(clock&&clock==reinterpret_cast<uintptr_t>(receiver)&&read<uintptr_t>(clock)==image+0x17499d0&&
        state&&read<uintptr_t>(state+0x158)==clock,"Ordered Clock pause current owner differs");
    require(incoming==0.0f||incoming==1.0f,"Ordered Clock local pause argument differs from native boolean scale");
    last_selected=read<uint32_t>(clock+0x58);last_incoming=incoming;
    last_lease=receipt.lease;last_session=receipt.session;last_epoch=receipt.world_epoch;
    last_tick=receipt.tick;last_invocation=receipt.invocation;++attempted;
    // Local menu modality must not suspend one peer's admitted simulation tick.
    // User Pause/speed remains the separate selected scale at Clock +0x58.
    original(receiver,1.0f);++returned;
    require(snapshot_prepare::mode()==mode&&read<uintptr_t>(image+0x13df9a0)==clock&&
        read<uintptr_t>(state+0x158)==clock,"Ordered Clock pause owner changed during original setter");
    require(read<float>(clock+0x5c)==1.0f,"Ordered Clock pause original setter readback differs");
    require(read<uint32_t>(clock+0x58)==last_selected,"Ordered Clock pause changed selected user speed");
    if(incoming==0.0f)++local_pause_overrides;
}
__declspec(noinline) void set(void* receiver,float incoming){
    set_call(receiver,incoming,reinterpret_cast<uintptr_t>(_ReturnAddress()));
}
Json status(){return {{"attempted",attempted},{"returned",returned},{"local_pause_overrides",local_pause_overrides},
    {"last_tick",last_tick},{"last_invocation",last_invocation},{"last_selected_bits",last_selected},{"last_incoming",last_incoming}};}
void install(){
    if(installed)return;
    for(const auto& [rva,size,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,2>{{
        {0x4a1bb0,5088,"933bbb87425a467d54e25ba1290fd55c0232e72a68d07c2039d7822f8d691b89"},
        {0x259b70,176,"e04b7a6083ecc1404c416b50fd098114afd5a6f69d270f8190f0f24cda619efd"}
    }}){std::string cause;const bool valid=native_routes->verify_body(rva,size,sha,cause);
        require(valid,"Ordered Clock pause native body differs RVA="+std::to_string(rva)+": "+cause);}
    const char* key="framework.step.clock-pause";
    native_routes->catalog(key,0x259b70,true);native_services->reserve(key);
    auto* target=reinterpret_cast<void*>(image+0x259b70);auto* detour=reinterpret_cast<void*>(&set);
    hook(target,detour,reinterpret_cast<void**>(&original));const auto enabled=MH_EnableHook(target);
    require(enabled==MH_OK,std::string("Ordered Clock pause hook enable failed: ")+MH_StatusToString(enabled));
    std::string cause;const bool published=native_routes->publish_framework_hook(target,detour,reinterpret_cast<void*>(original),cause);
    require(published,"Ordered Clock pause hook publication failed: "+cause);installed=true;
}
}
