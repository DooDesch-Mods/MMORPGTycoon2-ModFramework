// Private observation of the existing bounded Default RNG helper counters.
Json rng_diagnostic(const Json& args){
    static const bool private_profile=[](){
        for(const auto& part:root){auto name=part.string();std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
            if(name=="_mptest")return true;}return false;
    }();
    require(private_profile,"Step RNG diagnostic requires an owned private test profile");
    require(thread&&thread==GetCurrentThreadId(),"Step RNG diagnostic requires the bound game thread");
    const auto operation=args.at("action").get<std::string>();
    require(operation=="start"||operation=="status"||operation=="stop","Unknown Step RNG diagnostic action");
    // Stop remains available after a failed command or lease. It changes only
    // observation and must not require a healthy simulation to release it.
    if(operation=="stop")rng_trace::stop();
    if(operation=="start"){
        require(installed&&driver&&driver->frozen()&&native_steps&&lifecycle.bootstrap_held&&lifecycle.seed_returned==1&&failure.empty(),
            "Step RNG trace start requires a loaded ordered world");
        require(native_steps->snapshot().phase!=MT2FW_MT2_STEP_FAILED,"Step RNG trace cannot start on a failed lease");
        require(rng_trace::installed,"Step RNG helpers are not installed");
        rng_trace::start();
    }
    Json result={{"action",operation},{"process_id",GetCurrentProcessId()},{"trace",rng_trace::status()},{"presentation_rng",presentation_rng::status()},
        {"completed_steps",control.completed},{"inside_original_go",control.entered},
        {"coverage","Cumulative bounded helper receipts at this local diagnostic point; different local ticks are not a paired RNG equality boundary"}};
    if(native_steps){const auto receipt=native_steps->snapshot();
        result["ordered"]={{"generation",receipt.generation},{"world_epoch",receipt.world_epoch},{"lease",receipt.lease},
            {"session",receipt.session},{"phase",receipt.phase},{"tick",receipt.tick},{"completed_tick",receipt.completed_tick}};}
    try{result["global_random"]=read<std::array<uint64_t,4>>(rng());result["global_extra"]=read<int32_t>(rng()+0x20);}
    catch(const std::exception& error){result["random_read_error"]=error.what();}
    return result;
}
