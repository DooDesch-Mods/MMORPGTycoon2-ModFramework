// Included inside the common driver. Exit frames follow an acknowledged
// original Quit command and are native teardown, never authoritative ticks.
bool exit_requested=false,quit_attempted=false;
uint64_t exit_frames=0;
void preflight_exit(){
    require(driver&&driver->frozen()&&!driver->failed()&&failure.empty()&&bootstrap.game&&
        !quit_attempted&&!exit_requested&&thread==GetCurrentThreadId()&&
        control.phase==Control::Phase::Held&&!control.entered&&!timer_step::frame.active&&
        owner==bootstrap.game&&read<uintptr_t>(owner+0x28)==bootstrap.mode,
        "Step exit preflight requires the known frozen native owner outside original execution");
    native_steps->validate_native_exit_request();held();
    const auto finished=read<int32_t>(held_tasks+0xc0),snapshot=read<int32_t>(held_tasks+0xd8),cursor=read<int32_t>(held_tasks+0xdc);
    require(!loading_dispatch::uncertain&&!loading_dispatch::pending&&!bootstrap_boundary::uncertain&&
        !bootstrap_boundary::reveal::uncertain&&!bootstrap_boundary::branch_changed&&
        finished==0&&((snapshot==0&&cursor==0)||
            (snapshot==1&&cursor==1&&loading_dispatch::save_completion::consumed_snapshot(held_tasks))||
            loading_dispatch::completed_callbacks::matches(held_tasks)),
        "Step exit preflight requires zero finished/deferred tasks and an empty or acknowledged consumed callback snapshot; actual="+
        std::to_string(finished)+"/"+std::to_string(snapshot)+"/"+std::to_string(cursor)+"/"+std::to_string(loading_dispatch::pending));
    require(task_boundary::all_done(reinterpret_cast<void*>(held_tasks)),"Step exit preflight AllTasksAreDone returned false; required=true");
    native::WorldStatus actual;std::string cause;
    require(native::read_world_status(actual,cause),"Step exit preflight world read failed: "+cause);
    require(actual.active_mode_index==3&&!actual.title_transition_pending&&!actual.quitting&&
        actual.saves_in_progress==0&&actual.edit_operations==0,
        "Step exit preflight requires an idle loaded world before Quit");
}
#include "native_quit_save.inc"
void command_return(void* receiver,const void* command){
    if(!driver||!driver->frozen()||!bootstrap.game||exit_requested)return;
    const char* data=nullptr;size_t length=0;
    const auto code=command_view(command,data,length);if(code||length!=4)return;
    char name[4]{};require(command_copy(data,name,sizeof(name))==0,"Step exit command copy failed");
    if(std::memcmp(name,"Quit",4))return;
    require(!driver->failed()&&failure.empty()&&thread==GetCurrentThreadId()&&reinterpret_cast<uintptr_t>(receiver)==bootstrap.mode&&
        read<uintptr_t>(owner+0x28)==bootstrap.mode&&read<uint8_t>(bootstrap.mode+0x304)==1&&read<uint8_t>(bootstrap.mode+0x305)==0,
        "Step original Quit lacks current owner and native return-to-Title acknowledgement");
    native_steps->native_exit_requested();exit_requested=true;
}
void command_call(void* receiver,const void* command){
    if(!driver||!driver->frozen()||!bootstrap.game){original_ingame_command(receiver,command);return;}
    const char* data=nullptr;size_t length=0;char name[4]{};
    const bool quit=command_view(command,data,length)==0&&length==4&&command_copy(data,name,sizeof(name))==0&&std::memcmp(name,"Quit",4)==0;
    if(!quit){original_ingame_command(receiver,command);return;}
    const char* stage="admission";
    try{
        const auto receipt=native_steps->snapshot();
        require(!quit_attempted&&!exit_requested&&!driver->failed()&&failure.empty()&&thread==GetCurrentThreadId()&&
            (receipt.phase==MT2FW_MT2_STEP_HELD||receipt.phase==MT2FW_MT2_STEP_FAILED)&&receipt.callbacks_inflight==1&&receipt.workers_held==1&&
            control.phase==Control::Phase::Held&&!control.entered&&!timer_step::frame.active&&
            reinterpret_cast<uintptr_t>(receiver)==bootstrap.mode&&read<uintptr_t>(owner+0x28)==bootstrap.mode,
            "Step Quit requires one healthy held service command before Next");
        preflight_exit();
        if(quit_save.route==QuitSave::Route::None){exit_save(MT2FW_MT2_EXIT_SAVE_BEGIN,{});return;}
        quit_save_owner();
        if(!quit_save.ready)return;
        quit_attempted=true;loading_dispatch::quit_empty_dispatch=true;stage="original";
        original_ingame_command(receiver,command);
        loading_dispatch::quit_empty_dispatch=false;stage="native acknowledgement";
        require(quit_save.consumed==quit_save.required,"Original Quit did not consume its exact native autosave branch");
        command_return(receiver,command);
    }catch(const std::exception& error){
        loading_dispatch::quit_empty_dispatch=false;
        if(failure.empty())failure=std::string("Step Quit ")+stage+" failed; no replay: "+error.what();
        native_steps->failed(failure);runtime->log("steps","error",failure);throw;
    }catch(...){
        loading_dispatch::quit_empty_dispatch=false;
        if(failure.empty())failure=std::string("Step Quit ")+stage+" raised an unknown exception; no replay";
        native_steps->failed(failure);runtime->log("steps","error",failure);throw;
    }
}
mt2::NativeStepKernel::Proof exit_proof(){
    mt2::NativeStepKernel::Proof proof;proof.thread=GetCurrentThreadId();
    if(!exit_requested||!bootstrap.game||proof.thread!=thread||control.entered||timer_step::frame.active)return proof;
    native::WorldStatus status;std::string cause;
    require(native::read_world_status(status,cause),"Step exited-world observation failed: "+cause);
    if(status.active_mode_index!=2||status.requested_mode_index!=2||status.title_transition_pending||status.world_present||
        status.quitting||(status.saves_in_progress!=0&&status.saves_in_progress!=-1)||
        (status.edit_operations!=0&&status.edit_operations!=-1))return proof;
    const auto title=read<uintptr_t>(owner+0x350);
    require(title&&read<uintptr_t>(owner+0x28)==title&&read<uintptr_t>(title)==image+0x16dfbb0,
        "Step exited-world Title owner/address point differs");
    // Original Deinit deletes GameState and clears Mode+58. The three native
    // singleton destructors clear these slots; a changed pointer is not enough.
    if(read<uintptr_t>(bootstrap.mode+0x58)||read<uintptr_t>(image+0x13df2a0)||
        read<uintptr_t>(image+0x13df9c0)||read<uintptr_t>(image+0x13df8c0))return proof;
    held();
    if(read<int32_t>(held_tasks+0xc0)||!task_boundary::all_done(reinterpret_cast<void*>(held_tasks)))return proof;
    require(!loading_dispatch::uncertain&&!bootstrap_boundary::uncertain&&!bootstrap_boundary::reveal::uncertain&&!bootstrap_boundary::branch_changed,
        "Step exited-world native mutation remains uncertain or published");
    proof.world_exited=true;proof.quiescent=true;return proof;
}
void reset_driver(){
    require(exit_proof().world_exited,"Step reset requires destroyed native world and held idle Title");
    bootstrap_boundary::reveal::restore();
    driver->reset_after_world_exit();
    region_rng_load::reset();snapshot_prepare::reset();
    recovery_runtime_reset();
    presentation_rng::check();presentation_rng::cancel();
    // All old task ownership has ended through original Deinit/Flush/Finish.
    // Reset only local admission receipts; never edit a native queue or RNG.
    loading_dispatch::pending=0;loading_dispatch::partitions=loading_dispatch::deferred_visits=0;
    loading_dispatch::first_defer_frame=loading_dispatch::first_release_frame=0;
    loading_dispatch::release_admitted=loading_dispatch::finish_entered=loading_dispatch::finish_returned=0;
    loading_dispatch::first_error.clear();
    auto& ordering=loading_dispatch::ordered_finish::first_ids;ordering.clear();loading_dispatch::ordered_finish::last_ids.clear();
    loading_dispatch::ordered_finish::batches=loading_dispatch::ordered_finish::admitted=loading_dispatch::ordered_finish::moved_slots=0;
    loading_dispatch::ordered_finish::stale_visits=loading_dispatch::ordered_finish::last_frame=0;
    loading_dispatch::ordered_finish::order_hash=14695981039346656037ull;
    bootstrap_boundary::pending=false;bootstrap_boundary::load_mode=0;
    bootstrap_boundary::deferred=bootstrap_boundary::resumed=bootstrap_boundary::load_frame=0;bootstrap_boundary::first_error.clear();
    task_boundary::checked=task_boundary::skipped=task_boundary::attempted=task_boundary::returned=0;
    task_boundary::loading_skipped=task_boundary::ready_frame=0;task_boundary::last_mode_state=task_boundary::last_map_state=-1;task_boundary::first_error.clear();
    timer_step::frame={};
    // Resume only after kernel retirement and independently checked destruction.
    release_workers();resume_tasks=false;control={};bootstrap={};seeded_words={};lifecycle={sizeof(lifecycle)};
    pixels.invalidate();next_present=0;exit_requested=false;quit_attempted=false;exit_frames=0;quit_save={};
}
void exit_frame(void* game){
    require(exit_requested&&!driver->failed()&&failure.empty(),"Step teardown cannot replay an unknown original");
    if(exit_proof().world_exited&&native_steps->reset_after_world_exit())return;
    native::WorldStatus status;std::string cause;
    require(native::read_world_status(status,cause),"Step teardown status read failed: "+cause);
    require((status.active_mode_index==3&&read<uintptr_t>(owner+0x28)==bootstrap.mode&&status.quitting&&read<uint8_t>(bootstrap.mode+0x305)==0)||
        (status.active_mode_index==2&&!status.title_transition_pending&&!read<uintptr_t>(bootstrap.mode+0x58)),
        "Step teardown lost acknowledged native Quit or its actual Title transition");
    if(bootstrap_boundary::branch_changed){
        require(bootstrap_boundary::pending&&!bootstrap_boundary::uncertain,"Step teardown found an uncertain bootstrap branch");
        bootstrap_boundary::change_branch(bootstrap_boundary::HeldDelta,bootstrap_boundary::OriginalDelta);
        bootstrap_boundary::branch_changed=false;bootstrap_boundary::pending=false;
    }
    // Original Deinit deliberately Pause/Flush/Pump/Resume's its own tasks.
    // The gameplay admission hooks must forward those teardown operations.
    loading_dispatch::ordered_run::cancel();
    region_rng_load::cancel();
    control.phase=Control::Phase::Passive;
    if(held_tasks)release_workers();
    control.entered=true;timer_step::begin(owner,MT2FW_MT2_STEP_FIXED_DT_BITS);
    ++exit_frames;original_go(game);timer_step::finish();control.entered=false;
    pause_workers();timer_step::restore_held();
    mt2::observe_world(*runtime);
    if(exit_proof().world_exited)native_steps->reset_after_world_exit();
}
void verify_exit_bodies(){
#include "native_step_exit_pins.inc"
}
