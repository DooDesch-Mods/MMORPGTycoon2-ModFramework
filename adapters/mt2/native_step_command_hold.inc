// Road, Wall and Pass completion nest Pause/Resume inside the admitted command.
// Their Resume calls must leave the outer ordered hold in place until the next Go.
namespace command_hold {
constexpr uintptr_t RoadResumeReturn=0x5cbfff,WallResumeReturn=0x5ebf8b,PassResumeReturn=0x5c94b5;
uint64_t preserved=0,wall_preserved=0,pass_preserved=0;
void reject(const char* kind,const std::string& cause){
    if(failure.empty())failure=std::string("Step nested ")+kind+" Resume: "+cause;
    native_steps->failed(failure);
    runtime->log("steps","error",failure);
}
void resume_from(void* receiver,uintptr_t caller){
    if(snapshot_prepare::preserve_resume(receiver,caller))return;
    const bool wall=caller==image+WallResumeReturn;
    const bool pass=caller==image+PassResumeReturn;
    const char* kind=pass?"Pass":wall?"Wall":"Road";
    if((caller!=image+RoadResumeReturn&&!wall&&!pass)||!driver||!driver->frozen()||!held_tasks){
        original_resume(receiver);return;
    }
    try{
        require(thread==GetCurrentThreadId(),"owner thread changed");
        require(!driver->failed()&&failure.empty()&&!control.entered&&control.phase==Control::Phase::Held,
            "outer ordered hold is not current");
        const auto receipt=native_steps->snapshot();
        require(receipt.lease&&receipt.phase==MT2FW_MT2_STEP_HELD,"native lease is not held");
        require(reinterpret_cast<uintptr_t>(receiver)==held_tasks,"Task manager receiver differs");
        held();
        auto& count=pass?pass_preserved:wall?wall_preserved:preserved;
        if(count==0)runtime->log("steps","info",std::string("Preserved outer ordered hold at original ")+kind+" completion Resume");
        if(count!=UINT64_MAX)++count;
    }catch(const std::exception& error){reject(kind,error.what());}
    catch(...){reject(kind,"unknown native validation failure");}
}
__declspec(noinline) void resume_entry(void* receiver){
    resume_from(receiver,reinterpret_cast<uintptr_t>(_ReturnAddress()));
}
Json status(){return {{"road_resume_preserved",preserved},{"wall_resume_preserved",wall_preserved},{"pass_resume_preserved",pass_preserved}};}
}
