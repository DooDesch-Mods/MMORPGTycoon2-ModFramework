// Map::Update must emit its original load-completion event when the old
// exact fixed-step timer equals five seconds. Native variable dt rarely lands
// on that value, but 1/64 does; its strict comparison otherwise skips the event.
namespace reveal {
constexpr uintptr_t Branch=0x32f91f;
bool changed=false,uncertain=false;
void branch_is(uint8_t opcode){
    require(read<uint8_t>(image+Branch)==opcode&&read<uint8_t>(image+Branch+1)==0x4d,"Reveal completion branch differs");
}
void change(uint8_t before,uint8_t after){
    branch_is(before);uncertain=true;
    auto* code=reinterpret_cast<void*>(image+Branch);DWORD protection=0,discarded=0;
    if(!VirtualProtect(code,1,PAGE_EXECUTE_READWRITE,&protection))
        throw std::runtime_error("Reveal branch protection failed win32="+std::to_string(GetLastError()));
    std::memcpy(code,&after,1);
    const bool restored=VirtualProtect(code,1,protection,&discarded)!=0;
    const auto protection_error=restored?0:GetLastError();
    const bool flushed=FlushInstructionCache(GetCurrentProcess(),code,2)!=0;
    const auto flush_error=flushed?0:GetLastError();
    if(!restored||!flushed)throw std::runtime_error("Reveal branch publication failed protection="+std::to_string(protection_error)+" cache="+std::to_string(flush_error));
    branch_is(after);uncertain=false;
}
void held_owner(){
    require(thread==GetCurrentThreadId()&&held_tasks&&read<uintptr_t>(image+0x13df340)==held_tasks&&
        read<uintptr_t>(owner+0x68)==held_tasks&&read<uint8_t>(held_tasks+0xf8)==1&&
        read<int32_t>(held_tasks+0xa8)==0&&read<int32_t>(held_tasks+0x90)==0,"Reveal branch requires the held game-thread Task owner");
}
void arm(){
    require(!changed&&!uncertain,"Reveal branch already changed or uncertain");held_owner();
    change(0x72,0x76);changed=true;
}
void restore(){
    require(!uncertain,"Reveal branch cannot restore uncertain publication");
    if(!changed)return;
    require(!control.entered&&!timer_step::frame.active,"Reveal branch restore entered an original frame");held_owner();
    change(0x76,0x72);changed=false;
}
}
