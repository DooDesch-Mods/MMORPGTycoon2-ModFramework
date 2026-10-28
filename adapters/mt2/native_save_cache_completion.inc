// Native Save queues a local save-list scan after its writer has completed.
namespace save_cache {
constexpr uintptr_t AddressPoint=0x1789180,GlobalTask=0x1a3c048;
struct Context {uintptr_t tasks=0,task=0,snapshot=0;unsigned pumps=0;};
thread_local Context* current=nullptr;
uint64_t attempts=0,returns=0;
struct ExportOwner {uintptr_t mode=0,tasks=0;uint32_t thread=0;};
thread_local ExportOwner* export_owner=nullptr;
struct ExportScope {
    ExportOwner owner;
    ExportScope(uintptr_t mode,uintptr_t tasks,uint32_t thread):owner{mode,tasks,thread}{
        require(!export_owner&&!current&&!mt2::PreparedSaveScope::current&&!mt2::BlockingSaveScope::current,
            "Client Save cache export scope reentered");export_owner=&owner;
    }
    ~ExportScope(){export_owner=nullptr;}
};
void owner_check(uintptr_t tasks){
    const auto* scope=mt2::PreparedSaveScope::current;
    const auto* blocking=mt2::BlockingSaveScope::current;
    const bool captured=scope&&blocking&&!blocking->previous&&scope->mode==blocking->mode&&scope->thread==thread&&blocking->thread==thread&&
        scope->tasks==tasks&&scope->saves==1&&scope->flushes==1&&scope->resumes==1;
    const bool exported=export_owner&&!scope&&!blocking&&export_owner->thread==thread&&export_owner->tasks==tasks;
    require(thread==GetCurrentThreadId()&&tasks==held_tasks&&(captured||exported),
        "Save cache lacks completed capture or explicit Client publication ownership");
    const auto active_mode=exported?export_owner->mode:scope->mode;
    require(!control.entered&&!timer_step::frame.active&&control.phase==mt2::determinism::Gate::Phase::Held,
        "Save cache settlement escaped held capture");
    timer_step::current_game();
    require(read<uintptr_t>(owner+0x28)==active_mode&&read<uintptr_t>(active_mode)==image+0x16e6730&&
        read<int32_t>(active_mode+0x30c)==0&&tasks==read<uintptr_t>(owner+0x68)&&tasks==read<uintptr_t>(image+0x13df340)&&
        read<uintptr_t>(tasks)==image+0x16fdaf0&&!pending&&!uncertain,"Save cache native owner or pending gameplay differs");
}
void header(uintptr_t task,bool finished){
    require(task&&read<uintptr_t>(task)==image+AddressPoint&&read<int32_t>(task+8)==2&&read<uint8_t>(task+0xc)==0&&
        read<uint8_t>(task+0xd)==(finished?1:0)&&read<uint8_t>(task+0xe)==0&&
        read<uintptr_t>(image+AddressPoint+0x10)==image+0x7a2780&&read<uintptr_t>(image+AddressPoint+0x18)==image+0xc367a0,
        "Save cache task identity/type/lifecycle differs");
}
void queues(uintptr_t tasks,uintptr_t task,size_t admitted){
    for(size_t offset:{size_t(0x28),size_t(0x40),size_t(0x58),size_t(0x70),size_t(0x88),size_t(0xa0),size_t(0xb8)}){
        const auto count=read<int32_t>(tasks+offset+8),capacity=read<int32_t>(tasks+offset+12);
        require(count==(offset==admitted?1:0)&&capacity>=count&&capacity<=1000000,
            "Save cache contains foreign or missing live work offset="+std::to_string(offset)+" actual="+std::to_string(count));
        if(count){const auto data=read<uintptr_t>(tasks+offset);require(data&&read<uintptr_t>(data)==task,"Save cache native queue lost exact task");}
    }
}
void before_unlock(void* mutex){
    require(current,"Save cache callback has no current settlement");auto& value=*current;owner_check(value.tasks);
    require(reinterpret_cast<uintptr_t>(mutex)==value.tasks+0xf0&&read<uint8_t>(value.tasks+0xf8)==0,
        "Save cache callback mutex or scheduler differs");
    require(value.pumps==0,"Save cache callback pump repeated");queues(value.tasks,value.task,0);
    header(value.task,true);require(read<uintptr_t>(image+GlobalTask)==0,"Save cache Run did not release native cache owner");
    const auto data=read<uintptr_t>(value.tasks+0xd0);
    require(data&&read<int32_t>(value.tasks+0xd8)==1&&read<int32_t>(value.tasks+0xdc)==1&&read<uintptr_t>(data)==value.task,
        "Save cache callback snapshot differs");
    value.snapshot=data;++value.pumps;
}
void settle(Finish resume,Finish pause,Finish finish){
    require(!current&&resume&&pause&&finish,"Save cache settlement reentered or native ports absent");
    const auto tasks=held_tasks;owner_check(tasks);
    require(path_lock&&original_unlock&&read<uint8_t>(tasks+0xf8)==1,"Save cache requires held scheduler and native mutex");
    Context value{tasks};
    path_lock(reinterpret_cast<void*>(tasks+0xf0));
    try{
        value.task=read<uintptr_t>(image+GlobalTask);header(value.task,false);queues(tasks,value.task,0x58);
        if(!export_owner)require(save_completion::consumed_snapshot(tasks),"Save cache lacks consumed original writer receipt");
    }catch(...){original_unlock(reinterpret_cast<void*>(tasks+0xf0));throw;}
    original_unlock(reinterpret_cast<void*>(tasks+0xf0));
    current=&value;struct Clear {~Clear(){current=nullptr;}} clear;++attempts;
    try{
        resume(reinterpret_cast<void*>(tasks));
        require(read<uint8_t>(tasks+0xf8)==0,"Save cache original Resume was not acknowledged");
        finish(reinterpret_cast<void*>(tasks));
        pause(reinterpret_cast<void*>(tasks));
    }catch(const std::exception& error){
        const std::string cause=error.what();
        try{pause(reinterpret_cast<void*>(tasks));}
        catch(const std::exception& recovery){throw std::runtime_error(cause+"; Save cache hold restoration failed: "+recovery.what());}
        catch(...){throw std::runtime_error(cause+"; Save cache hold restoration raised unknown exception");}
        throw;
    }catch(...){
        try{pause(reinterpret_cast<void*>(tasks));}
        catch(const std::exception& recovery){throw std::runtime_error(std::string("Save cache native settlement raised unknown exception; hold restoration failed: ")+recovery.what());}
        catch(...){throw std::runtime_error("Save cache native settlement and hold restoration raised unknown exceptions");}
        throw;
    }
    owner_check(tasks);require(read<uint8_t>(tasks+0xf8)==1&&value.pumps==1&&value.snapshot,"Save cache native completion/hold receipt missing");
    queues(tasks,value.task,0);require(read<uintptr_t>(image+GlobalTask)==0,"Save cache native owner returned after completion");
    // Original Pump marks completion and deletes the orphan. Only retained
    // pointer values may be read after it returns.
    save_completion::consumed={owner,export_owner?export_owner->mode:mt2::PreparedSaveScope::current->mode,tasks,value.snapshot,value.task,thread};
    require(save_completion::consumed_snapshot(tasks),"Save cache consumed callback allocation changed");++returns;
}
}
