#define main driver_baseline_main
#include "native_step_driver_tests.cpp"
#undef main

int main(){try{
    {Test t(true);auto s=t.spec();struct Context{Test* t;bool lost=false;} context{&t};
        s.proof_user=&context;s.proof=[](void* p,const auto* q,auto* r){auto& c=*static_cast<Context*>(p);
            return c.lost?MT2FW_MT2_UNAVAILABLE:Test::c_proof(c.t,q,r);};
        check(t.driver->inspect(t.observed.identity.owner,s).context,"preload cannot freeze original identity");
        t.observed.phase=StepNativeObservation::Phase::Loaded;
        context.lost=true;
        check(!t.driver->inspect(t.observed.identity.owner,s).context,"ordinary membership proof was relabeled healthy");
        t.driver->recovery_local(t.observed.identity.owner,s,Test::c_proof,&t);
        check(t.go_count==0&&t.timer_count==0,"local recovery proof called original Go/Timer");
        t.put(Test::tasks+0xf8,uint8_t(0));
        check(raises([&]{t.driver->recovery_local(t.observed.identity.owner,s,Test::c_proof,&t);},"not paused/drained"),"local recovery bypassed actual scheduler hold");}
    {Test t(true);auto s=t.spec();check(t.driver->inspect(t.observed.identity.owner,s).context,"freeze failed");t.observed.phase=StepNativeObservation::Phase::Loaded;
        ++t.observed.identity.generation;check(raises([&]{t.driver->recovery_local(t.observed.identity.owner,s,Test::c_proof,&t);},"identity changed"),"recovery adopted another selected-file generation");}
    {Test t(true);auto s=t.spec();check(t.driver->inspect(t.observed.identity.owner,s).context,"freeze failed");t.observed.phase=StepNativeObservation::Phase::Loaded;
        s.frozen_peer_mask=1;check(raises([&]{t.driver->recovery_local(t.observed.identity.owner,s,Test::c_proof,&t);},"original Step session or cohort"),"recovery lowered the normal frozen cohort");}
    {Test t(true);auto s=t.spec();check(t.driver->inspect(t.observed.identity.owner,s).context,"freeze failed");t.observed.phase=StepNativeObservation::Phase::Loaded;
        t.put(Test::mode+0x58,uintptr_t(1));check(raises([&]{t.driver->recovery_local(t.observed.identity.owner,s,Test::c_proof,&t);},"GameState/World"),"recovery accepted changed native pointers");}
    {Test t(true);auto s=t.spec();check(t.driver->inspect(t.observed.identity.owner,s).context,"freeze failed");t.observed.phase=StepNativeObservation::Phase::Loaded;
        t.thread=102;check(raises([&]{t.driver->recovery_local(t.observed.identity.owner,s,Test::c_proof,&t);},"driver/thread/owner"),"recovery accepted another game thread");}
    std::cout<<"PASS "<<checks<<" actual recovery native-driver checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
