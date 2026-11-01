// Original FinishAllTasks owns callback execution and object deletion. Retain
// only its returned scratch-array values, never the lifetime of those objects.
namespace completed_callbacks {
struct Receipt {
    uintptr_t game=0,mode=0,tasks=0,allocation=0;
    uint64_t generation=0,world_epoch=0;
    uint32_t thread=0;
    int32_t count=0,capacity=0;
    std::vector<uintptr_t> values;
} receipt;
void clear(){receipt={};}
void record(uintptr_t tasks){
    clear();
    // Loading can retain live ToonPlans and has no bound world receipt yet.
    // Its existing bootstrap task proof owns those callbacks until Step one.
    if(control.phase==Control::Phase::Loading)return;
    require(lifecycle.generation&&bootstrap.world_epoch&&thread==GetCurrentThreadId()&&
        ((control.phase==Control::Phase::Stepping&&control.entered&&timer_step::frame.active)||recovery_draining())&&
        !uncertain&&first_error.empty()&&finish_entered==finish_returned&&
        !ordered_run::open&&!ordered_run::masked&&!ordered_run::running&&!ordered_run::transfer_uncertain,
        "Completed callbacks lack a returned known native drain generation="+std::to_string(lifecycle.generation)+
        " world_epoch="+std::to_string(bootstrap.world_epoch)+" pending="+std::to_string(pending));
    require(tasks&&tasks==read<uintptr_t>(owner+0x68)&&tasks==read<uintptr_t>(image+0x13df340)&&
        read<uintptr_t>(tasks)==image+0x16fdaf0,"Completed callback Task owner differs");
    Receipt next;
    next.game=owner;next.mode=read<uintptr_t>(owner+0x28);next.tasks=tasks;
    next.generation=lifecycle.generation;next.world_epoch=bootstrap.world_epoch;next.thread=thread;
    next.allocation=read<uintptr_t>(tasks+0xd0);next.count=read<int32_t>(tasks+0xd8);next.capacity=read<int32_t>(tasks+0xdc);
    require(next.count>=0&&next.capacity>=next.count&&next.capacity<=1000000&&(!next.count||next.allocation),
        "Completed callback snapshot bounds differ count="+std::to_string(next.count)+
        " capacity="+std::to_string(next.capacity)+" allocation="+std::to_string(next.allocation)+
        " world_epoch="+std::to_string(next.world_epoch)+" pending="+std::to_string(pending));
    next.values.reserve(size_t(next.count));
    for(int32_t i=0;i<next.count;++i)next.values.push_back(read<uintptr_t>(next.allocation+size_t(i)*8));
    receipt=std::move(next);
}
bool matches(uintptr_t tasks){
    if(!receipt.tasks||receipt.tasks!=tasks||receipt.game!=owner||receipt.mode!=read<uintptr_t>(owner+0x28)||
        receipt.generation!=lifecycle.generation||receipt.world_epoch!=bootstrap.world_epoch||
        receipt.thread!=thread||thread!=GetCurrentThreadId()||uncertain||!first_error.empty()||
        ordered_run::open||ordered_run::masked||ordered_run::running||ordered_run::transfer_uncertain||
        read<uintptr_t>(tasks+0xd0)!=receipt.allocation||read<int32_t>(tasks+0xd8)!=receipt.count||
        read<int32_t>(tasks+0xdc)!=receipt.capacity)return false;
    for(size_t i=0;i<receipt.values.size();++i)
        if(read<uintptr_t>(receipt.allocation+i*8)!=receipt.values[i])return false;
    return true;
}
}
