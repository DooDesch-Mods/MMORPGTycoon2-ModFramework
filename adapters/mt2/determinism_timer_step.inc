// Included inside the private lab namespace after read<T> and write<T>.
namespace timer_step {
using Update=void(*)(void*,float);
Update original_update=nullptr;
struct Frame {
    uintptr_t game=0;
    uint32_t fixed=0,passive=0,last_measured=0,last_applied=0;
    uint64_t entered=0,returned=0,updates_entered=0,updates_returned=0,applied=0,restored=0;
    unsigned calls=0;
    bool active=false,passive_pending=false;
};
thread_local Frame frame;
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
void current_game(){
    require(frame.game&&read<uintptr_t>(image+0x13df8f0)==frame.game,"Fixed timestep current Game owner differs");
    require(read<uintptr_t>(frame.game)==image+0x1743740,"Fixed timestep Game address point differs");
}
void timer_owner(uintptr_t receiver){
    current_game();require(receiver&&read<uintptr_t>(image+0x1c4eb70)==receiver,"Fixed timestep Timer singleton differs");
    require(read<uintptr_t>(receiver)==image+0x16f9b80&&read<uintptr_t>(image+0x16f9ba0)==image+0xa36300,"Fixed timestep Timer type/dispatch differs");
    require(read<uint8_t>(receiver+8)==1,"Fixed timestep Timer system is not active");
    const auto data=read<uintptr_t>(frame.game+8);const auto count=read<int32_t>(frame.game+0x10);
    require(count>0&&count<=64&&data,"Fixed timestep active system array bounds differ");
    unsigned found=0;for(int32_t i=0;i<count;++i)found+=read<uintptr_t>(data+size_t(i)*8)==receiver;
    require(found==1,"Fixed timestep Timer lacks unique current system membership");
}
void begin(uintptr_t game,uint32_t fixed){
    require(!frame.active,"Fixed timestep outer frame reentered");
    require(!frame.passive_pending||frame.game==game,"Fixed timestep owner changed before passive restoration");
    frame.game=game;current_game();
    if(!frame.passive_pending){frame.passive=read<uint32_t>(game+0x1c);frame.passive_pending=true;}
    frame.fixed=fixed;frame.calls=0;frame.active=true;++frame.entered;
}
// Caller is captured in the hook itself, before any helper or original call.
void update_call(void* receiver,float incoming,uintptr_t caller){
    if(!frame.active){original_update(receiver,incoming);return;}
    ++frame.calls;++frame.updates_entered;
    original_update(receiver,incoming);
    ++frame.updates_returned;
    require(caller==image+0x95675f,"Fixed timestep Timer was not called by the admitted coreGame::Go system loop");
    require(frame.calls==1,"Fixed timestep outer frame called Timer more than once");
    timer_owner(reinterpret_cast<uintptr_t>(receiver));
    frame.last_measured=read<uint32_t>(frame.game+0x1c);
    // Timer statistics and wall-clock measurement remain original. Only the
    // central step consumed by the remaining original frame is fixed here.
    write(frame.game+0x1c,frame.fixed);
    frame.last_applied=read<uint32_t>(frame.game+0x1c);
    require(frame.last_applied==frame.fixed,"Fixed timestep Timer return readback differs");++frame.applied;
}
__declspec(noinline) void update(void* receiver,float incoming){update_call(receiver,incoming,reinterpret_cast<uintptr_t>(_ReturnAddress()));}
void finish(){
    require(frame.active&&frame.calls==1,"Fixed timestep outer frame did not execute one Timer update");current_game();
    require(read<uint32_t>(frame.game+0x1c)==frame.fixed,"Fixed timestep changed after Timer return");
    frame.active=false;++frame.returned;
}
void abandon(){frame.active=false;}
void restore_held(){
    require(!frame.active&&frame.passive_pending,"Fixed timestep passive restoration lacks a completed outer boundary");current_game();
    require(held_tasks&&read<uintptr_t>(image+0x13df340)==held_tasks&&read<uintptr_t>(frame.game+0x68)==held_tasks,"Fixed timestep held Task owner differs");
    require(read<uint8_t>(held_tasks+0xf8)==1&&read<int32_t>(held_tasks+0xa8)==0&&read<int32_t>(held_tasks+0x90)==0,"Fixed timestep passive restoration requires paused/drained workers");
    write(frame.game+0x1c,frame.passive);frame.passive_pending=false;++frame.restored;
}
Json status(){return {{"outer_entered",frame.entered},{"outer_returned",frame.returned},{"timer_entered",frame.updates_entered},
    {"timer_returned",frame.updates_returned},{"fixed_applied",frame.applied},{"passive_restored",frame.restored},
    {"requested_bits",frame.fixed},{"last_measured_bits",frame.last_measured},{"last_applied_bits",frame.last_applied},
    {"active",frame.active},{"passive_restore_pending",frame.passive_pending}};}
void install(){
    std::string body(0x340,'\0');SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+0xa36300),body.data(),body.size(),&copied)||copied!=body.size())
        throw std::runtime_error("Fixed timestep Timer body read failed");
    require(digest(body)=="2442e3fc5884b09357a378bdeed30fa487466e4d69c36c864254857dc030c334","Fixed timestep exact Timer Update body differs");
    hook(reinterpret_cast<void*>(image+0xa36300),reinterpret_cast<void*>(&update),reinterpret_cast<void**>(&original_update));
}
}
