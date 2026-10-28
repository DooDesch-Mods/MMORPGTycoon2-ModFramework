void checkpoint(const MT2FW_MT2_StepReceipt& receipt){
    static const bool private_profile=[](){
        for(const auto& part:root){auto name=part.string();std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
            if(name=="_mptest")return true;}return false;
    }();
    if(!private_profile)return;
    static mt2::step_checkpoint::Selection selected;
    static bool selection_read=false,selection_valid=false;
    try{
        if(!selection_read){
            selection_read=true;
            const auto override_path=root/"diagnostics"/"step-checkpoints.json";
            if(fs::exists(override_path)){
                require(fs::is_regular_file(override_path)&&fs::file_size(override_path)<=1024,"Private Step checkpoint override is not a bounded file");
                selected.configure(read_json(override_path));
                runtime->log("steps","info","Private Step checkpoint ticks="+std::to_string(selected.ticks[0])+","+std::to_string(selected.ticks[1]));
            }
            selection_valid=true;
        }
        if(!selection_valid||!selected.claim(receipt))return;
        require(control.phase==Control::Phase::Held&&!control.entered&&!timer_step::frame.active&&thread==GetCurrentThreadId()&&
            receipt.generation==bootstrap.identity.generation&&receipt.world_epoch==bootstrap.world_epoch,
            "Private Step checkpoint lifecycle differs from held bootstrap");
        held();presentation_rng::check();
        native::SimulationState sim;std::string cause;
        require(native::read_simulation_state(sim,cause),"Private Step checkpoint simulation read failed: "+cause);
        const auto random=read<std::array<uint64_t,4>>(rng());const auto extra=read<int32_t>(rng()+0x20);
        mt2::determinism::WorkWitness work([](uintptr_t at,size_t count){
            mt2::determinism::WorkWitness::Bytes value(count);SIZE_T copied=0;
            require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),value.data(),count,&copied)&&copied==count,
                "Private Step checkpoint native read failed");return value;
        },image,owner);
        const auto canonical=mt2::step_checkpoint::canonical(work,receipt.completed_tick,bootstrap.identity.snapshot_sha256,
            *bootstrap.identity.agreed_seed,random,extra,sim.cash,sim.logged_in_subscribers);
        work.finish();held();
        native::SimulationState after;cause.clear();
        require(native::read_simulation_state(after,cause)&&sim.cash==after.cash&&sim.logged_in_subscribers==after.logged_in_subscribers,
            "Private Step checkpoint simulation changed during read: "+cause);
        require(read<std::array<uint64_t,4>>(rng())==random&&read<int32_t>(rng()+0x20)==extra,"Private Step checkpoint RNG changed during read");
        const auto encoded=canonical.dump();require(encoded.size()<=1024*1024,"Private Step checkpoint exceeds 1 MiB bound");
        auto gaps=work.gaps();
        gaps.push_back("NPC state and world objects, terrain, buildings, paths, quests and full world graph are not captured");
        gaps.push_back("Full Clock graph, economy records, software/development projects and tutorial/scenario state are not captured");
        const Json artifact={{"schema_version",1},{"canonical_sha256",digest(encoded)},{"canonical",canonical},{"complete_world_state",false},{"gaps",gaps},
            {"local_evidence",{{"checkpoint_ticks",selected.ticks},{"ordered_finish",loading_dispatch::ordered_finish::status()},{"process_id",GetCurrentProcessId()},{"generation",receipt.generation},{"world_epoch",receipt.world_epoch},
                {"lease",receipt.lease},{"session",receipt.session},{"thread",receipt.thread},{"actual_role",receipt.actual_role},
                {"phase",receipt.phase},{"outcome",receipt.outcome},{"tick",receipt.tick},{"completed_tick",receipt.completed_tick},
                {"completed_steps",control.completed},{"invocation",receipt.invocation},{"callbacks_inflight",receipt.callbacks_inflight},
                {"original_attempted",receipt.original_attempted},{"original_returned",receipt.original_returned},{"timer_calls",receipt.timer_calls},
                {"dt_bits",receipt.dt_bits},{"workers_held",receipt.workers_held},{"bootstrap_coverage",receipt.bootstrap_coverage}}}};
        const auto path=root/"diagnostics"/("step-checkpoint-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(receipt.completed_tick)+".json");
        require(!fs::exists(path),"Private Step checkpoint path exists; no overwrite");
        atomic_json(path,artifact);require(read_json(path)==artifact,"Private Step checkpoint persisted readback differs");
        runtime->log("steps","info","Private Step checkpoint retained tick="+std::to_string(receipt.completed_tick)+" path="+path.string());
    }catch(const std::exception& error){
        runtime->log("steps","error","Private Step checkpoint failed tick="+std::to_string(receipt.completed_tick)+": "+error.what()+"; accepted step unchanged; no retry");
    }catch(...){
        runtime->log("steps","error","Private Step checkpoint failed tick="+std::to_string(receipt.completed_tick)+": unknown diagnostic exception; accepted step unchanged; no retry");
    }
}
