// Quit retains its native intent before _BeginQuit changes windows or mode.
struct QuitSave {
    enum class Route {None,Managed,Terminal,Reload};
    Route route=Route::None;
    uint64_t lease=0,session=0,world=0;uintptr_t mode=0;
    bool required=false,ready=false,consumed=false;
    std::string nonce,directory;
} quit_save;
void quit_save_owner(){
    const auto receipt=native_steps->snapshot();
    require(quit_save.route!=QuitSave::Route::None&&quit_save.mode==bootstrap.mode&&
        quit_save.lease==receipt.lease&&quit_save.session==receipt.session&&quit_save.world==receipt.world_epoch,
        "Quit Save retained native owner changed");
    require(quit_save.required==(read<int32_t>(bootstrap.mode+0x2c4)!=0),
        "Quit Save original autosave condition changed");
    if(quit_save.required)require(quit_save.directory==async_save::text_at(bootstrap.mode+0x2e0),"Quit Save original local world directory changed");
}
void exit_save(uint32_t operation,const std::string& nonce){
    preflight_exit();
    const auto receipt=native_steps->snapshot();
    if(operation==MT2FW_MT2_EXIT_SAVE_BEGIN){
        require(nonce.empty()&&receipt.phase==MT2FW_MT2_STEP_HELD,"Quit Save BEGIN requires healthy held service without a nonce");
        if(quit_save.route!=QuitSave::Route::None){
            quit_save_owner();require(quit_save.route==QuitSave::Route::Managed,"Quit Save BEGIN conflicts with native retirement scope");return;
        }
        QuitSave retained;retained.route=QuitSave::Route::Managed;retained.lease=receipt.lease;retained.session=receipt.session;
        retained.world=receipt.world_epoch;retained.mode=bootstrap.mode;retained.required=read<int32_t>(bootstrap.mode+0x2c4)!=0;
        if(retained.required)retained.directory=async_save::text_at(bootstrap.mode+0x2e0);
        quit_save=std::move(retained);
        // QUIT is an intent for this pinned native branch, not a synthetic
        // report that the inner Async Save has already executed.
        require(native_steps->save_request(reinterpret_cast<void*>(bootstrap.mode),MT2FW_MT2_SAVE_QUIT,
            "autosave",8,quit_save.required,0x4a0690),"Quit Save intent was not retained");return;
    }
    if(operation==MT2FW_MT2_EXIT_SAVE_COMPLETE){
        quit_save_owner();require(quit_save.route==QuitSave::Route::Managed&&!quit_save.ready&&!quit_save.consumed&&
            receipt.phase==MT2FW_MT2_STEP_HELD,"Quit Save COMPLETE lacks its untouched healthy intent");
        if(quit_save.required){
            const auto& certificate=snapshot_prepare::certificate;
            require(nonce.size()==36&&nonce==certificate.nonce&&snapshot_prepare::phase==snapshot_prepare::Phase::Released&&
                certificate.lease==receipt.lease&&certificate.session==receipt.session&&certificate.world_epoch==receipt.world_epoch&&
                receipt.completed_tick>=certificate.tick&&
                ((receipt.actual_role==MT2FW_MT2_SESSION_HOST&&snapshot_prepare::capture_returned)||
                 (receipt.actual_role==MT2FW_MT2_SESSION_CLIENT&&snapshot_prepare::client_cache_returned)),
                "Quit Save COMPLETE lacks its exact released file/cache certificate");
        }else require(nonce.empty(),"Quit without native autosave cannot claim a Save nonce");
        quit_save.nonce=nonce;quit_save.ready=true;return;
    }
    if(operation==MT2FW_MT2_EXIT_SAVE_TERMINAL&&quit_save.route==QuitSave::Route::Managed){
        quit_save_owner();
        require(nonce.empty()&&receipt.phase==MT2FW_MT2_STEP_FAILED&&!quit_save.ready&&!quit_save.consumed,
            "Terminal Quit transition requires one failed unconsumed managed Save intent");
        require(!async_save::pending(),"Terminal Quit transition retains an incomplete native async Save; no original retry");
        // A new explicit Leave may use the original terminal autosave only
        // after the failed managed attempt has no live or uncertain writer.
        // The completed managed writer still publishes its queue owner after
        // a failed companion. Retire that local receipt before original Quit
        // can queue its distinct terminal autosave.
        async_save::retire();
        quit_save.route=QuitSave::Route::Terminal;quit_save.ready=true;return;
    }
    require((operation==MT2FW_MT2_EXIT_SAVE_TERMINAL||operation==MT2FW_MT2_EXIT_SAVE_RELOAD)&&quit_save.route==QuitSave::Route::None,
        "Original Quit retirement scope is invalid or repeated");
    if(operation==MT2FW_MT2_EXIT_SAVE_TERMINAL){
        require(nonce.empty()&&receipt.phase==MT2FW_MT2_STEP_FAILED,"Terminal Quit requires the retained failed service");
        require(!async_save::pending(),"Terminal Quit retains an incomplete native async Save; no original retry");
        async_save::retire();
    }else{
        const auto& certificate=snapshot_prepare::certificate;
        require(receipt.phase==MT2FW_MT2_STEP_HELD&&snapshot_prepare::phase==snapshot_prepare::Phase::Released&&
            nonce.size()==36&&nonce==certificate.nonce&&certificate.lease==receipt.lease&&certificate.session==receipt.session&&
            certificate.world_epoch==receipt.world_epoch&&certificate.tick==receipt.completed_tick,
            "Reload Quit requires its unchanged released snapshot hold");
    }
    quit_save.route=operation==MT2FW_MT2_EXIT_SAVE_TERMINAL?QuitSave::Route::Terminal:QuitSave::Route::Reload;
    quit_save.lease=receipt.lease;quit_save.session=receipt.session;quit_save.world=receipt.world_epoch;quit_save.mode=bootstrap.mode;
    quit_save.required=read<int32_t>(bootstrap.mode+0x2c4)!=0;quit_save.ready=true;quit_save.nonce=nonce;
    if(quit_save.required)quit_save.directory=async_save::text_at(bootstrap.mode+0x2e0);
}
int quit_save_route(void* receiver,uint32_t kind,const std::string& filename,bool option,uintptr_t caller){
    if(!loading_dispatch::quit_empty_dispatch)return 0;
    require(quit_attempted&&!exit_requested&&thread==GetCurrentThreadId()&&
        reinterpret_cast<uintptr_t>(receiver)==bootstrap.mode&&caller==image+0x4a079a&&
        kind==MT2FW_MT2_SAVE_ASYNC&&filename=="autosave"&&!option,
        "Original Quit autosave call differs from its pinned native branch");
    quit_save_owner();
    require(quit_save.ready&&quit_save.required&&!quit_save.consumed,
        "Original Quit autosave lacks one completed or explicit retirement intent");
    quit_save.consumed=true;
    // Managed completion already published this exact local file/companion.
    // Existing terminal/reload teardown still owns its original unmanaged Save.
    return quit_save.route==QuitSave::Route::Managed?1:2;
}
