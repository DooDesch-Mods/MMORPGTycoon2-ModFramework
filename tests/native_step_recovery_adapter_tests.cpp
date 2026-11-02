#define main adapter_baseline_main
#include "native_step_adapter_tests.cpp"
#undef main
#include "native_step_recovery.h"

struct RecoveryAdapterFixture:Fixture {
    MT2FW_MT2_StepRecoveryDraftV1 recovery{};
    MT2FW_MT2_RecoveryReceipt result{sizeof(result)};
    MT2FW_MT2_RecoverySpec watch{};
    bool lost=false,retire_observer=false;
    int local_reads=0,observers=0,recovery_services=0;
    int watch_begins=0,watch_ends=0,adoptions=0;
    RecoveryAdapterFixture(){
        NativeStepAdapter::Driver driver;
        driver.inspect=[this](auto*,const auto&){NativeStepKernel::Proof p;p.context=p.exclusive_routes=p.quiescent=p.bootstrap=true;
            p.world_epoch=native_epoch;p.witness[0]=42;return p;};
        driver.advance=[this](const auto&,auto& r){++originals;r.original_attempted=r.original_returned=r.timer_calls=r.workers_held=1;};
        driver.recovery_local=[this](auto*,const auto& s,auto proof,void* user){
            MT2FW_MT2_StepProofQuery q{sizeof(q),MT2FW_MT2_STEP_DRAFT_VERSION,s.session,s.session_provider_token,7};
            MT2FW_MT2_StepProofReply reply{sizeof(reply)};
            check(proof(user,&q,&reply)==MT2FW_MT2_OK,"scoped recovery local proof failed");};
        driver.recovery={[this](const auto&,const auto&,auto purpose){
            if(purpose==NativeStepRecovery::LocalProof::WatchBegin)++watch_begins;
            else if(purpose==NativeStepRecovery::LocalProof::WatchEnd)++watch_ends;else ++adoptions;
        },[](auto&){},[](const auto&,auto,void*){},[](const auto&){return true;},[](const auto&){}};
        check(adapter.configure(std::move(driver)),"complete recovery driver configure failed");bind();
        check(adapter.query_recovery(&plugin->host,1,&recovery,sizeof(recovery))==MT2FW_MT2_OK,"complete recovery query failed");
        watch.size=sizeof(watch);watch.local_slot=2;watch.lease=receipt.lease;watch.session_provider_token=provider_token;watch.user=this;
        watch.original={{11,22,0,0},{111,222,0,0}};
        watch.observe=[](void* p,const auto*,auto* reply){auto& f=*static_cast<RecoveryAdapterFixture*>(p);++f.observers;
            check(f.plugin->native_pins==1,"recovery observer lacks native pin");reply->observed=f.watch.original;
            if(f.lost)reply->observed.process_nonces[0]=999;
            reply->disposition=f.lost?MT2FW_MT2_COHORT_INTERRUPTED:MT2FW_MT2_COHORT_HEALTHY;
            if(f.retire_observer){MT2FW_MT2_StepReceipt r{sizeof(r)};
                check(f.api.unbind(f.api.context,f.receipt.lease,&r)==MT2FW_MT2_BUSY,"recovery callback storage retired before return");}
            return MT2FW_MT2_OK;};
        watch.local_proof=[](void* p,const auto*,auto*){auto& f=*static_cast<RecoveryAdapterFixture*>(p);++f.local_reads;
            check(f.plugin->native_pins==1,"local proof lacks native pin");return MT2FW_MT2_OK;};
        watch.service=[](void* p,const auto*){auto& f=*static_cast<RecoveryAdapterFixture*>(p);++f.recovery_services;
            check(f.plugin->native_pins==1,"recovery service lacks native pin");return MT2FW_MT2_OK;};
        watch.witness=[](void*,const auto*,unsigned char*,char*,uint32_t){return MT2FW_MT2_UNAVAILABLE;};
        watch.publication=[](void*,const auto*,const auto*,char*,uint32_t){return MT2FW_MT2_UNAVAILABLE;};
    }
    void arm(){check(recovery.bind(recovery.context,&watch,&result)==MT2FW_MT2_OK,"recovery observer bind failed");check(activate()==MT2FW_MT2_OK,"recovery adapter activation failed");}
};
int main(){try{
    {Fixture f;f.configure();f.bind();MT2FW_MT2_StepRecoveryDraftV1 recovery{};
        check(f.adapter.query_recovery(&f.plugin->host,1,&recovery,sizeof(recovery))==MT2FW_MT2_UNAVAILABLE,"partial driver advertised recovery support");}
    {RecoveryAdapterFixture f;auto copy=f.plugin->host;check(f.adapter.query_recovery(&copy,1,&f.recovery,sizeof(f.recovery))==MT2FW_MT2_UNAVAILABLE,"copied Host acquired recovery");}
    {RecoveryAdapterFixture f;check(f.adapter.query_recovery(&f.plugin->host,2,&f.recovery,sizeof(f.recovery))==MT2FW_MT2_INVALID,"wrong recovery draft accepted");}
    {RecoveryAdapterFixture f;f.arm();f.adapter.boundary();check(f.originals==1&&f.observers==1,"healthy wrapper changed normal execution");
        check(f.watch_begins==1&&f.watch_ends==1&&!f.adoptions,"adapter changed healthy WATCH proof purpose");
        f.stale=f.lost=true;f.adapter.boundary();check(f.recovery.read(f.recovery.context,f.receipt.lease,&f.result)==MT2FW_MT2_OK,"local receipt unavailable");
        check(f.result.interruption&&f.recovery_services==1&&f.originals==1&&f.plugin->native_pins==0,"membership-independent recovery lost its actual provider or native pin");}
    {RecoveryAdapterFixture f;f.arm();f.world=false;f.adapter.boundary();check(f.observers==0&&f.adapter.snapshot().phase==MT2FW_MT2_STEP_FAILED,"retired world bypassed recovery provider owner checks");}
    {RecoveryAdapterFixture f;f.arm();f.retire_observer=true;f.adapter.boundary();check(f.originals==1&&f.adapter.snapshot().phase==MT2FW_MT2_STEP_HELD,"observer BUSY retirement corrupted its live lease");}
    {RecoveryAdapterFixture f;f.arm();f.thread=8;f.adapter.boundary();check(f.observers==0&&f.adapter.snapshot().phase==MT2FW_MT2_STEP_FAILED,"wrong-thread recovery observer called");}
    std::cout<<"PASS "<<checks<<" actual recovery adapter checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
