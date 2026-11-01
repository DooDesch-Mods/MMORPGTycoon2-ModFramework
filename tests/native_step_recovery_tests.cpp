#define main step_baseline_main
#include "native_step_provider_tests.cpp"
#undef main
#include "native_step_recovery.h"
#include <functional>

struct RecoveryFixture:Fixture {
    NativeStepRecovery recovery;
    MT2FW_MT2_RecoverySpec watch{};
    MT2FW_MT2_RecoveryReceipt result{sizeof(result)};
    MT2FW_MT2_RecoveryRequest request{};
    MT2FW_MT2_RecoveryCohort observed{};
    std::function<void()> service_action;
    int observations=0,local_proofs=0,prepares=0,captures=0,pumps=0,publications=0,retires=0;
    bool bad_local=false,throw_prepare=false,throw_capture=false,pump_done=true,bad_publication=false;
    unsigned char covered=27;
    RecoveryFixture(unsigned mask=3,unsigned local=1):recovery(kernel,{
        [this](const auto&,const auto&){++local_proofs;if(bad_local)throw std::runtime_error("actual local native hold unavailable");},
        [this](auto&){++prepares;if(throw_prepare)throw std::runtime_error("original recovery flush failed");},
        [this](const auto&,auto callback,void* user){++captures;char cause[256]{};
            if(throw_capture)throw std::runtime_error("original Save unknown");
            check(callback(user,cause,sizeof(cause))==MT2FW_MT2_OK,"capture callback failed");},
        [this](const auto&){++pumps;return pump_done;},[this](const auto&){++retires;}}){
        proof.role=local==1?MT2FW_MT2_SESSION_HOST:MT2FW_MT2_SESSION_CLIENT;spec.frozen_peer_mask=uint8_t(mask);
        check(bind()==MT2FW_MT2_OK,"recovery step bind failed");
        watch.size=sizeof(watch);watch.local_slot=local;watch.lease=receipt.lease;watch.session_provider_token=22;watch.user=this;
        for(unsigned i=0;i<4;++i)if(mask&(1u<<i)){watch.original.physical_ids[i]=100+i;watch.original.process_nonces[i]=1000+i;}
        observed=watch.original;
        watch.observe=[](void* p,const auto*,auto* out){auto& f=*static_cast<RecoveryFixture*>(p);++f.observations;
            out->observed=f.observed;out->disposition=std::memcmp(&f.observed,&f.watch.original,sizeof(f.observed))?
                MT2FW_MT2_COHORT_INTERRUPTED:MT2FW_MT2_COHORT_HEALTHY;return MT2FW_MT2_OK;};
        watch.local_proof=spec.proof;
        watch.service=[](void* p,const auto*){auto& f=*static_cast<RecoveryFixture*>(p);if(f.service_action)f.service_action();return MT2FW_MT2_OK;};
        watch.witness=[](void* p,const auto*,unsigned char* out,char*,uint32_t){out[0]=static_cast<RecoveryFixture*>(p)->covered;return MT2FW_MT2_OK;};
        watch.publication=[](void* p,const auto*,const auto*,char* cause,uint32_t){auto& f=*static_cast<RecoveryFixture*>(p);++f.publications;
            if(f.bad_publication){std::memcpy(cause,"companion readback failed",sizeof("companion readback failed"));return MT2FW_MT2_FAILED;}return MT2FW_MT2_OK;};
    }
    void arm(){check(recovery.bind(&watch,spec.frozen_peer_mask,22,&result)==MT2FW_MT2_OK,"recovery bind unavailable");
        kernel.recovery_boundary([this]{return recovery.boundary();});check(activate()==MT2FW_MT2_OK,"recovery activation failed");}
    void interrupt(unsigned peer=2){observed.physical_ids[peer-1]=observed.process_nonces[peer-1]=0;proof.context=false;
        kernel.boundary();check(recovery.read(receipt.lease,&result)==MT2FW_MT2_OK,"recovery read failed");}
    void begin(unsigned mask=1){request={};request.size=sizeof(request);request.lease=receipt.lease;request.interruption=result.interruption;
        request.operation=MT2FW_MT2_RECOVERY_BEGIN;request.survivor_mask=uint8_t(mask);
        std::memcpy(request.recovery_nonce,"12345678-1234-1234-1234-123456789abc",37);
        for(unsigned i=0;i<4;++i)if(mask&(1u<<i)){request.survivors.physical_ids[i]=watch.original.physical_ids[i];request.survivors.process_nonces[i]=watch.original.process_nonces[i];}
        run(MT2FW_MT2_RECOVERY_BEGIN);
    }
    MT2FW_MT2_Result last=MT2FW_MT2_UNAVAILABLE;
    void run(unsigned op){request.operation=op;service_action=[this,op]{
        last=recovery.operation(&request,op==MT2FW_MT2_RECOVERY_CAPTURE?+[](void*,char*,uint32_t){return MT2FW_MT2_OK;}:nullptr,nullptr,&result);};
        kernel.boundary();service_action={};}
    void capture(){begin();run(MT2FW_MT2_RECOVERY_PREPARE);run(MT2FW_MT2_RECOVERY_SEAL);run(MT2FW_MT2_RECOVERY_CAPTURE);}
    void publish(){run(MT2FW_MT2_RECOVERY_PUMP);request.publication={sizeof(request.publication),0,100,200,{1},{2}};run(MT2FW_MT2_RECOVERY_PUBLISH);}
};
int main(){try{
    {RecoveryFixture f;f.arm();f.kernel.boundary();check(f.originals==1&&f.services==1&&f.observations==1,"healthy recovery changed the normal Step flow");
        f.interrupt();check(f.kernel.snapshot().phase==MT2FW_MT2_STEP_HELD&&f.result.interruption&&f.result.completed_tick==1&&f.result.next_tick==2&&
            f.kernel.snapshot().original_attempted==1&&f.kernel.snapshot().original_returned==1,"fresh interruption confused previous original flags");
        for(int i=0;i<3;++i)f.kernel.boundary();check(f.originals==1&&f.services==1,"interrupted lease resumed original flow");}
    {RecoveryFixture f;f.arm();f.interrupt();check(f.result.completed_tick==0&&f.result.next_tick==1&&f.result.workers_held==1,"zero-tick interruption unavailable");
        f.capture();check(f.prepares==1&&f.captures==1&&f.result.phase==MT2FW_MT2_RECOVERY_CONSUMED,"single-use recovery capture unavailable");
        f.pump_done=false;f.run(MT2FW_MT2_RECOVERY_PUMP);check(!f.result.capture_returned&&f.result.phase==MT2FW_MT2_RECOVERY_CONSUMED,"incomplete writer reported complete");
        f.pump_done=true;f.publish();check(f.result.phase==MT2FW_MT2_RECOVERY_PUBLISHED&&f.publications==1,"full file/companion publication unavailable");
        f.run(MT2FW_MT2_RECOVERY_PREFLIGHT_RETIRE);check(f.retires==1&&f.result.phase==MT2FW_MT2_RECOVERY_RETIRE_READY&&f.originals==0,"retirement granted simulation permission");}
    {RecoveryFixture f;f.arm();f.observed.process_nonces[1]=9999;f.proof.context=false;f.kernel.boundary();f.recovery.read(f.receipt.lease,&f.result);
        check(f.result.phase==MT2FW_MT2_RECOVERY_INTERRUPTED&&f.result.original.process_nonces[1]==1001&&f.result.observed.process_nonces[1]==9999,"same-account restart replaced original process identity");f.begin();check(f.last==MT2FW_MT2_OK,"restarted original process counted as survivor");}
    {RecoveryFixture f(15);f.arm();f.interrupt(2);f.begin(13);check(f.last==MT2FW_MT2_OK&&f.result.survivor_mask==13,"four-peer survivor cohort unavailable");}
    {RecoveryFixture f(15);f.arm();f.interrupt(2);f.begin(1);check(f.last==MT2FW_MT2_FAILED&&f.prepares==0,"surviving original processes silently omitted");}
    {RecoveryFixture f;f.watch.original.process_nonces[1]=f.watch.original.process_nonces[0];check(f.recovery.bind(&f.watch,3,22,&f.result)==MT2FW_MT2_INVALID,"duplicate original process identity accepted");}
    {RecoveryFixture f;f.arm();f.kernel.failed("post-original witness unknown");f.kernel.boundary();check(f.observations==0&&f.kernel.snapshot().phase==MT2FW_MT2_STEP_FAILED,"FAILED reclassified as Recovery hold");}
    {RecoveryFixture f;f.arm();f.thread=8;f.kernel.boundary();check(f.observations==0&&f.kernel.snapshot().phase==MT2FW_MT2_STEP_FAILED,"wrong thread reached recovery observer");}
    {RecoveryFixture f;f.arm();f.bad_local=true;f.kernel.boundary();check(f.observations==0&&f.kernel.snapshot().phase==MT2FW_MT2_STEP_FAILED,"unknown native hold reached transport observer");}
    {RecoveryFixture f;f.arm();f.throw_advance=true;f.kernel.boundary();f.interrupt();check(!f.result.interruption&&f.originals==1&&f.kernel.snapshot().phase==MT2FW_MT2_STEP_FAILED,"uncommitted original effect was salvaged");}
    {RecoveryFixture f;f.arm();f.interrupt();f.begin();f.throw_prepare=true;f.run(MT2FW_MT2_RECOVERY_PREPARE);
        check(f.last==MT2FW_MT2_FAILED&&f.prepares==1&&f.result.prepare_attempted==1&&!f.result.prepare_returned,"failed preparation lost its consumed attempt");
        f.run(MT2FW_MT2_RECOVERY_PREPARE);check(f.prepares==1&&f.kernel.snapshot().phase==MT2FW_MT2_STEP_FAILED,"failed preparation replayed");}
    {RecoveryFixture f;f.arm();f.interrupt();f.capture();f.run(MT2FW_MT2_RECOVERY_CAPTURE);check(f.last==MT2FW_MT2_FAILED&&f.captures==1,"original Save replayed");}
    {RecoveryFixture f;f.arm();f.interrupt();f.capture();f.covered=28;f.run(MT2FW_MT2_RECOVERY_PUMP);
        check(f.last==MT2FW_MT2_FAILED&&!f.result.capture_returned,"changed post-capture witness acknowledged");}
    {RecoveryFixture f;f.arm();f.interrupt();f.capture();f.bad_publication=true;f.publish();
        check(f.last==MT2FW_MT2_FAILED&&f.result.capture_returned==1&&std::strstr(f.result.cause,"companion"),"companion failure hidden or capture repeated");}
    {RecoveryFixture f;f.arm();f.interrupt();f.begin();f.run(MT2FW_MT2_RECOVERY_PREFLIGHT_RETIRE);check(f.last==MT2FW_MT2_FAILED&&f.retires==0,"Host retired before fresh full capture");}
    {RecoveryFixture f(15,3);f.arm();f.interrupt(2);f.begin(13);f.run(MT2FW_MT2_RECOVERY_PREFLIGHT_RETIRE);
        check(f.retires==1&&f.prepares==0&&f.captures==0,"surviving original Client cannot retire its real hold");}
    {RecoveryFixture f;f.arm();f.interrupt();f.begin();f.request.survivor_mask=3;f.run(MT2FW_MT2_RECOVERY_PREPARE);
        check(f.last==MT2FW_MT2_FAILED&&f.prepares==0,"recovery survivor set changed mid-attempt");}
    {RecoveryFixture f;f.arm();f.interrupt();f.begin();f.request.interruption++;f.run(MT2FW_MT2_RECOVERY_PREPARE);
        check(f.last==MT2FW_MT2_INVALID&&f.prepares==0,"stale interruption nonce accepted");}
    {RecoveryFixture f;f.arm();f.interrupt();f.service_action=[&]{f.kernel.boundary();};f.kernel.boundary();
        check(f.kernel.snapshot().phase==MT2FW_MT2_STEP_FAILED&&f.originals==0,"recovery service reentered original execution");}
    std::cout<<"PASS "<<checks<<" actual kernel/recovery checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
