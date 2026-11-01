#include "native_step_adapter.h"
#include <stdexcept>

namespace mt2fw::mt2 {
bool NativeStepAdapter::reject(const std::string& cause,const char* level){
    if(last_reason_!=cause){runtime_.log("steps",level,cause);last_reason_=cause;}return false;
}
bool NativeStepAdapter::current()const{
    return plugin_&&host_&&runtime_.native_plugin(host_)==plugin_&&plugin_->manifest.id=="doodesch.multiplayer"&&
        plugin_->manifest.type=="native"&&thread_()==bound_()&&bound_()!=0;
}
bool NativeStepAdapter::configure(Driver driver){
    if(retained()||proof_inflight_||lab_())return reject("Native step driver conflicts with retained lease, proof callback or private lab owner");
    if(!driver.inspect||!driver.advance)return reject("Native step Go/Timer/bootstrap driver is incomplete");
    driver_=std::move(driver);last_reason_.clear();return true;
}
MT2FW_MT2_Result NativeStepAdapter::query(const MT2FW_Host* host,uint32_t version,MT2FW_MT2_StepServiceDraftV1* out,uint32_t capacity){
    if(!out||capacity<sizeof(*out)||version!=MT2FW_MT2_STEP_DRAFT_VERSION)return MT2FW_MT2_INVALID;
    auto plugin=runtime_.native_plugin(host);
    if(!plugin||plugin->manifest.id!="doodesch.multiplayer"||plugin->manifest.type!="native"||!bound_()||thread_()!=bound_())return MT2FW_MT2_UNAVAILABLE;
    if(lab_()){reject("Native step discovery rejected: private lab already owns Go/Timer");return MT2FW_MT2_UNAVAILABLE;}
    if(!driver_.inspect||!driver_.advance){reject("Native step driver is not configured; bootstrap/route handoff remains unavailable","info");return MT2FW_MT2_UNAVAILABLE;}
    if(plugin_&&plugin_!=plugin)return MT2FW_MT2_BUSY;
    plugin_=plugin;host_=host;
    if(!kernel_){
        kernel_=std::make_unique<NativeStepKernel>(host,[this](const MT2FW_Host* owner,const MT2FW_MT2_StepSpec& spec){
            if(callbacks_retired_&&driver_.exited){auto exited=driver_.exited();if(exited.world_exited)return exited;}
            NativeStepKernel::Proof proof;proof.owner=current();proof.thread=bound_();proof.lab_gate=lab_();
            uint64_t epoch=0;uint32_t role=MT2FW_MT2_SESSION_UNKNOWN;std::string cause;
            proof.provider=proof.owner&&patches_.inspect_step_provider(*plugin_,spec.session_provider_token,epoch,role,cause);
            if(!proof.provider){proof.cause=cause.empty()?"Native step supported owner is not current":cause;return proof;}
            proof=driver_.inspect(owner,spec);proof.owner=current();proof.thread=bound_();proof.lab_gate=lab_();proof.provider=true;
            if(proof.bootstrap&&proof.context){
                // Framework counts load and leave; NativeOpen counts loaded worlds.
                // Pin both independently after their existing current-world proofs.
                if(!epoch||!proof.world_epoch){
                    proof.context=false;proof.bootstrap=false;proof.cause="Step loaded proof lacks its Framework or NativeOpen epoch";
                }else if(framework_epoch_&&(framework_epoch_!=epoch||native_epoch_!=proof.world_epoch)){
                    proof.context=false;proof.bootstrap=false;proof.cause="Step world epoch pair changed: Framework="+
                        std::to_string(epoch)+" expected="+std::to_string(framework_epoch_)+" NativeOpen="+
                        std::to_string(proof.world_epoch)+" expected="+std::to_string(native_epoch_);
                }else{framework_epoch_=epoch;native_epoch_=proof.world_epoch;}
            }
            proof.role=role;return proof;
        },[this](const auto& frame,auto& receipt){
            if(!current())throw std::runtime_error("Native step owner retired before original entry");
            driver_.advance(frame,receipt);
        },thread_);
        if(kernel_->query(host,version,&api_,sizeof(api_))!=MT2FW_MT2_OK)throw std::runtime_error("Native step kernel discovery failed");
    }
    *out={sizeof(*out),version,this,
        [](void* p,const MT2FW_MT2_StepSpec* s,MT2FW_MT2_StepReceipt* r){if(!p)return MT2FW_MT2_INVALID;auto& a=*static_cast<NativeStepAdapter*>(p);return a.guard([&]{return a.bind(s,r);},r);},
        [](void* p,const MT2FW_MT2_StepActivation* s,MT2FW_MT2_StepReceipt* r){if(!p)return MT2FW_MT2_INVALID;auto& a=*static_cast<NativeStepAdapter*>(p);return a.guard([&]{if(!a.current())return MT2FW_MT2_UNAVAILABLE;return a.api_.activate(a.api_.context,s,r);},r);},
        [](void* p,uint64_t lease,MT2FW_MT2_StepReceipt* r){if(!p)return MT2FW_MT2_INVALID;auto& a=*static_cast<NativeStepAdapter*>(p);return a.guard([&]{return a.api_.read(a.api_.context,lease,r);},r);},
        [](void* p,uint64_t lease,MT2FW_MT2_StepReceipt* r){if(!p)return MT2FW_MT2_INVALID;auto& a=*static_cast<NativeStepAdapter*>(p);return a.guard([&]{if(a.save_inflight_||a.recovery_callbacks_inflight_)return MT2FW_MT2_BUSY;auto result=a.api_.unbind(a.api_.context,lease,r);if(result==MT2FW_MT2_OK){a.callbacks_retired_=true;if(a.recovery_)a.recovery_->detach();}return result;},r);},
        [](void* p,uint64_t lease,MT2FW_MT2_StepReceipt* r){
            if(!p||!r||r->size!=sizeof(*r))return MT2FW_MT2_INVALID;
            auto& a=*static_cast<NativeStepAdapter*>(p);
            a.exit_preflight_ready_=false;a.exit_preflight_lease_=lease;a.exit_preflight_phase_=0;a.exit_preflight_cause_.clear();
            const auto result=a.guard([&]{
                if(!a.current()||a.callbacks_retired_||!a.driver_.preflight_exit)return MT2FW_MT2_UNAVAILABLE;
                const auto result=a.api_.read(a.api_.context,lease,r);
                if(result!=MT2FW_MT2_OK)return result;
                a.kernel_->validate_native_exit_request();a.driver_.preflight_exit();return MT2FW_MT2_OK;
            },r);
            a.exit_preflight_ready_=result==MT2FW_MT2_OK;a.exit_preflight_phase_=r->phase;
            if(result!=MT2FW_MT2_OK)a.exit_preflight_cause_=r->cause[0]?r->cause:"Step exit preflight unavailable result="+std::to_string(result);
            return result;
        },
        [](void* p,const MT2FW_MT2_SnapshotRequest* request,MT2FW_MT2_SnapshotCaptureFn callback,void* user,MT2FW_MT2_StepReceipt* out){
            if(!p)return MT2FW_MT2_INVALID;return static_cast<NativeStepAdapter*>(p)->snapshot(request,callback,user,out);
        },
        [](void* p,uint64_t lease,uint32_t operation,const char* nonce,MT2FW_MT2_StepReceipt* r){
            if(!p||!r||r->size!=sizeof(*r)||!nonce)return MT2FW_MT2_INVALID;
            auto& a=*static_cast<NativeStepAdapter*>(p);
            return a.guard([&]{
                if(!a.current()||a.callbacks_retired_||a.save_inflight_||a.snapshot_inflight_||a.proof_inflight_||!a.driver_.exit_save)
                    return MT2FW_MT2_UNAVAILABLE;
                const auto length=strnlen(nonce,37);if(length!=0&&length!=36)return MT2FW_MT2_INVALID;
                const auto result=a.api_.read(a.api_.context,lease,r);if(result!=MT2FW_MT2_OK)return result;
                a.kernel_->validate_native_exit_request();a.driver_.exit_save(operation,std::string(nonce,length));
                return MT2FW_MT2_OK;
            },r);
        }};
    return MT2FW_MT2_OK;
}
MT2FW_MT2_Result NativeStepAdapter::snapshot(const MT2FW_MT2_SnapshotRequest* request,
    MT2FW_MT2_SnapshotCaptureFn callback,void* user,MT2FW_MT2_StepReceipt* out){
    return guard([&]{
        if(!current()||callbacks_retired_||proof_inflight_||snapshot_inflight_||!driver_.snapshot||!kernel_)
            throw std::runtime_error("Snapshot bridge lacks its current non-reentrant owner");
        if(!request||request->size!=sizeof(*request)||request->reserved||request->peer_mask!=callbacks_.frozen_peer_mask||
           request->operation<MT2FW_MT2_SNAPSHOT_ARM||request->operation>MT2FW_MT2_SNAPSHOT_CLIENT_CACHE)
            throw std::runtime_error("Snapshot bridge request shape or frozen peer mask differs");
        const auto result=api_.read(api_.context,request->lease,out);
        if(result!=MT2FW_MT2_OK)return result;
        if(out->session!=request->session||out->world_epoch!=request->world_epoch||out->thread!=bound_()||
           (out->phase!=MT2FW_MT2_STEP_HELD&&!(request->operation==MT2FW_MT2_SNAPSHOT_PUMP&&out->phase==MT2FW_MT2_STEP_FAILED))||out->workers_held!=1||out->callbacks_inflight!=1)
            throw std::runtime_error("Snapshot bridge current session/world/held receipt differs");
        const bool arm=request->operation==MT2FW_MT2_SNAPSHOT_ARM;
        if(arm?(!boundary_inflight_||service_inflight_||out->tick!=request->tick||!request->invocation||
                out->invocation!=request->invocation||out->original_attempted||out->original_returned):
               (!service_inflight_||boundary_inflight_||request->invocation||out->completed_tick!=request->tick))
            throw std::runtime_error("Snapshot bridge does not own the exact boundary or completed-tick service");
        if((request->operation==MT2FW_MT2_SNAPSHOT_CAPTURE)!=bool(callback)||
           ((request->operation==MT2FW_MT2_SNAPSHOT_CAPTURE||request->operation==MT2FW_MT2_SNAPSHOT_PUMP)&&out->actual_role!=MT2FW_MT2_SESSION_HOST))
            throw std::runtime_error("Snapshot bridge callback or Host role differs");
        if(request->operation==MT2FW_MT2_SNAPSHOT_CLIENT_CACHE&&out->actual_role!=MT2FW_MT2_SESSION_CLIENT)
            throw std::runtime_error("Snapshot local cache requires the sealed Client owner");
        snapshot_inflight_=true;
        struct Clear{bool& value;~Clear(){value=false;}}clear{snapshot_inflight_};
        try{driver_.snapshot(*request,callback,user);}
        catch(const std::exception& e){kernel_->failed(e.what());throw;}
        catch(...){kernel_->failed("Snapshot operation raised unknown native exception");throw;}
        return MT2FW_MT2_OK;
    },out);
}
bool NativeStepAdapter::save_request(void* mode,uint32_t kind,const char* filename,uint32_t bytes,bool option,uintptr_t caller_rva){
    if(!kernel_||!retained())return false;
    try{
        if(!current()||callbacks_retired_||proof_inflight_||snapshot_inflight_||save_inflight_||!callbacks_.save_request)
            throw std::runtime_error("Native Save request lacks its non-reentrant current owner");
        if(!mode||!caller_rva||(kind!=MT2FW_MT2_SAVE_ASYNC&&kind!=MT2FW_MT2_SAVE_BLOCKING&&kind!=MT2FW_MT2_SAVE_PERIODIC&&kind!=MT2FW_MT2_SAVE_QUIT)||
            !filename||!bytes||bytes>4096||std::memchr(filename,0,bytes))throw std::runtime_error("Native Save request shape differs");
        kernel_->validate_save_request();const auto before=kernel_->snapshot();
        MT2FW_MT2_SaveRequest call{sizeof(call),kind,uint32_t(option),0,before.lease,before.session,before.world_epoch,
            before.tick,before.invocation,before.thread,reinterpret_cast<uintptr_t>(mode),caller_rva,filename,bytes,0};
        save_inflight_=true;struct Clear{bool& value;~Clear(){value=false;}}clear{save_inflight_};char cause[1024]{};
        const auto result=pinned(MT2FW_MT2_FAILED,[&]{return callbacks_.save_request(callbacks_.user,&call,cause,sizeof(cause));});
        if(!std::memchr(cause,0,sizeof(cause)))throw std::runtime_error("Native Save request cause is not terminated");
        if(result!=MT2FW_MT2_OK)throw std::runtime_error("Native Save intent was not retained result="+std::to_string(result)+" cause="+cause);
        if(!current()||callbacks_retired_)throw std::runtime_error("Native Save request owner retired during callback");
        kernel_->validate_save_request();const auto after=kernel_->snapshot();
        if(after.lease!=before.lease||after.session!=before.session||after.world_epoch!=before.world_epoch||
            after.tick!=before.tick||after.invocation!=before.invocation||after.phase!=before.phase)
            throw std::runtime_error("Native Save request advanced or changed its bound execution");
        return true;
    }catch(const std::exception& e){kernel_->failed(e.what());reject(e.what(),"error");throw;}
    catch(...){kernel_->failed("Native Save request raised unknown exception");reject("Native Save request raised unknown exception","error");throw;}
}
MT2FW_MT2_Result NativeStepAdapter::bind(const MT2FW_MT2_StepSpec* input,MT2FW_MT2_StepReceipt* out){
    if(!current())return MT2FW_MT2_UNAVAILABLE;
    if(proof_inflight_)return MT2FW_MT2_BUSY;
    if(!input||input->size!=sizeof(*input))return MT2FW_MT2_INVALID;
    auto proxy=*input;proxy.service=service;proxy.boundary=boundary_call;proxy.user=this;proxy.proof=proof_call;proxy.proof_user=this;
    // Validate the caller's callbacks before replacing them with owner-pinned thunks.
    if(!input->service||!input->boundary||!input->proof||!input->save_request)return MT2FW_MT2_INVALID;
    const auto previous=callbacks_;const bool retired=callbacks_retired_;callbacks_=*input;callbacks_retired_=false;
    const auto prior_framework=framework_epoch_,prior_native=native_epoch_;
    framework_epoch_=native_epoch_=0;
    const auto result=api_.bind(api_.context,&proxy,out);
    if(result!=MT2FW_MT2_OK){callbacks_=previous;callbacks_retired_=retired;framework_epoch_=prior_framework;native_epoch_=prior_native;}
    return result;
}
MT2FW_MT2_Result MT2FW_CALL NativeStepAdapter::proof_call(void* context,const MT2FW_MT2_StepProofQuery* query,MT2FW_MT2_StepProofReply* out){
    if(!context||!query||!out||query->size!=sizeof(*query)||query->version!=MT2FW_MT2_STEP_DRAFT_VERSION||out->size!=sizeof(*out))return MT2FW_MT2_INVALID;
    auto& adapter=*static_cast<NativeStepAdapter*>(context);
    if(!adapter.current()||adapter.callbacks_retired_||!adapter.callbacks_.proof)return MT2FW_MT2_UNAVAILABLE;
    if(query->thread!=adapter.bound_()||query->session!=adapter.callbacks_.session||query->session_provider_token!=adapter.callbacks_.session_provider_token)return MT2FW_MT2_INVALID;
    auto error=[&](const char* cause){const auto count=std::min(std::strlen(cause),sizeof(out->cause)-1);std::memcpy(out->cause,cause,count);out->cause[count]=0;return MT2FW_MT2_FAILED;};
    if(adapter.proof_inflight_)return error("Step proof callback reentered");
    struct Release {bool& flag;~Release(){flag=false;}} release{adapter.proof_inflight_};adapter.proof_inflight_=true;
    try{
        const auto result=adapter.pinned(MT2FW_MT2_FAILED,[&]{return adapter.callbacks_.proof(adapter.callbacks_.proof_user,query,out);});
        if(!adapter.current()||adapter.callbacks_retired_)return error("Step proof owner retired during callback");
        return result;
    }catch(const std::exception& e){return error(e.what());}
    catch(...){return error("Step proof callback raised an unknown native exception");}
}
MT2FW_MT2_Result MT2FW_CALL NativeStepAdapter::service(void* context,const MT2FW_MT2_StepReceipt* receipt){
    auto& service_owner=*static_cast<NativeStepAdapter*>(context);
    if(service_owner.service_inflight_)return MT2FW_MT2_FAILED;
    service_owner.service_inflight_=true;
    struct ServiceClear{bool& value;~ServiceClear(){value=false;}}service_clear{service_owner.service_inflight_};
    auto& adapter=*static_cast<NativeStepAdapter*>(context);
    if(!adapter.current()||adapter.callbacks_retired_)return MT2FW_MT2_UNAVAILABLE;
    if(!receipt)return MT2FW_MT2_INVALID;
    if(receipt->phase!=MT2FW_MT2_STEP_FAILED)
        return adapter.pinned(MT2FW_MT2_FAILED,[&]{return adapter.callbacks_.service(adapter.callbacks_.user,receipt);});
    if(receipt->callbacks_inflight!=1||receipt->workers_held!=1||adapter.runtime_.rendering)
        throw std::runtime_error("Terminal UI requires its sole held service outside a render callback");
    // Only this owner-pinned local callback may issue the existing ui.native
    // requests. No UI event broadcast or world update is performed here.
    struct Restore {bool& value;~Restore(){value=false;}} restore{adapter.runtime_.rendering};
    adapter.runtime_.rendering=true;
    return adapter.pinned(MT2FW_MT2_FAILED,[&]{return adapter.callbacks_.service(adapter.callbacks_.user,receipt);});
}
MT2FW_MT2_StepDisposition MT2FW_CALL NativeStepAdapter::boundary_call(void* context,const MT2FW_MT2_StepBoundary* frame,MT2FW_MT2_StepNext next,void* continuation){
    auto& adapter=*static_cast<NativeStepAdapter*>(context);
    if(!adapter.current()||adapter.callbacks_retired_)return MT2FW_MT2_STEP_STOP_UNKNOWN;
    if(adapter.boundary_inflight_)return MT2FW_MT2_STEP_STOP_UNKNOWN;
    adapter.boundary_inflight_=true;
    struct Clear{bool& value;~Clear(){value=false;}}clear{adapter.boundary_inflight_};
    return adapter.pinned(MT2FW_MT2_STEP_STOP_UNKNOWN,[&]{return adapter.callbacks_.boundary(adapter.callbacks_.user,frame,next,continuation);});
}
bool NativeStepAdapter::retire(Plugin& plugin){
    if(plugin_==&plugin&&(proof_inflight_||save_inflight_||recovery_callbacks_inflight_))return false;
    if(plugin_!=&plugin||!kernel_||callbacks_retired_)return true;
    auto receipt=kernel_->snapshot();const auto result=api_.unbind(api_.context,receipt.lease,&receipt);
    if(result==MT2FW_MT2_BUSY)return false;
    if(result!=MT2FW_MT2_OK){reject(std::string("Native step retirement failed: ")+receipt.cause);return false;}
    callbacks_retired_=true;if(recovery_)recovery_->detach();return true;
}
bool NativeStepAdapter::reset_after_world_exit(){
    if(!kernel_||!callbacks_retired_||proof_inflight_||save_inflight_||!driver_.exited||!driver_.reset)return false;
    if(!kernel_->reset_after_world_exit())return false;
    try{driver_.reset();callbacks_={};recovery_callbacks_={};recovery_.reset();framework_epoch_=native_epoch_=0;return true;}
    catch(const std::exception& error){kernel_->failed(std::string("Step world-exit reset failed: ")+error.what());throw;}
    catch(...){kernel_->failed("Step world-exit reset raised an unknown exception");throw;}
}
Json NativeStepAdapter::status()const{
    auto r=kernel_?kernel_->snapshot():MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};
    return {{"configured",bool(driver_.inspect)&&bool(driver_.advance)},{"held",retained()},{"phase",r.phase},
        {"lease",r.lease},{"tick",r.tick},{"completed_tick",r.completed_tick},{"actual_role",r.actual_role},
        {"exit_preflight",{{"ready",exit_preflight_ready_&&r.lease==exit_preflight_lease_&&r.phase==exit_preflight_phase_},
            {"lease",exit_preflight_lease_},{"phase",exit_preflight_phase_},{"cause",exit_preflight_cause_}}},
        {"outcome",r.outcome},{"cause",r.cause},{"availability_cause",last_reason_},{"terminal_error",kernel_?kernel_->terminal_error():std::string()}};
}
}
