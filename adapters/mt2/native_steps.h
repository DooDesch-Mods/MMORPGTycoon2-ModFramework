#pragma once
#include "sdk/native_steps_draft.h"
#include <array>
#include <functional>
#include <string>

namespace mt2fw::mt2 {
// Framework-owned driver inputs. Plugins cannot supply these through the C API.
class NativeStepKernel {
public:
    struct Proof {
        bool owner=false,provider=false,context=false,exclusive_routes=false;
        bool bootstrap=false,quiescent=false,world_exited=false,lab_gate=false;
        uint32_t role=MT2FW_MT2_SESSION_UNKNOWN;
        uint64_t thread=0,world_epoch=0;
        std::array<unsigned char,32> witness{};
        std::string cause;
    };
    using Inspect=std::function<Proof(const MT2FW_Host*,const MT2FW_MT2_StepSpec&)>;
    // The actual Go/Timer driver writes attempt/return flags before/after native
    // calls, including when a later proof or cleanup raises.
    using Advance=std::function<void(const MT2FW_MT2_StepBoundary&,MT2FW_MT2_StepReceipt&)>;
    using Thread=std::function<uint64_t()>;
    NativeStepKernel(const MT2FW_Host*,Inspect,Advance,Thread);
    MT2FW_MT2_Result query(const MT2FW_Host*,uint32_t,MT2FW_MT2_StepServiceDraftV1*,uint32_t);
    MT2FW_MT2_StepReceipt boundary();
    // Outer-frame maintenance never lends Next. Activation is allowed from its
    // startup service after Framework publishes its retained bootstrap receipt.
    MT2FW_MT2_StepReceipt maintenance();
    const std::string& terminal_error()const{return terminal_error_;}
    void lifecycle(const MT2FW_MT2_StepReceipt&);
    void failed(const std::string& cause){fail(cause);}
    void validate_native_exit_request()const;
    void validate_save_request();
    void native_exit_requested();
    MT2FW_MT2_StepReceipt snapshot()const{return receipt_;}
    bool held()const{return receipt_.phase!=MT2FW_MT2_STEP_PASSIVE;}
    // Only the native lifecycle driver may clear a retained failed/retired hold.
    bool reset_after_world_exit();
private:
    const MT2FW_Host* owner_;
    Inspect inspect_;
    Advance advance_;
    Thread thread_;
    MT2FW_MT2_StepSpec spec_{};
    MT2FW_MT2_StepReceipt receipt_{sizeof(receipt_),MT2FW_MT2_STEP_PASSIVE};
    uint64_t next_lease_=1,next_invocation_=1,invocation_=0,next_tick_=0,bound_thread_=0;
    bool servicing_=false,exiting_=false;
    bool bound_=false,entered_=false,next_attempted_=false,retiring_=false;
    std::string cause_,rejection_,terminal_error_;
    MT2FW_MT2_Result reject(MT2FW_MT2_Result,const std::string&);
    Proof current(bool bootstrap);
    void fail(const std::string&);
    void copy(MT2FW_MT2_StepReceipt*)const;
    template<class F>MT2FW_MT2_Result control(F&&,MT2FW_MT2_StepReceipt*)noexcept;
    MT2FW_MT2_Result bind(const MT2FW_MT2_StepSpec*,MT2FW_MT2_StepReceipt*);
    MT2FW_MT2_Result activate(const MT2FW_MT2_StepActivation*,MT2FW_MT2_StepReceipt*);
    MT2FW_MT2_Result read(uint64_t,MT2FW_MT2_StepReceipt*);
    MT2FW_MT2_Result unbind(uint64_t,MT2FW_MT2_StepReceipt*);
    MT2FW_MT2_Result next(uint64_t,MT2FW_MT2_StepReceipt*);
    static MT2FW_MT2_Result MT2FW_CALL next_call(void*,uint64_t,MT2FW_MT2_StepReceipt*);
};
}
