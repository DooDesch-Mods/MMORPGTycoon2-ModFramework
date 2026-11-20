namespace client_periodic_ui {
mt2::NativeClientPeriodicState state;
uint32_t notify_exception=0;
struct WindowState {uint8_t visible=0,blocked=0,locked=0;int32_t mode=0;} delegated_window,before_window,after_window;
WindowState inspect_window(uintptr_t window){
    require(window&&read<uintptr_t>(bootstrap.mode+0x90)==window,"Client periodic UI Window no longer belongs to its Mode");
    require(read<uintptr_t>(window)==image+0x1718910&&read<uintptr_t>(image+0x1718910+0x1d0)==image+0x827210,
        "Client periodic UI native address point or Show slot differs");
    WindowState value{read<uint8_t>(window+0x45f),read<uint8_t>(window+0x359),read<uint8_t>(window+0x4ec),read<int32_t>(window+0x4e8)};
    require(value.visible<=1&&value.blocked<=1&&value.locked<=1,"Client periodic UI native flags are not Boolean");
    return value;
}
uint32_t notify_native(uintptr_t window){
    __try{reinterpret_cast<void(*)(void*)>(image+0x6665b0)(reinterpret_cast<void*>(window));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
void retain(void* receiver,uintptr_t caller){
    const auto receipt=native_steps->snapshot();
    require(caller==image+0x6666f5,"Client periodic UI delegation is outside original AutosaveWindow SetMode");
    require(receipt.actual_role==MT2FW_MT2_SESSION_CLIENT&&receipt.thread==GetCurrentThreadId()&&
        receipt.phase==MT2FW_MT2_STEP_ENTERED&&control.entered&&timer_step::frame.active&&
        bootstrap.game==owner&&receipt.world_epoch==bootstrap.world_epoch&&receipt.generation==lifecycle.generation,
        "Client periodic UI delegation lacks its current original Client Step");
    validate_save_mode(receiver);tasks();
    const auto window=read<uintptr_t>(bootstrap.mode+0x90);const auto observed=inspect_window(window);
    require(observed.visible==1&&observed.blocked==1&&observed.mode==0,
        "Client periodic UI delegation lacks its original pre-return saving state");
    state.retain(receipt,reinterpret_cast<uintptr_t>(receiver),window,snapshot_prepare::last_order);
    delegated_window=observed;before_window=after_window={};notify_exception=0;
}
void verify_completion_owner(){
    const auto receipt=native_steps->snapshot();
    require(receipt.actual_role==MT2FW_MT2_SESSION_CLIENT&&receipt.phase==MT2FW_MT2_STEP_HELD&&
        receipt.lease==state.lease&&receipt.session==state.session&&receipt.world_epoch==state.world_epoch&&
        receipt.generation==state.generation&&receipt.thread==state.thread&&receipt.thread==GetCurrentThreadId()&&
        receipt.completed_tick==state.certificate.tick&&receipt.completed_tick>=state.request_tick&&
        receipt.callbacks_inflight==1&&receipt.workers_held==1&&outer_entered&&!control.entered&&!timer_step::frame.active&&
        control.phase==Control::Phase::Held&&bootstrap.game==owner&&bootstrap.mode==state.mode,
        "Client periodic UI completion escaped its exact post-request held service");
    validate_save_mode(reinterpret_cast<void*>(state.mode));held();
    require(!async_save::retained&&!async_save::published.load(std::memory_order_acquire)&&
        read<int32_t>(state.mode+0x30c)==0&&!mt2::PreparedSaveScope::current&&!mt2::BlockingSaveScope::current,
        "Client periodic UI completion has a local native writer or active Save scope");
    require(snapshot_prepare::phase==snapshot_prepare::Phase::Released,
        "Client periodic UI completion requires successful ordinary snapshot release");
    const auto& captured=snapshot_prepare::certificate;
    require(captured.operation==MT2FW_MT2_SNAPSHOT_ARM_PERIODIC&&captured.lease==state.certificate.lease&&
        captured.session==state.certificate.session&&captured.world_epoch==state.certificate.world_epoch&&
        captured.tick==state.certificate.tick&&captured.order==state.certificate.order&&captured.peer_mask==state.certificate.peer_mask&&
        std::memcmp(captured.nonce,state.certificate.nonce,37)==0&&
        std::memcmp(captured.witness_sha256,state.certificate.witness_sha256,32)==0,
        "Client periodic UI released native certificate has a different purpose or owner");
    before_window=inspect_window(state.window);
    require(before_window.visible==1&&before_window.blocked==1&&before_window.mode==1&&before_window.locked==delegated_window.locked,
        "Client periodic UI completion lacks the same unwound visible saving window and edit lock");
}
void released(const MT2FW_MT2_SnapshotRequest& request){
    const bool completed=state.complete(request,verify_completion_owner,[]{
        notify_exception=notify_native(state.window);
        require(!notify_exception,"Client periodic UI NotifyDoneSaving raised native exception="+std::to_string(notify_exception));
    },[]{
        validate_save_mode(reinterpret_cast<void*>(state.mode));held();after_window=inspect_window(state.window);
        require(after_window.visible==0&&after_window.locked==0,
            "Client periodic UI original completion returned without hiding its window and releasing its edit lock");
    });
    if(completed)runtime->log("steps","info","Client periodic UI completed request_tick="+std::to_string(state.request_tick)+
        " capture_tick="+std::to_string(request.tick)+" order="+std::to_string(request.order)+" nonce="+state.certificate.nonce);
}
Json window_json(const WindowState& window){return {{"visible",window.visible},{"blocked",window.blocked},{"edit_lock",window.locked},{"mode",window.mode}};}
Json status(){
    constexpr const char* phases[]={"idle","delegated","bound","sealed","attempted","returned","failed"};
    return {{"phase",phases[size_t(state.phase)]},{"request_tick",state.request_tick},{"lease",state.lease},{"session",state.session},
        {"world_epoch",state.world_epoch},{"generation",state.generation},{"window",state.window},{"mode",state.mode},
        {"order_floor",state.order_floor},{"capture_tick",state.certificate.tick},{"order",state.certificate.order},
        {"nonce",state.certificate.nonce},{"attempted",state.attempted},{"returned",state.returned},{"seh_code",notify_exception},
        {"delegations",state.delegations},{"notify_attempts",state.notify_attempts},{"notify_returns",state.notify_returns},
        {"delegated_window",window_json(delegated_window)},{"before_window",window_json(before_window)},
        {"after_window",window_json(after_window)},{"cause",state.cause}};
}
void arm(const MT2FW_MT2_SnapshotRequest& request){
    if(state.bind(request))runtime->log("steps","info","Client periodic UI bound request_tick="+std::to_string(state.request_tick)+
        " capture_tick="+std::to_string(request.tick)+" order="+std::to_string(request.order)+" nonce="+state.certificate.nonce);
}
void seal(const MT2FW_MT2_SnapshotRequest& request){state.seal(request);}
void install(){
    presentation_rng::verify(0x6665b0,80,"d3fba1a9c6458c8026246215a39c63def90038d007062b8972d343b10db0813f");
    presentation_rng::verify(0x666600,304,"6ef581716ba4035dfcba328afabe8c29d13fa6597f30539a2762ae0bd9ef915a");
}
void reset(){state={};notify_exception=0;delegated_window=before_window=after_window={};}
}
