// Included after the existing Step Quit path and before driver configuration.
namespace recovery_runtime {
MT2FW_MT2_RecoveryReceipt certificate{sizeof(certificate)};
bool attempted=false,prepared=false,capture_consumed=false,capture_completed=false;
std::function<void(void*)> native_finish;
uint64_t go_entered=0,go_returned=0,timer_entered=0,timer_returned=0;
void exact(const MT2FW_MT2_RecoveryReceipt& value){
    require(attempted&&value.lease==certificate.lease&&value.session==certificate.session&&
        value.world_epoch==certificate.world_epoch&&value.generation==certificate.generation&&value.interruption==certificate.interruption&&
        value.completed_tick==certificate.completed_tick&&value.next_tick==certificate.next_tick&&
        value.local_process==certificate.local_process&&value.original_mask==certificate.original_mask&&value.survivor_mask==certificate.survivor_mask&&
        std::memcmp(&value.original,&certificate.original,sizeof(value.original))==0&&
        std::memcmp(&value.survivors,&certificate.survivors,sizeof(value.survivors))==0&&
        std::memcmp(value.recovery_nonce,certificate.recovery_nonce,37)==0,
        "Recovery native attempt differs from its consumed process/hold certificate");
}
void no_go(){
    require(timer_step::frame.entered==go_entered&&timer_step::frame.returned==go_returned&&
        timer_step::frame.updates_entered==timer_entered&&timer_step::frame.updates_returned==timer_returned&&
        control.completed==certificate.completed_tick&&!control.entered&&!timer_step::frame.active,
        "Recovery native preparation/capture executed Go/Timer or changed the completed tick");
}
void callback_ownership(){
    require(!loading_dispatch::uncertain&&loading_dispatch::first_error.empty()&&
        loading_dispatch::finish_entered==loading_dispatch::finish_returned&&
        !loading_dispatch::ordered_run::open&&!loading_dispatch::ordered_run::masked&&
        !loading_dispatch::ordered_run::running&&!loading_dispatch::ordered_run::transfer_uncertain,
        "Recovery callback ownership has failed, active or uncertain native work");
}
void initial_tasks(){
    // Loading deliberately retains ToonPlan callbacks for the first admitted
    // Step. Their native ownership is part of the bootstrap certificate.
    held();
    require(loading_dispatch::enabled&&!loading_dispatch::uncertain&&loading_dispatch::first_error.empty(),
        "Recovery initial task hold lacks healthy loading dispatch ownership");
    for(size_t offset:{size_t(0x28),size_t(0x40),size_t(0x58),size_t(0x70),size_t(0x88),size_t(0xa0),size_t(0xd0)}){
        const auto count=read<int32_t>(held_tasks+offset+8),capacity=read<int32_t>(held_tasks+offset+12);
        require(count==0&&capacity>=0&&capacity<=1000000,
            "Recovery initial task hold has unrelated queue work offset="+std::to_string(offset)+" count="+std::to_string(count));
    }
    const auto count=read<int32_t>(held_tasks+0xc0),capacity=read<int32_t>(held_tasks+0xc4);
    const auto data=read<uintptr_t>(held_tasks+0xb8);
    require(count==loading_dispatch::pending&&count>=0&&capacity>=count&&capacity<=1000000&&(!count||data),
        "Recovery initial retained plan queue bounds/count differ");
    std::set<uintptr_t> unique;
    for(int32_t i=0;i<count;++i){const auto task=read<uintptr_t>(data+size_t(i)*8);
        require(loading_dispatch::plan(task)&&unique.insert(task).second,
            "Recovery initial retained queue contains other/duplicate callbacks");
    }
}
#include "native_step_recovery_watch.inc"
void local(const MT2FW_MT2_RecoverySpec& spec,const MT2FW_MT2_StepReceipt& step,LocalProof purpose){
    require(driver&&driver->frozen()&&!driver->failed()&&failure.empty()&&outer_entered&&thread==GetCurrentThreadId()&&
        bootstrap.game==owner&&step.world_epoch==bootstrap.world_epoch&&step.generation==lifecycle.generation&&
        control.phase==Control::Phase::Held&&!control.entered&&!timer_step::frame.active&&!quit_attempted&&!exit_requested,
        "Recovery local native world is failed, entered, retiring or outside its actual outer hold");
    held();callback_ownership();presentation_rng::check();
    require(step.completed_tick==control.completed&&read<uintptr_t>(owner+0x28)==bootstrap.mode&&
        !mt2::PreparedSaveScope::current&&!mt2::BlockingSaveScope::current&&
        !loading_dispatch::uncertain&&!bootstrap_boundary::uncertain&&!bootstrap_boundary::reveal::uncertain,
        "Recovery local native owner has an active or unknown Save/callback/step");
    bool initial=false;
    if(bootstrap_boundary::pending||bootstrap_boundary::branch_changed){
        // The initial load-only hold keeps its branch until the first admitted
        // Step. Validate that exact boundary without releasing or restoring it.
        require(!attempted&&step.completed_tick==0&&bootstrap_boundary::enabled&&
            bootstrap_boundary::pending&&bootstrap_boundary::branch_changed&&
            !bootstrap_boundary::reveal::changed&&bootstrap_boundary::first_error.empty()&&
            bootstrap_boundary::render_captured&&bootstrap_boundary::render_returned&&!bootstrap_boundary::render_restored,
            "Recovery local bootstrap is not the exact initial load-only hold");
        bootstrap_boundary::require_loaded();initial=true;
    }
    if(purpose!=LocalProof::Adoption){healthy_watch(spec,step,purpose,initial);return;}
    adoption_ready();
    if(capture_consumed&&async_save::retained){
        const auto& held=async_save::held_owner();
        require(held.certificate.lease==certificate.lease&&std::memcmp(held.certificate.nonce,certificate.recovery_nonce,37)==0,
            "Recovery writer is not owned by its exact consumed certificate");
    }else{
        require(!async_save::retained&&!async_save::published.load(std::memory_order_acquire)&&read<int32_t>(bootstrap.mode+0x30c)==0,
            "Recovery local hold has an unknown native Save writer");
        // Pending native work at a healthy hold belongs to the next Step.
        // Only an actual recovery preparation establishes a Save-ready queue.
        if(initial)initial_tasks();else if(attempted)snapshot_prepare::empty_tasks();
    }
    if(attempted)no_go();
}
void prepare(MT2FW_MT2_RecoveryReceipt& value){
    require(!attempted&&value.prepare_attempted==1&&value.local_slot==1&&
        value.phase==MT2FW_MT2_RECOVERY_PREPARING&&value.completed_tick==control.completed,
        "Recovery preparation requires one unused original Host hold");
    require(!bootstrap_boundary::pending&&!bootstrap_boundary::branch_changed&&
        !bootstrap_boundary::uncertain&&!bootstrap_boundary::reveal::uncertain,
        "Recovery preparation requires the completed first-step bootstrap transition");
    held();callback_ownership();
    require(!loading_dispatch::pending,"Recovery preparation has deferred loading callbacks");
    async_save::retire();
    const auto active=snapshot_prepare::mode(),state=read<uintptr_t>(active+0x58),clock=read<uintptr_t>(image+0x13df9a0);
    require(clock&&read<uintptr_t>(clock)==image+0x17499d0&&read<uintptr_t>(state+0x158)==clock,
        "Recovery selected Clock native owner differs");
    const auto scheduler=held_tasks;held();
    value.selected_clock_bits=read<uint32_t>(clock+0x58);
    certificate=value;attempted=true;
    go_entered=timer_step::frame.entered;go_returned=timer_step::frame.returned;
    timer_entered=timer_step::frame.updates_entered;timer_returned=timer_step::frame.updates_returned;
    // Consume the certificate before either the pinned Clock setter or Resume.
    snapshot_prepare::phase=snapshot_prepare::Phase::Failed;
    native::ClockState actual;std::string clock_cause;
    const bool selected=native::set_clock(true,1.0f,clock_cause);
    require(selected,"Recovery original selected Clock pause failed: "+clock_cause);
    const bool clock_read=native::read_clock(actual,clock_cause);
    require(clock_read&&actual.time_scale==0,"Recovery original selected Clock pause readback failed: "+clock_cause);
    require(read<uint32_t>(clock+0x58)==0,"Recovery selected Clock pause readback differs");
    bool release_attempted=false,pause_attempted=false;
    try{
        // Retire normal ordered retrieval while held. Original recovery
        // preparation settles existing native work together with its flush.
        loading_dispatch::ordered_run::cancel();
        release_attempted=true;original_resume(reinterpret_cast<void*>(scheduler));
        require(tasks()==scheduler&&read<uint8_t>(scheduler+0xf8)==0,"Recovery original scheduler Resume was not acknowledged");
        held_tasks=0;lifecycle.workers_held=0;
        {loading_dispatch::RecoveryDrain drain(value,owner,scheduler);
            snapshot_prepare::prepare_native(native_finish);}
        pause_attempted=true;original_pause(reinterpret_cast<void*>(scheduler));held_tasks=scheduler;held();lifecycle.workers_held=1;
        snapshot_prepare::empty_tasks();no_go();exact(value);prepared=true;
        snapshot_prepare::recovery_owner={value.lease,value.interruption,value.generation,{}};
        std::memcpy(snapshot_prepare::recovery_owner.nonce,value.recovery_nonce,37);
        snapshot_prepare::certificate={sizeof(MT2FW_MT2_SnapshotRequest),MT2FW_MT2_SNAPSHOT_CAPTURE,
            value.lease,value.session,value.world_epoch,value.completed_tick,0,value.interruption,{}};
        std::memcpy(snapshot_prepare::certificate.nonce,value.recovery_nonce,37);
        snapshot_prepare::certificate.peer_mask=uint8_t(value.original_mask);
    }catch(const std::exception& e){
        std::string cause=e.what();
        if(release_attempted&&!pause_attempted){pause_attempted=true;
            try{original_pause(reinterpret_cast<void*>(scheduler));held_tasks=scheduler;held();lifecycle.workers_held=1;}
            catch(const std::exception& hold){cause+="; recovery held restoration failed: "+std::string(hold.what());}
            catch(...){cause+="; recovery held restoration raised unknown exception";}}
        snapshot_prepare::phase=snapshot_prepare::Phase::Failed;throw std::runtime_error(cause);
    }catch(...){
        std::string cause="Recovery original preparation raised unknown exception";
        if(release_attempted&&!pause_attempted){pause_attempted=true;
            try{original_pause(reinterpret_cast<void*>(scheduler));held_tasks=scheduler;held();lifecycle.workers_held=1;}
            catch(const std::exception& hold){cause+="; recovery held restoration failed: "+std::string(hold.what());}
            catch(...){cause+="; recovery held restoration raised unknown exception";}}
        snapshot_prepare::phase=snapshot_prepare::Phase::Failed;throw std::runtime_error(cause);
    }
}
void capture(const MT2FW_MT2_RecoveryReceipt& value,MT2FW_MT2_SnapshotCaptureFn callback,void* user){
    exact(value);no_go();require(prepared&&!capture_consumed&&callback&&value.capture_attempted==1&&
        value.phase==MT2FW_MT2_RECOVERY_CONSUMED&&snapshot_prepare::phase==snapshot_prepare::Phase::Prepared,
        "Recovery native capture is not one unused prepared certificate");
    capture_consumed=true;certificate=value;
    std::memcpy(snapshot_prepare::certificate.witness_sha256,value.witness_sha256,32);
    snapshot_prepare::phase=snapshot_prepare::Phase::Consumed;++snapshot_prepare::captures;
    mt2::PreparedSaveScope scope(snapshot_prepare::prepared_mode,snapshot_prepare::prepared_tasks,thread);
    char cause[1024]{};MT2FW_MT2_Result result;
    try{result=callback(user,cause,sizeof(cause));}
    catch(...){if(scope.asynchronous)async_save::retain_scope(scope);throw;}
    if(scope.asynchronous)async_save::retain_scope(scope);
    require(std::memchr(cause,0,sizeof(cause))!=nullptr,"Recovery capture cause is not terminated");
    require(result==MT2FW_MT2_OK,"Recovery original capture callback failed result="+std::to_string(result)+": "+cause);
    no_go();
    if(scope.asynchronous)require(async_save::started()&&scope.saves==1&&scope.flushes==1&&scope.resumes<=1,
        "Recovery capture did not start exactly one original async Save");
    else{require(scope.saves==1&&scope.flushes==1&&scope.resumes==1,"Recovery capture did not complete exactly one original blocking Save");capture_completed=true;}
}
bool pump(const MT2FW_MT2_RecoveryReceipt& value){
    exact(value);no_go();require(capture_consumed&&value.capture_attempted==1&&value.phase==MT2FW_MT2_RECOVERY_CONSUMED,
        "Recovery PUMP cannot start or replace a native writer");
    if(!capture_completed)capture_completed=async_save::pump();
    no_go();if(capture_completed){snapshot_prepare::empty_tasks();snapshot_prepare::capture_returned=true;}
    return capture_completed;
}
void retire(const MT2FW_MT2_RecoveryReceipt& value){
    require(value.survivor_mask&&(value.survivor_mask&(1u<<(value.local_slot-1)))&&
        value.local_process==value.original.process_nonces[value.local_slot-1],"Recovery retirement lost the original surviving process");
    if(value.local_slot==1){exact(value);no_go();require(capture_completed&&value.phase==MT2FW_MT2_RECOVERY_PUBLISHED,
        "Recovery Host retirement lacks its completed fresh publication");async_save::retire();}
    else require(value.phase==MT2FW_MT2_RECOVERY_INTERRUPTED&&!attempted&&!capture_consumed,
        "Recovery Client retirement cannot adopt a Host capture");
    preflight_exit();require(quit_save.route==QuitSave::Route::None,"Recovery retirement conflicts with another original Quit intent");
    quit_save.route=QuitSave::Route::Recovery;quit_save.lease=value.lease;quit_save.session=value.session;
    quit_save.world=value.world_epoch;quit_save.mode=bootstrap.mode;
    quit_save.required=read<int32_t>(bootstrap.mode+0x2c4)!=0;quit_save.ready=true;quit_save.nonce=value.recovery_nonce;
    if(quit_save.required)quit_save.directory=async_save::text_at(bootstrap.mode+0x2e0);
}
void reset(){watch_reset();certificate={sizeof(certificate)};attempted=prepared=capture_consumed=capture_completed=false;go_entered=go_returned=timer_entered=timer_returned=0;}
}
