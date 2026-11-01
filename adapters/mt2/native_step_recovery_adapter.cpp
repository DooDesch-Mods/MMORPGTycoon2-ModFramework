#include "native_step_adapter.h"

namespace mt2fw::mt2 {
MT2FW_MT2_Result NativeStepAdapter::query_recovery(const MT2FW_Host* owner,uint32_t version,MT2FW_MT2_StepRecoveryDraftV1* out,uint32_t capacity){
    if(!out||capacity<sizeof(*out)||version!=MT2FW_MT2_STEP_RECOVERY_DRAFT_VERSION)return MT2FW_MT2_INVALID;
    if(owner!=host_||!current()||callbacks_retired_||lab_()||!kernel_||!driver_.recovery_local||!driver_.recovery.local||
        !driver_.recovery.prepare||!driver_.recovery.capture||!driver_.recovery.pump||!driver_.recovery.retire)return MT2FW_MT2_UNAVAILABLE;
    if(!recovery_){
        auto driver=driver_.recovery;
        driver.local=[this](const MT2FW_MT2_RecoverySpec& spec,const MT2FW_MT2_StepReceipt& step){
            uint64_t epoch=0;std::string cause;
            if(!current()||callbacks_retired_||lab_()||proof_inflight_||snapshot_inflight_||save_inflight_||boundary_inflight_||
                !patches_.inspect_step_provider_owner(*plugin_,spec.session_provider_token,epoch,cause)||epoch!=framework_epoch_)
                throw std::runtime_error("Recovery local provider/native world owner differs: "+cause);
            if(step.world_epoch!=native_epoch_)throw std::runtime_error("Recovery NativeOpen epoch changed");
            driver_.recovery_local(host_,callbacks_,recovery_proof,this);driver_.recovery.local(spec,step);
            if(!current()||callbacks_retired_||!patches_.inspect_step_provider_owner(*plugin_,spec.session_provider_token,epoch,cause)||epoch!=framework_epoch_)
                throw std::runtime_error("Recovery native owner retired during local proof: "+cause);
        };
        driver.capture=[this](const auto& receipt,auto callback,void* user){
            struct Capture {NativeStepAdapter* adapter;MT2FW_MT2_SnapshotCaptureFn callback;void* user;} capture{this,callback,user};
            driver_.recovery.capture(receipt,[](void* context,char* cause,uint32_t bytes){auto& c=*static_cast<Capture*>(context);
                return c.adapter->recovery_pinned([&]{return c.callback(c.user,cause,bytes);});},&capture);
        };
        recovery_=std::make_unique<NativeStepRecovery>(*kernel_,std::move(driver));
        kernel_->recovery_boundary([this]{return recovery_&&recovery_->active()?recovery_->boundary():false;});
    }
    *out={sizeof(*out),version,this,
        [](void* context,const MT2FW_MT2_RecoverySpec* input,MT2FW_MT2_RecoveryReceipt* receipt){
            if(!context||!input||!receipt||receipt->size!=sizeof(*receipt))return MT2FW_MT2_INVALID;
            auto& a=*static_cast<NativeStepAdapter*>(context);
            if(!a.current()||a.callbacks_retired_||a.proof_inflight_||a.service_inflight_||a.boundary_inflight_||!a.recovery_)return MT2FW_MT2_UNAVAILABLE;
            if(a.recovery_->active())return MT2FW_MT2_BUSY;
            auto proxy=*input;a.recovery_callbacks_=*input;proxy.user=&a;
            proxy.observe=[](void* p,const auto* r,auto* o){auto& a=*static_cast<NativeStepAdapter*>(p);
                return a.recovery_pinned([&]{return a.recovery_callbacks_.observe(a.recovery_callbacks_.user,r,o);});};
            proxy.service=[](void* p,const auto* r){auto& a=*static_cast<NativeStepAdapter*>(p);
                return a.recovery_pinned([&]{return a.recovery_callbacks_.service(a.recovery_callbacks_.user,r);});};
            proxy.witness=[](void* p,const auto* r,unsigned char* value,char* cause,uint32_t bytes){auto& a=*static_cast<NativeStepAdapter*>(p);
                return a.recovery_pinned([&]{return a.recovery_callbacks_.witness(a.recovery_callbacks_.user,r,value,cause,bytes);});};
            proxy.publication=[](void* p,const auto* r,const auto* pub,char* cause,uint32_t bytes){auto& a=*static_cast<NativeStepAdapter*>(p);
                return a.recovery_pinned([&]{return a.recovery_callbacks_.publication(a.recovery_callbacks_.user,r,pub,cause,bytes);});};
            // Validate original callbacks before replacement by pinned thunks.
            if(!input->observe||!input->local_proof||!input->service||!input->witness||!input->publication)return MT2FW_MT2_INVALID;
            return a.recovery_->bind(&proxy,a.callbacks_.frozen_peer_mask,a.callbacks_.session_provider_token,receipt);
        },
        [](void* context,uint64_t lease,MT2FW_MT2_RecoveryReceipt* receipt){
            if(!context)return MT2FW_MT2_INVALID;auto& a=*static_cast<NativeStepAdapter*>(context);
            if(!a.current()||a.callbacks_retired_||!a.recovery_)return MT2FW_MT2_UNAVAILABLE;
            return a.recovery_->read(lease,receipt);
        },
        [](void* context,const MT2FW_MT2_RecoveryRequest* request,MT2FW_MT2_SnapshotCaptureFn callback,void* user,MT2FW_MT2_RecoveryReceipt* receipt){
            if(!context)return MT2FW_MT2_INVALID;auto& a=*static_cast<NativeStepAdapter*>(context);
            if(!a.current()||a.callbacks_retired_||!a.recovery_)return MT2FW_MT2_UNAVAILABLE;
            return a.recovery_->operation(request,callback,user,receipt);
        }};
    return MT2FW_MT2_OK;
}
MT2FW_MT2_Result MT2FW_CALL NativeStepAdapter::recovery_proof(void* context,const MT2FW_MT2_StepProofQuery* query,MT2FW_MT2_StepProofReply* reply){
    if(!context||!query||!reply)return MT2FW_MT2_INVALID;auto& a=*static_cast<NativeStepAdapter*>(context);
    if(!a.current()||a.callbacks_retired_||a.proof_inflight_||!a.recovery_||!a.recovery_->active()||!a.recovery_callbacks_.local_proof||
        query->size!=sizeof(*query)||query->version!=MT2FW_MT2_STEP_DRAFT_VERSION||query->session!=a.callbacks_.session||
        query->session_provider_token!=a.callbacks_.session_provider_token||query->thread!=a.bound_()||reply->size!=sizeof(*reply))return MT2FW_MT2_INVALID;
    a.proof_inflight_=true;struct Clear{bool& value;~Clear(){value=false;}}clear{a.proof_inflight_};
    try{return a.recovery_pinned([&]{return a.recovery_callbacks_.local_proof(a.recovery_callbacks_.user,query,reply);});}
    catch(const std::exception& e){const auto n=std::min(std::strlen(e.what()),sizeof(reply->cause)-1);std::memcpy(reply->cause,e.what(),n);reply->cause[n]=0;return MT2FW_MT2_FAILED;}
    catch(...){constexpr char cause[]="Recovery local proof raised an unknown exception";std::memcpy(reply->cause,cause,sizeof(cause));return MT2FW_MT2_FAILED;}
}
}
