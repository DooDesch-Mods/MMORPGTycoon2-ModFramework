#pragma once
#include "native_steps.h"
#include "sdk/native_step_recovery_draft.h"

namespace mt2fw::mt2 {
class NativeStepRecovery {
public:
    enum class LocalProof { WatchBegin, WatchEnd, Adoption };
    struct Driver {
        std::function<void(const MT2FW_MT2_RecoverySpec&,const MT2FW_MT2_StepReceipt&,LocalProof)> local;
        std::function<void(MT2FW_MT2_RecoveryReceipt&)> prepare;
        std::function<void(const MT2FW_MT2_RecoveryReceipt&,MT2FW_MT2_SnapshotCaptureFn,void*)> capture;
        std::function<bool(const MT2FW_MT2_RecoveryReceipt&)> pump;
        std::function<void(const MT2FW_MT2_RecoveryReceipt&)> retire;
    };
    NativeStepRecovery(NativeStepKernel& kernel,Driver driver):kernel_(kernel),driver_(std::move(driver)){}
    MT2FW_MT2_Result bind(const MT2FW_MT2_RecoverySpec*,uint8_t,uint64_t,MT2FW_MT2_RecoveryReceipt*);
    MT2FW_MT2_Result read(uint64_t,MT2FW_MT2_RecoveryReceipt*);
    MT2FW_MT2_Result operation(const MT2FW_MT2_RecoveryRequest*,MT2FW_MT2_SnapshotCaptureFn,void*,MT2FW_MT2_RecoveryReceipt*);
    bool boundary();
    bool interrupted()const{return receipt_.interruption!=0;}
    bool active()const{return bound_;}
    const MT2FW_MT2_RecoverySpec& spec()const{return spec_;}
    const MT2FW_MT2_RecoveryReceipt& receipt()const{return receipt_;}
    void detach(){bound_=false;spec_={};}
private:
    NativeStepKernel& kernel_;Driver driver_;
    MT2FW_MT2_RecoverySpec spec_{};
    MT2FW_MT2_RecoveryReceipt receipt_{sizeof(receipt_)};
    bool bound_=false,observing_=false,servicing_=false,operating_=false;
    uint64_t next_interruption_=1;
    void local(LocalProof purpose=LocalProof::Adoption);
    void witness(unsigned char*);
    void fail(const std::string&);
    void copy(MT2FW_MT2_RecoveryReceipt*)const;
};
}
