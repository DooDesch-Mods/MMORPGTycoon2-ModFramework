#include "native_step_recovery.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace mt2fw::mt2 {
namespace {
void need(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
bool same(const MT2FW_MT2_RecoveryCohort& a,const MT2FW_MT2_RecoveryCohort& b){return std::memcmp(&a,&b,sizeof(a))==0;}
bool nonzero(const unsigned char* value){return std::any_of(value,value+32,[](auto b){return b!=0;});}
void nonce(const char* value){
    need(value[36]==0,"Recovery nonce is not terminated");
    for(size_t i=0;i<36;++i)need((i==8||i==13||i==18||i==23)?value[i]=='-':
        ((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')),"Recovery nonce is not canonical UUID");
}
void cause_ok(MT2FW_MT2_Result result,const char* cause,size_t bytes,const char* operation){
    need(std::memchr(cause,0,bytes)!=nullptr,std::string(operation)+" cause is not terminated");
    need(result==MT2FW_MT2_OK,std::string(operation)+" failed result="+std::to_string(result)+": "+cause);
}
}
void NativeStepRecovery::copy(MT2FW_MT2_RecoveryReceipt* out)const{*out=receipt_;}
void NativeStepRecovery::fail(const std::string& cause){
    if(receipt_.phase!=MT2FW_MT2_RECOVERY_FAILED){receipt_.phase=MT2FW_MT2_RECOVERY_FAILED;
        const auto n=std::min(cause.size(),sizeof(receipt_.cause)-1);std::memcpy(receipt_.cause,cause.data(),n);receipt_.cause[n]=0;}
    kernel_.failed(cause);
}
MT2FW_MT2_Result NativeStepRecovery::bind(const MT2FW_MT2_RecoverySpec* input,uint8_t mask,uint64_t provider,MT2FW_MT2_RecoveryReceipt* out){
    if(!out||out->size!=sizeof(*out))return MT2FW_MT2_INVALID;
    const auto step=kernel_.snapshot();
    if(bound_)return MT2FW_MT2_BUSY;
    if(!input||input->size!=sizeof(*input)||input->lease!=step.lease||input->session_provider_token!=provider||
       input->local_slot<1||input->local_slot>4||!(mask&(1u<<(input->local_slot-1)))||mask>15||!(mask&1)||
       !input->observe||!input->local_proof||!input->service||!input->witness||!input->publication)return MT2FW_MT2_INVALID;
    if(step.phase!=MT2FW_MT2_STEP_ARMED||step.callbacks_inflight||step.invocation||
       !driver_.local||!driver_.prepare||!driver_.capture||!driver_.pump||!driver_.retire)return MT2FW_MT2_UNAVAILABLE;
    if(uint32_t(input->local_slot==1?MT2FW_MT2_SESSION_HOST:MT2FW_MT2_SESSION_CLIENT)!=step.actual_role)return MT2FW_MT2_INVALID;
    for(unsigned i=0;i<4;++i){const bool present=bool(mask&(1u<<i));
        if(present?(!input->original.physical_ids[i]||!input->original.process_nonces[i]):
                   (input->original.physical_ids[i]||input->original.process_nonces[i]))return MT2FW_MT2_INVALID;
        for(unsigned j=0;j<i;++j)if(present&&(mask&(1u<<j))&&
            (input->original.physical_ids[i]==input->original.physical_ids[j]||input->original.process_nonces[i]==input->original.process_nonces[j]))return MT2FW_MT2_INVALID;
    }
    spec_=*input;receipt_={sizeof(receipt_),MT2FW_MT2_RECOVERY_WATCHING};
    receipt_.lease=step.lease;receipt_.session=step.session;receipt_.thread=step.thread;
    receipt_.original_mask=mask;receipt_.local_slot=input->local_slot;
    receipt_.original=input->original;receipt_.local_process=input->original.process_nonces[input->local_slot-1];
    bound_=true;copy(out);return MT2FW_MT2_OK;
}
void NativeStepRecovery::local(){
    const auto step=kernel_.snapshot();
    need(bound_&&step.lease==receipt_.lease&&step.session==receipt_.session&&step.thread==receipt_.thread&&
        step.phase==MT2FW_MT2_STEP_HELD&&step.workers_held==1&&step.bootstrap_held==1&&step.generation&&step.world_epoch&&
        (interrupted()?(step.completed_tick==receipt_.completed_tick&&step.generation==receipt_.generation&&step.world_epoch==receipt_.world_epoch):true),
        "Recovery local hold lacks its exact healthy retained Step identity");
    kernel_.validate_recovery_boundary(servicing_);
    driver_.local(spec_,step);
}
bool NativeStepRecovery::boundary(){
    if(!bound_)return false;
    try{
        need(!observing_&&!servicing_&&!operating_,"Recovery boundary reentered");local();
        if(!interrupted()){
            const auto step=kernel_.snapshot();
            need(step.completed_tick!=std::numeric_limits<uint64_t>::max(),"Recovery completed tick exhausted");
            receipt_.completed_tick=step.completed_tick;receipt_.next_tick=step.completed_tick+1;
            receipt_.world_epoch=step.world_epoch;receipt_.generation=step.generation;receipt_.workers_held=1;
            MT2FW_MT2_RecoveryObservation observed{sizeof(observed)};
            observing_=true;MT2FW_MT2_Result result;
            try{result=spec_.observe(spec_.user,&receipt_,&observed);}catch(...){observing_=false;throw;}
            observing_=false;cause_ok(result,observed.cause,sizeof(observed.cause),"Recovery transport observation");
            need(observed.size==sizeof(observed),"Recovery observation size differs");local();
            need(kernel_.snapshot().completed_tick==receipt_.completed_tick,"Recovery observer changed the completed tick");
            for(unsigned i=0;i<4;++i){
                need(bool(observed.observed.physical_ids[i])==bool(observed.observed.process_nonces[i]),"Recovery observed partial process identity");
                need(!(receipt_.original_mask&(1u<<i))?(!observed.observed.physical_ids[i]&&!observed.observed.process_nonces[i]):
                    (!observed.observed.physical_ids[i]||observed.observed.physical_ids[i]==receipt_.original.physical_ids[i]),
                    "Recovery observation changed original physical slot");
            }
            const auto slot=receipt_.local_slot-1;
            need(observed.observed.physical_ids[slot]==receipt_.original.physical_ids[slot]&&
                 observed.observed.process_nonces[slot]==receipt_.local_process,"Recovery local original process is absent");
            const bool healthy=same(observed.observed,receipt_.original);
            need(observed.disposition==uint32_t(healthy?MT2FW_MT2_COHORT_HEALTHY:MT2FW_MT2_COHORT_INTERRUPTED),"Recovery disposition contradicts actual process cohort");
            if(healthy)return false;
            need(next_interruption_!=std::numeric_limits<uint64_t>::max(),"Recovery interruption nonce exhausted");
            receipt_.observed=observed.observed;receipt_.interruption=next_interruption_++;
            receipt_.phase=MT2FW_MT2_RECOVERY_INTERRUPTED;
        }else{
            MT2FW_MT2_RecoveryObservation observed{sizeof(observed)};
            observing_=true;MT2FW_MT2_Result result;
            try{result=spec_.observe(spec_.user,&receipt_,&observed);}catch(...){observing_=false;throw;}
            observing_=false;cause_ok(result,observed.cause,sizeof(observed.cause),"Recovery transport maintenance");
            need(observed.size==sizeof(observed)&&
                (observed.disposition==MT2FW_MT2_COHORT_HEALTHY||observed.disposition==MT2FW_MT2_COHORT_INTERRUPTED),
                "Recovery transport maintenance returned an invalid observation");
            local();
            // Observation can change; the attempt's original and survivor sets
            // cannot. MP still requires actual retirement ACKs for the fixed set.
            receipt_.observed=observed.observed;
        }
        // A separate service may perform only explicit recovery operations.
        // The normal service and borrowed Next remain unreachable for this lease.
        servicing_=true;
        try{kernel_.recovery_service([&]{
            const auto result=spec_.service(spec_.user,&receipt_);
            need(result==MT2FW_MT2_OK,"Recovery service failed result="+std::to_string(result));
        });}catch(...){servicing_=false;throw;}
        servicing_=false;
        need(kernel_.snapshot().phase==MT2FW_MT2_STEP_HELD||kernel_.snapshot().phase==MT2FW_MT2_STEP_RETIRING,
            "Recovery service changed the retained native Step hold");
        if(kernel_.snapshot().phase==MT2FW_MT2_STEP_HELD)local();
        return true;
    }catch(const std::exception& e){fail(e.what());return true;}
    catch(...){fail("Recovery boundary raised an unknown exception");return true;}
}
MT2FW_MT2_Result NativeStepRecovery::read(uint64_t lease,MT2FW_MT2_RecoveryReceipt* out){
    if(!out||out->size!=sizeof(*out)||!bound_||lease!=receipt_.lease)return MT2FW_MT2_INVALID;
    MT2FW_MT2_StepReceipt step{sizeof(step)};
    if(kernel_.read_local(lease,&step)!=MT2FW_MT2_OK)return MT2FW_MT2_INVALID;
    copy(out);return MT2FW_MT2_OK;
}
void NativeStepRecovery::witness(unsigned char* value){
    local();char cause[256]{};
    cause_ok(spec_.witness(spec_.user,&receipt_,value,cause,sizeof(cause)),cause,sizeof(cause),"Recovery covered observation");
    need(nonzero(value),"Recovery covered observation is empty");local();
}
MT2FW_MT2_Result NativeStepRecovery::operation(const MT2FW_MT2_RecoveryRequest* request,MT2FW_MT2_SnapshotCaptureFn callback,void* user,MT2FW_MT2_RecoveryReceipt* out){
    if(!out||out->size!=sizeof(*out))return MT2FW_MT2_INVALID;
    if(!bound_||!servicing_||observing_||operating_||!request||request->size!=sizeof(*request)||
       request->lease!=receipt_.lease||request->interruption!=receipt_.interruption||!interrupted()||
       request->reserved[0]||request->reserved[1]||request->operation<1||request->operation>7||
       (request->operation==MT2FW_MT2_RECOVERY_CAPTURE)!=bool(callback)||(!callback&&user))return MT2FW_MT2_INVALID;
    const auto retained_request=*request;request=&retained_request;
    operating_=true;
    try{
        need(receipt_.phase!=MT2FW_MT2_RECOVERY_FAILED,"Recovery attempt retains its original failure");local();nonce(request->recovery_nonce);
        const auto op=request->operation;
        if(op==MT2FW_MT2_RECOVERY_BEGIN){
            need(receipt_.phase==MT2FW_MT2_RECOVERY_INTERRUPTED&&!receipt_.survivor_mask,"Recovery BEGIN is repeated");
            const auto mask=request->survivor_mask;
            need(mask&&(mask&1)&&(mask&(1u<<(receipt_.local_slot-1)))&&!(mask&~receipt_.original_mask),"Recovery survivor cohort is invalid");
            for(unsigned i=0;i<4;++i)need(mask&(1u<<i)?
                (request->survivors.physical_ids[i]==receipt_.original.physical_ids[i]&&request->survivors.process_nonces[i]==receipt_.original.process_nonces[i]&&
                 request->survivors.physical_ids[i]==receipt_.observed.physical_ids[i]&&request->survivors.process_nonces[i]==receipt_.observed.process_nonces[i]):
                (!request->survivors.physical_ids[i]&&!request->survivors.process_nonces[i]),"Recovery survivor is not an observed original process");
            // Every observed original survivor is part of this attempt.
            for(unsigned i=0;i<4;++i)if((receipt_.original_mask&(1u<<i))&&receipt_.observed.process_nonces[i]==receipt_.original.process_nonces[i])
                need(mask&(1u<<i),"Recovery cannot omit an observed surviving original process");
            receipt_.survivor_mask=mask;receipt_.survivors=request->survivors;
            std::memcpy(receipt_.recovery_nonce,request->recovery_nonce,37);
        }else{
            need(receipt_.survivor_mask&&request->survivor_mask==receipt_.survivor_mask&&same(request->survivors,receipt_.survivors)&&
                std::memcmp(request->recovery_nonce,receipt_.recovery_nonce,37)==0,"Recovery attempt nonce or frozen survivor cohort changed");
            const bool host=kernel_.snapshot().actual_role==MT2FW_MT2_SESSION_HOST;
            if(op==MT2FW_MT2_RECOVERY_PREPARE){
                need(host&&receipt_.local_slot==1&&receipt_.phase==MT2FW_MT2_RECOVERY_INTERRUPTED&&!receipt_.prepare_attempted,"Recovery preparation is not an unused Host attempt");
                receipt_.prepare_attempted=1;receipt_.phase=MT2FW_MT2_RECOVERY_PREPARING;
                driver_.prepare(receipt_);local();receipt_.prepare_returned=1;receipt_.phase=MT2FW_MT2_RECOVERY_PREPARED;
            }else if(op==MT2FW_MT2_RECOVERY_SEAL){
                need(host&&receipt_.phase==MT2FW_MT2_RECOVERY_PREPARED,"Recovery SEAL lacks completed preparation");
                witness(receipt_.witness_sha256);receipt_.phase=MT2FW_MT2_RECOVERY_SEALED;
            }else if(op==MT2FW_MT2_RECOVERY_CAPTURE){
                need(host&&receipt_.phase==MT2FW_MT2_RECOVERY_SEALED&&!receipt_.capture_attempted,"Recovery CAPTURE is already consumed or not sealed");
                unsigned char observed[32]{};witness(observed);need(std::memcmp(observed,receipt_.witness_sha256,32)==0,"Recovery covered state changed before capture");
                receipt_.capture_attempted=1;receipt_.phase=MT2FW_MT2_RECOVERY_CONSUMED;
                driver_.capture(receipt_,callback,user);
            }else if(op==MT2FW_MT2_RECOVERY_PUMP){
                need(host&&receipt_.phase==MT2FW_MT2_RECOVERY_CONSUMED,"Recovery PUMP has no consumed original writer");
                if(driver_.pump(receipt_)){
                    unsigned char observed[32]{};witness(observed);need(std::memcmp(observed,receipt_.witness_sha256,32)==0,"Recovery covered state changed after capture");
                    receipt_.capture_returned=1;receipt_.phase=MT2FW_MT2_RECOVERY_CAPTURED;
                }
            }else if(op==MT2FW_MT2_RECOVERY_PUBLISH){
                const auto& p=request->publication;
                need(host&&receipt_.phase==MT2FW_MT2_RECOVERY_CAPTURED&&p.size==sizeof(p)&&!p.reserved&&p.file_bytes&&p.companion_bytes&&
                    nonzero(p.file_sha256)&&nonzero(p.companion_sha256),"Recovery publication lacks completed full file and companion");
                char cause[256]{};cause_ok(spec_.publication(spec_.user,&receipt_,&p,cause,sizeof(cause)),cause,sizeof(cause),"Recovery publication");
                local();receipt_.file_bytes=p.file_bytes;receipt_.companion_bytes=p.companion_bytes;
                std::memcpy(receipt_.file_sha256,p.file_sha256,32);std::memcpy(receipt_.companion_sha256,p.companion_sha256,32);
                receipt_.phase=MT2FW_MT2_RECOVERY_PUBLISHED;
            }else{
                need(op==MT2FW_MT2_RECOVERY_PREFLIGHT_RETIRE&&
                    (host?receipt_.phase==MT2FW_MT2_RECOVERY_PUBLISHED:receipt_.phase==MT2FW_MT2_RECOVERY_INTERRUPTED),
                    "Recovery retirement lacks Host publication or original Client hold");
                driver_.retire(receipt_);receipt_.phase=MT2FW_MT2_RECOVERY_RETIRE_READY;
            }
        }
        copy(out);operating_=false;return MT2FW_MT2_OK;
    }catch(const std::exception& e){fail(e.what());}
    catch(...){fail("Recovery operation raised an unknown exception");}
    copy(out);operating_=false;return MT2FW_MT2_FAILED;
}
}
