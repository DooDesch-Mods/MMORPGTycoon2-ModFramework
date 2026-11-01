#pragma once
#include "native_steps.h"
#include "native_step_recovery.h"
#include "native_patches.h"
#include <algorithm>
#include <cstring>
#include <exception>

namespace mt2fw::mt2 {
class NativeStepAdapter {
public:
    struct Driver {NativeStepKernel::Inspect inspect;NativeStepKernel::Advance advance;
        std::function<NativeStepKernel::Proof()> exited;
        std::function<void()> reset;
        std::function<void()> preflight_exit;
        std::function<void(const MT2FW_MT2_SnapshotRequest&,MT2FW_MT2_SnapshotCaptureFn,void*)> snapshot;
        std::function<void(uint32_t,const std::string&)> exit_save;
        std::function<void(const MT2FW_Host*,const MT2FW_MT2_StepSpec&,MT2FW_MT2_StepProofFn,void*)> recovery_local;
        NativeStepRecovery::Driver recovery;
    };
    NativeStepAdapter(Runtime& runtime,NativePatches& patches,NativeStepKernel::Thread thread,NativeStepKernel::Thread bound,std::function<bool()> lab)
        :runtime_(runtime),patches_(patches),thread_(std::move(thread)),bound_(std::move(bound)),lab_(std::move(lab)){}
    bool configure(Driver);
    MT2FW_MT2_Result query(const MT2FW_Host*,uint32_t,MT2FW_MT2_StepServiceDraftV1*,uint32_t);
    MT2FW_MT2_Result query_recovery(const MT2FW_Host*,uint32_t,MT2FW_MT2_StepRecoveryDraftV1*,uint32_t);
    const MT2FW_MT2_RecoveryReceipt* recovery_snapshot()const{return recovery_&&recovery_->active()?&recovery_->receipt():nullptr;}
    bool retire(Plugin&);
    bool retained()const{return kernel_&&kernel_->held();}
    MT2FW_MT2_StepReceipt boundary(){return kernel_?kernel_->boundary():MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};}
    MT2FW_MT2_StepReceipt snapshot()const{return kernel_?kernel_->snapshot():MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};}
    MT2FW_MT2_StepReceipt maintenance(){return kernel_?kernel_->maintenance():snapshot();}
    const std::string terminal_error()const{return kernel_?kernel_->terminal_error():"Terminal kernel is unavailable";}
    void lifecycle(const MT2FW_MT2_StepReceipt& receipt){if(kernel_)kernel_->lifecycle(receipt);}
    void failed(const std::string& cause){if(kernel_)kernel_->failed(cause);}
    void validate_native_exit_request()const{if(!kernel_)throw std::runtime_error("Step exit kernel is absent");kernel_->validate_native_exit_request();}
    void native_exit_requested(){if(kernel_)kernel_->native_exit_requested();}
    bool reset_after_world_exit();
    bool save_request(void* mode,uint32_t kind,const char* filename,uint32_t bytes,bool option,uintptr_t caller_rva);
    Json status()const;
private:
    Runtime& runtime_;NativePatches& patches_;
    NativeStepKernel::Thread thread_,bound_;
    std::function<bool()> lab_;
    Driver driver_;
    Plugin* plugin_=nullptr;
    const MT2FW_Host* host_=nullptr;
    std::unique_ptr<NativeStepKernel> kernel_;
    std::unique_ptr<NativeStepRecovery> recovery_;
    MT2FW_MT2_RecoverySpec recovery_callbacks_{};
    MT2FW_MT2_StepServiceDraftV1 api_{};
    MT2FW_MT2_StepSpec callbacks_{};
    bool callbacks_retired_=true;
    bool proof_inflight_=false,service_inflight_=false,boundary_inflight_=false,snapshot_inflight_=false,save_inflight_=false;
    unsigned recovery_callbacks_inflight_=0;
    uint64_t framework_epoch_=0,native_epoch_=0;
    std::string last_reason_;
    bool exit_preflight_ready_=false;
    uint64_t exit_preflight_lease_=0;
    uint32_t exit_preflight_phase_=0;
    std::string exit_preflight_cause_;
    bool reject(const std::string&,const char* level="warn");
    bool current()const;
    template<class F>MT2FW_MT2_Result recovery_pinned(F&& call){
        ++recovery_callbacks_inflight_;
        struct Clear{unsigned& count;~Clear(){--count;}}clear{recovery_callbacks_inflight_};
        return pinned(MT2FW_MT2_FAILED,std::forward<F>(call));
    }
    template<class Result,class F>Result pinned(Result fallback,F&& fn){
        auto& plugin=*plugin_;++plugin.native_pins;
        Result result=fallback;std::exception_ptr callback_error,release_error;
        try{result=fn();}catch(...){callback_error=std::current_exception();}
        try{runtime_.release_native(plugin);}catch(...){release_error=std::current_exception();}
        auto cause=[](std::exception_ptr error){try{std::rethrow_exception(error);}catch(const std::exception& e){return std::string(e.what());}catch(...){return std::string("unknown native exception");}};
        if(callback_error&&release_error)throw std::runtime_error(cause(callback_error)+"; native pin release also failed: "+cause(release_error));
        if(callback_error)std::rethrow_exception(callback_error);
        if(release_error)std::rethrow_exception(release_error);
        return result;
    }
    template<class F>MT2FW_MT2_Result guard(F&& fn,MT2FW_MT2_StepReceipt* out)noexcept{
        if(!out||out->size<sizeof(*out))return MT2FW_MT2_INVALID;
        try{return fn();}
        catch(const std::exception& error){const auto n=std::min(std::strlen(error.what()),sizeof(out->cause)-1);std::memcpy(out->cause,error.what(),n);out->cause[n]=0;return MT2FW_MT2_FAILED;}
        catch(...){constexpr char cause[]="Native step adapter raised an unknown exception";std::memcpy(out->cause,cause,sizeof(cause));return MT2FW_MT2_FAILED;}
    }
    MT2FW_MT2_Result snapshot(const MT2FW_MT2_SnapshotRequest*,MT2FW_MT2_SnapshotCaptureFn,void*,MT2FW_MT2_StepReceipt*);
    MT2FW_MT2_Result bind(const MT2FW_MT2_StepSpec*,MT2FW_MT2_StepReceipt*);
    static MT2FW_MT2_Result MT2FW_CALL service(void*,const MT2FW_MT2_StepReceipt*);
    static MT2FW_MT2_Result MT2FW_CALL recovery_proof(void*,const MT2FW_MT2_StepProofQuery*,MT2FW_MT2_StepProofReply*);
    static MT2FW_MT2_Result MT2FW_CALL proof_call(void*,const MT2FW_MT2_StepProofQuery*,MT2FW_MT2_StepProofReply*);
    static MT2FW_MT2_StepDisposition MT2FW_CALL boundary_call(void*,const MT2FW_MT2_StepBoundary*,MT2FW_MT2_StepNext,void*);
};
}
