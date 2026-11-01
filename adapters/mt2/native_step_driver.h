#pragma once
#include "native_steps.h"
#include "native_region_rng.h"
#include <optional>
#include <stdexcept>

namespace mt2fw::mt2 {
// Internal Framework handoff. No field is supplied by the public Step C table.
// The retained MP owner reads NativeOpen/normal_load and the live Session wire.
struct StepPreloadIdentity {
    const MT2FW_Host* owner=nullptr;
    uint64_t session=0,provider_token=0,generation=0,snapshot_bytes=0,thread=0;
    std::optional<uint64_t> agreed_seed;
    uint8_t peer_mask=0;
    std::string snapshot_uuid,snapshot_sha256;
    uint32_t region_rng_mode=0,region_rng_count=0;
    std::array<unsigned char,32> region_rng_sha256{};
    bool operator==(const StepPreloadIdentity&)const=default;
};
struct StepNativeObservation {
    enum class Phase {Unavailable,Preload,Loading,Loaded,Exited};
    StepPreloadIdentity identity;
    Phase phase=Phase::Unavailable;
    uint64_t world_epoch=0;
    uintptr_t game=0,mode=0,state=0,world=0,map=0;
    std::array<unsigned char,32> witness{};
    // Captured before the original bootstrap Pause, not inferred from Pause=1.
    std::optional<bool> resume_workers;
    std::string cause;
};

class NativeStepDriver {
public:
    using Source=std::function<StepNativeObservation(const MT2FW_Host*)>;
    struct Backend {
        uintptr_t image=0;
        NativeStepKernel::Thread thread;
        std::function<void(uintptr_t,void*,size_t)> read;
        std::function<void(uintptr_t,const void*,size_t)> write;
        // Revalidate acknowledged Framework-owned Go/Timer trampolines and pins.
        std::function<void()> prove_routes;
        std::function<void(void*)> original_go,pause,resume,finish_all;
        std::function<bool(void*)> all_done;
        std::function<void(void*,float)> original_timer;
        // Restore the accepted native load barrier before the first Go only.
        std::function<void(const MT2FW_MT2_StepBoundary&)> before_step;
        // A Framework lifecycle producer adds its own Pause/certificate receipt
        // to the independently supplied, sealed NativeOpen identity.
        std::function<bool(StepNativeObservation&)> bootstrap;
        std::function<bool()> world_exited;
        // Ordered snapshot preparation runs once after original Go, before Pause.
        std::function<void()> after_go;
    };
    NativeStepDriver(Backend,Source);
    explicit NativeStepDriver(Backend backend):NativeStepDriver(std::move(backend),{}){}
    void freeze_preload(const StepPreloadIdentity&);
    void reset_after_world_exit();
    NativeStepKernel::Proof inspect(const MT2FW_Host*,const MT2FW_MT2_StepSpec&);
    void advance(const MT2FW_MT2_StepBoundary&,MT2FW_MT2_StepReceipt&);
    // Called by the existing Framework hooks, not installed by this class.
    void timer_update(void*,float,uintptr_t immediate_return);
    void after_update(void* game);
    StepNativeObservation observation()const;
    std::vector<region_rng::Row> region_rng_rows()const;
    bool frozen()const{return frozen_.has_value();}
    bool failed()const{return !failure_.empty();}
    void recovery_local(const MT2FW_Host*,const MT2FW_MT2_StepSpec&,MT2FW_MT2_StepProofFn,void*);
    const std::string& failure()const{return failure_;}
private:
    Backend native_;
    Source source_;
    MT2FW_MT2_StepSpec proof_spec_{};
    bool c_proof_=false;
    std::optional<StepPreloadIdentity> frozen_;
    std::optional<bool> resume_workers_;
    bool active_=false,pause_attempted_=false;
    uintptr_t game_=0,tasks_=0;
    uint32_t timer_calls_=0,update_calls_=0,passive_dt_=0;
    MT2FW_MT2_StepReceipt* receipt_=nullptr;
    MT2FW_MT2_StepBoundary active_frame_{};
    std::string failure_;
    template<class T>T read(uintptr_t at)const{T value{};native_.read(at,&value,sizeof(value));return value;}
    template<class T>void write(uintptr_t at,T value){native_.write(at,&value,sizeof(value));if(read<T>(at)!=value)throw std::runtime_error("Step native write readback differs");}
    StepNativeObservation loaded(const MT2FW_Host*,bool entered=false);
    StepNativeObservation validate_loaded(StepNativeObservation,bool entered);
    void current_game(uintptr_t)const;
    uintptr_t scheduler(uintptr_t)const;
    void held(uintptr_t)const;
    void pause_and_restore();
};
}
