using LocalProof=mt2::NativeStepRecovery::LocalProof;
struct WatchStamp {
    snapshot_prepare::Phase phase=snapshot_prepare::Phase::Idle;
    MT2FW_MT2_SnapshotRequest snapshot{};
    uintptr_t mode=0,tasks=0,writer=0,save_task=0;
};
WatchStamp watch_stamp{},ordinary_watch_previous{};
MT2FW_MT2_SnapshotRequest ordinary_watch_owner{};
bool watch_pending=false,ordinary_watch_active=false;
bool same_snapshot_owner(const MT2FW_MT2_SnapshotRequest& a,const MT2FW_MT2_SnapshotRequest& b){
    return a.size==b.size&&a.operation==b.operation&&a.lease==b.lease&&a.session==b.session&&a.world_epoch==b.world_epoch&&
        a.tick==b.tick&&a.invocation==b.invocation&&a.order==b.order&&a.peer_mask==b.peer_mask&&a.reserved==b.reserved&&
        std::memcmp(a.nonce,b.nonce,37)==0;
}
bool same_watch(const WatchStamp& a,const WatchStamp& b){
    return a.phase==b.phase&&a.mode==b.mode&&a.tasks==b.tasks&&a.writer==b.writer&&a.save_task==b.save_task&&
        same_snapshot_owner(a.snapshot,b.snapshot)&&a.snapshot.ack_mask==b.snapshot.ack_mask&&
        std::memcmp(a.snapshot.witness_sha256,b.snapshot.witness_sha256,32)==0;
}
void watch_reset(){watch_stamp={};ordinary_watch_previous={};ordinary_watch_owner={};watch_pending=ordinary_watch_active=false;}
void adoption_ready(){
    require(!watch_pending,"Recovery adoption overlaps a healthy observation");
    if(!attempted){
        require(!async_save::retained&&!async_save::published.load(std::memory_order_acquire),
            "Recovery cannot adopt an ordinary or foreign native Save writer");
        require(snapshot_prepare::phase==snapshot_prepare::Phase::Idle||snapshot_prepare::phase==snapshot_prepare::Phase::Released,
            "Recovery cannot adopt an old pending snapshot operation");
    }
}
void healthy_watch(const MT2FW_MT2_RecoverySpec& spec,const MT2FW_MT2_StepReceipt& step,LocalProof purpose,bool initial){
    using Phase=snapshot_prepare::Phase;
    require(!attempted&&!capture_consumed&&!snapshot_prepare::recovery_owner.lease,
        "Healthy Recovery watch cannot use a recovery-owned snapshot");
    WatchStamp current;current.phase=snapshot_prepare::phase;
    const auto& snapshot=snapshot_prepare::certificate;
    current.snapshot=snapshot;current.mode=snapshot_prepare::prepared_mode;current.tasks=snapshot_prepare::prepared_tasks;
    const bool inactive=current.phase==Phase::Idle||current.phase==Phase::Released;
    const bool writer=bool(async_save::retained)||async_save::published.load(std::memory_order_acquire)!=nullptr;
    if(inactive){
        require(!writer&&read<int32_t>(bootstrap.mode+0x30c)==0,"Healthy Recovery watch has an unowned native Save writer");
        if(ordinary_watch_active){
            require(current.phase==Phase::Released&&same_snapshot_owner(snapshot,ordinary_watch_owner),
                "Healthy Recovery watch lost its ordinary snapshot without exact release");
            ordinary_watch_active=false;ordinary_watch_owner={};ordinary_watch_previous={};
        }
        if(initial)initial_tasks();
    }else{
        require(!initial&&(current.phase==Phase::Prepared||current.phase==Phase::Sealed||current.phase==Phase::Consumed),
            "Healthy Recovery watch has an unknown or armed snapshot phase");
        snapshot_prepare::shape(snapshot);
        uint8_t mask=0;for(unsigned i=0;i<4;++i)if(spec.original.physical_ids[i])mask|=uint8_t(1u<<i);
        require((snapshot.operation==MT2FW_MT2_SNAPSHOT_ARM||snapshot.operation==MT2FW_MT2_SNAPSHOT_ARM_PERIODIC)&&
            snapshot.lease==step.lease&&snapshot.session==step.session&&
            snapshot.world_epoch==step.world_epoch&&snapshot.tick==step.completed_tick&&snapshot.invocation&&
            snapshot.order==snapshot_prepare::last_order&&snapshot.peer_mask==mask&&
            snapshot_prepare::prepared_mode==bootstrap.mode&&snapshot_prepare::prepared_tasks==held_tasks,
            "Healthy Recovery watch ordinary snapshot identity or native owner differs");
        // A later completed original Step proves the previous snapshot passed
        // its ordinary release gate, even if release and Next shared a frame.
        if(ordinary_watch_active&&snapshot.tick>ordinary_watch_owner.tick&&snapshot.order>ordinary_watch_owner.order)
            {ordinary_watch_active=false;ordinary_watch_previous={};}
        if(ordinary_watch_active)require(same_snapshot_owner(snapshot,ordinary_watch_owner),
            "Healthy Recovery watch ordinary snapshot owner changed");
        else{ordinary_watch_owner=snapshot;ordinary_watch_active=true;}
        require(current.phase==Phase::Prepared?snapshot_prepare::empty_hash(snapshot):!snapshot_prepare::empty_hash(snapshot),
            "Healthy Recovery watch ordinary snapshot witness phase differs");
        current.snapshot=snapshot;current.mode=snapshot_prepare::prepared_mode;current.tasks=snapshot_prepare::prepared_tasks;
        if(writer){
            auto& held=async_save::held_owner();
            require(current.phase==Phase::Consumed&&step.actual_role==MT2FW_MT2_SESSION_HOST&&held.entry_returned&&held.task.load()&&
                same_snapshot_owner(held.certificate,snapshot)&&held.certificate.ack_mask==snapshot.ack_mask&&
                std::memcmp(held.certificate.witness_sha256,snapshot.witness_sha256,32)==0&&held.save.asynchronous&&
                held.save.mode==current.mode&&held.save.tasks==current.tasks&&held.save.thread==step.thread&&
                held.save.saves==1&&held.save.flushes==1&&held.save.resumes<=1,
                "Healthy Recovery watch writer is not owned by the exact ordinary snapshot");
            const auto saves=read<int32_t>(bootstrap.mode+0x30c);
            require(saves==0||saves==1,"Healthy Recovery watch ordinary native Save count differs");
            current.writer=reinterpret_cast<uintptr_t>(&held);
            current.save_task=held.task.load();
        }else{
            require(current.phase!=Phase::Consumed||snapshot_prepare::capture_returned,
                "Healthy Recovery watch ordinary capture has no returned capture or owned writer");
            snapshot_prepare::clock();snapshot_prepare::empty_tasks();
        }
        if(ordinary_watch_previous.mode){
            require(current.phase>=ordinary_watch_previous.phase,
                "Healthy Recovery watch ordinary snapshot phase went backwards");
            if(ordinary_watch_previous.phase!=Phase::Prepared)require(
                std::memcmp(current.snapshot.witness_sha256,ordinary_watch_previous.snapshot.witness_sha256,32)==0,
                "Healthy Recovery watch sealed ordinary witness changed");
            if(ordinary_watch_previous.writer)require(current.writer==ordinary_watch_previous.writer&&current.save_task==ordinary_watch_previous.save_task,
                "Healthy Recovery watch ordinary writer changed before release");
        }
    }
    if(purpose==LocalProof::WatchBegin){
        require(!watch_pending,"Healthy Recovery watch reentered its observation");watch_stamp=current;watch_pending=true;
    }else{
        require(purpose==LocalProof::WatchEnd&&watch_pending&&same_watch(current,watch_stamp),
            "Healthy Recovery watch snapshot or writer changed during observation");
        watch_pending=false;watch_stamp={};
        if(!inactive)ordinary_watch_previous=current;
    }
}
