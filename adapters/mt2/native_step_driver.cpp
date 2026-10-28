#include "native_step_driver.h"
#include "native_step_pins.h"
#include "native_step_proof.h"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>

namespace mt2fw::mt2 {
namespace {
void require(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
bool valid_uuid(const std::string& value){return !value.empty()&&value.size()<MT2FW_MT2_STEP_UUID_BYTES&&
    std::all_of(value.begin(),value.end(),[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_';});}
void identity_valid(const StepPreloadIdentity& value){
    require(value.owner&&value.session&&value.provider_token&&value.generation&&value.snapshot_bytes&&value.thread&&value.peer_mask,
        "Step preload lacks owner/session/provider/generation/bytes/thread/membership");
    require(value.agreed_seed.has_value(),"Step preload lacks a shared agreed seed");
    require(valid_uuid(value.snapshot_uuid)&&value.snapshot_sha256.size()==64&&
        value.snapshot_sha256.find_first_not_of("0123456789abcdef")==std::string::npos,"Step preload snapshot UUID or SHA256 is invalid");
}
std::string exception_cause(std::exception_ptr error){try{std::rethrow_exception(error);}catch(const std::exception& e){return e.what();}catch(...){return "unknown native exception";}}
}
NativeStepDriver::NativeStepDriver(Backend backend,Source source):native_(std::move(backend)),source_(std::move(source)){
    require(native_.image&&native_.thread&&native_.read&&native_.write&&native_.prove_routes&&native_.original_go&&native_.original_timer&&
        native_.pause&&native_.resume&&native_.finish_all&&native_.all_done&&native_.before_step,"Step native driver is incomplete");
}
void NativeStepDriver::freeze_preload(const StepPreloadIdentity& identity){
    require(source_&&!frozen_&&!active_&&!failed(),"Step preload Source is missing or its identity is already frozen or failed");identity_valid(identity);
    require(native_.thread()==identity.thread,"Step preload is outside the bound game thread");native_.prove_routes();
    const auto actual=source_(identity.owner);
    require(actual.phase==StepNativeObservation::Phase::Preload&&actual.identity==identity,
        "Step preload differs from verified snapshot/session selection: "+actual.cause);
    frozen_=identity;
}
StepNativeObservation NativeStepDriver::observation()const{
    require(frozen_&&source_&&!failed()&&native_.thread()==frozen_->thread,"Step lifecycle lacks current frozen source/thread");
    auto value=source_(frozen_->owner);
    require(value.identity==*frozen_,"Step lifecycle selected identity changed: "+value.cause);
    return value;
}
void NativeStepDriver::reset_after_world_exit(){
    require(frozen_&&!active_&&native_.thread()==frozen_->thread&&native_.world_exited&&native_.world_exited(),
        "Step driver reset lacks independently verified exited world");
    native_.prove_routes();
    if(c_proof_)source_={};c_proof_=false;proof_spec_={};frozen_.reset();resume_workers_.reset();
    game_=tasks_=0;timer_calls_=update_calls_=passive_dt_=0;pause_attempted_=false;receipt_=nullptr;failure_.clear();
}
void NativeStepDriver::current_game(uintptr_t game)const{
    require(game&&read<uintptr_t>(native_.image+0x13df8f0)==game&&read<uintptr_t>(game)==native_.image+0x1743740,
        "Step current Game singleton/address point changed");
}
uintptr_t NativeStepDriver::scheduler(uintptr_t game)const{
    current_game(game);const auto tasks=read<uintptr_t>(native_.image+0x13df340);
    require(tasks&&read<uintptr_t>(game+0x68)==tasks&&read<uintptr_t>(tasks)==native_.image+0x16fdaf0,"Step Task owner/address point changed");
    return tasks;
}
void NativeStepDriver::held(uintptr_t tasks)const{
    const auto paused=read<uint8_t>(tasks+0xf8);
    const auto running_type2=read<int32_t>(tasks+0xa8),running_type1=read<int32_t>(tasks+0x90);
    require(paused==1&&running_type2==0&&running_type1==0,
        "Step scheduler is not paused/drained: paused="+std::to_string(paused)+
        " running_type1="+std::to_string(running_type1)+" running_type2="+std::to_string(running_type2));
}
StepNativeObservation NativeStepDriver::loaded(const MT2FW_Host* owner,bool entered){
    require(frozen_&&owner==frozen_->owner,"Step driver has no immutable preload owner");
    require(!failed(),"Step driver retains an unknown result: "+failure_);
    require(native_.thread()==frozen_->thread,"Step native game thread changed");native_.prove_routes();
    auto actual=source_(owner);
    if(native_.bootstrap)require(native_.bootstrap(actual),"Step Framework bootstrap receipt is not ready");
    require(actual.identity==*frozen_,"Step NativeOpen/session/snapshot identity changed: "+actual.cause);
    require(actual.phase==StepNativeObservation::Phase::Loaded&&actual.world_epoch&&actual.resume_workers.has_value()&&
        std::any_of(actual.witness.begin(),actual.witness.end(),[](unsigned char b){return b!=0;}),
        "Step loaded bootstrap witness is unavailable: "+actual.cause);
    current_game(actual.game);
    require(actual.mode&&read<uintptr_t>(actual.game+0x28)==actual.mode&&read<uintptr_t>(native_.image+0x13df320)==actual.mode&&
        read<uintptr_t>(actual.mode)==native_.image+0x16e6730&&read<int32_t>(actual.mode+0x2c4)==4,
        "Step loaded Mode singleton/state changed");
    require(actual.state&&read<uintptr_t>(actual.mode+0x58)==actual.state&&actual.world&&read<uintptr_t>(actual.state+0x160)==actual.world&&
        read<uintptr_t>(native_.image+0x13df9c0)==actual.world&&read<uintptr_t>(actual.world)==native_.image+0x174aef0,
        "Step loaded GameState/World identity changed");
    require(actual.map&&read<uintptr_t>(actual.world+0x158)==actual.map&&read<uintptr_t>(native_.image+0x13df8c0)==actual.map&&
        read<uintptr_t>(actual.map)==native_.image+0x1742860&&read<uint8_t>(actual.map+0x47f)==1,"Step loaded Map/readiness changed");
    const auto task=scheduler(actual.game);
    if(entered){
        // Immutable intent may be retained while the one original Go owns its
        // resumed scheduler. This proof is deliberately not quiescent.
        require(active_&&receipt_&&actual.game==game_&&task==tasks_&&active_frame_.invocation&&
            receipt_->phase==MT2FW_MT2_STEP_ENTERED&&receipt_->workers_held==0&&receipt_->callbacks_inflight==1&&
            receipt_->original_attempted==1&&receipt_->original_returned==0&&
            receipt_->lease==active_frame_.lease&&receipt_->session==active_frame_.session&&
            receipt_->world_epoch==actual.world_epoch&&receipt_->world_epoch==active_frame_.world_epoch&&
            receipt_->tick==active_frame_.tick&&receipt_->invocation==active_frame_.invocation&&
            receipt_->thread==active_frame_.thread&&receipt_->actual_role==active_frame_.actual_role&&
            receipt_->dt_bits==MT2FW_MT2_STEP_FIXED_DT_BITS&&read<uint8_t>(task+0xf8)==0&&
            read<uint32_t>(game_+0x1c)==MT2FW_MT2_STEP_FIXED_DT_BITS,
            "Step entered intent lacks its exact active original Go receipt/scheduler");
    }else held(task);
    if(resume_workers_)require(resume_workers_==actual.resume_workers,"Step bootstrap prior scheduler state changed");
    return actual;
}
NativeStepKernel::Proof NativeStepDriver::inspect(const MT2FW_Host* owner,const MT2FW_MT2_StepSpec& spec){
    NativeStepKernel::Proof proof;
    try{
        require(owner&&spec.size==sizeof(spec),"Step native inspect requires the current sized draft");
        if(!source_){
            require(spec.proof!=nullptr,"Step native proof callback is missing");c_proof_=true;
            source_=[this](const MT2FW_Host* host){return read_step_proof(host,proof_spec_,native_.thread());};
        }
        if(!frozen_&&c_proof_){
            require(spec.proof!=nullptr,"Step native proof callback is missing");proof_spec_=spec;
            const auto selected=source_(owner);
            require(selected.identity.session==spec.session&&selected.identity.provider_token==spec.session_provider_token&&
                selected.identity.peer_mask==spec.frozen_peer_mask&&std::memchr(spec.snapshot_uuid,0,sizeof(spec.snapshot_uuid))&&
                selected.identity.snapshot_uuid==spec.snapshot_uuid,"Step proof preload differs from bind selection");
            freeze_preload(selected.identity);
        }
        require(frozen_&&owner==frozen_->owner,"Step requires an owner-scoped verified preload handoff");
        require(!failed(),"Step native failure retained: "+failure_);
        require(spec.size==sizeof(spec)&&spec.session==frozen_->session&&spec.session_provider_token==frozen_->provider_token&&
            spec.frozen_peer_mask==frozen_->peer_mask&&spec.dt_bits==MT2FW_MT2_STEP_FIXED_DT_BITS&&
            std::memchr(spec.snapshot_uuid,0,sizeof(spec.snapshot_uuid))&&spec.snapshot_uuid==frozen_->snapshot_uuid,
            "Step spec differs from the immutable verified preload identity");
        require(native_.thread()==frozen_->thread,"Step inspect changed game thread");native_.prove_routes();
        auto actual=source_(owner);require(actual.identity==*frozen_,"Step current snapshot/session differs: "+actual.cause);
        require(actual.phase==StepNativeObservation::Phase::Preload||actual.phase==StepNativeObservation::Phase::Loading||
            actual.phase==StepNativeObservation::Phase::Loaded,"Step native lifecycle is unavailable: "+actual.cause);
        proof.owner=true;proof.context=true;proof.exclusive_routes=true;proof.thread=frozen_->thread;
        if(actual.phase==StepNativeObservation::Phase::Loaded&&(!native_.bootstrap||native_.bootstrap(actual))){
            actual=loaded(owner,active_);proof.bootstrap=true;proof.quiescent=!active_;proof.world_epoch=actual.world_epoch;proof.witness=actual.witness;
        }else proof.cause="Step native selected load has not reached its held bootstrap";
    }catch(const std::exception& e){proof.owner=false;proof.context=false;proof.bootstrap=false;proof.cause=e.what();}
    catch(...){proof.owner=false;proof.context=false;proof.bootstrap=false;proof.cause="Step native proof raised an unknown exception";}
    return proof;
}
void NativeStepDriver::timer_update(void* receiver,float incoming,uintptr_t caller){
    if(!active_){native_.original_timer(receiver,incoming);return;}
    require(native_.thread()==frozen_->thread,"Step Timer thread changed");
    ++timer_calls_;receipt_->timer_calls=timer_calls_;native_.original_timer(receiver,incoming);
    require(caller==native_.image+step_pins::timer_return_rva&&timer_calls_==1,"Step Timer caller/count differs");
    current_game(game_);const auto timer=reinterpret_cast<uintptr_t>(receiver);
    require(timer&&read<uintptr_t>(native_.image+0x1c4eb70)==timer&&read<uintptr_t>(timer)==native_.image+0x16f9b80&&
        read<uintptr_t>(native_.image+0x16f9ba0)==native_.image+step_pins::timer_rva&&read<uint8_t>(timer+8)==1,"Step Timer singleton/dispatch changed");
    const auto systems=read<uintptr_t>(game_+8);const auto count=read<int32_t>(game_+0x10);
    require(systems&&count>0&&count<=64,"Step active system array bounds changed");
    unsigned found=0;for(int32_t i=0;i<count;++i)found+=read<uintptr_t>(systems+size_t(i)*8)==timer;
    require(found==1,"Step Timer is not a unique active system");write(game_+0x1c,MT2FW_MT2_STEP_FIXED_DT_BITS);
}
void NativeStepDriver::after_update(void* game){
    if(!active_)return;
    require(native_.thread()==frozen_->thread&&game_==reinterpret_cast<uintptr_t>(game)&&++update_calls_==1,"Step GameUpdate caller/count differs");
    const auto tasks=scheduler(game_);require(tasks==tasks_&&read<uint8_t>(tasks+0xf8)==0,"Step GameUpdate scheduler is not released");
    require(read<int32_t>(tasks+0xc0)>=0,"Step finished-task count is negative");
    if(!native_.all_done(reinterpret_cast<void*>(tasks))||read<int32_t>(tasks+0xc0)!=0){
        // Preserve the engine's wait, GL pump, callback ownership and one attempt.
        native_.finish_all(reinterpret_cast<void*>(tasks));
        require(native_.all_done(reinterpret_cast<void*>(tasks))&&read<int32_t>(tasks+0xc0)==0,
            "Step FinishAllTasks returned with pending or finished work; no repeated drain");
    }
}
void NativeStepDriver::pause_and_restore(){
    require(native_.thread()==frozen_->thread,"Step held restoration changed game thread");
    require(scheduler(game_)==tasks_,"Step Task owner changed before held restoration");
    pause_attempted_=true;native_.pause(reinterpret_cast<void*>(tasks_));held(tasks_);
    write(game_+0x1c,passive_dt_);receipt_->workers_held=1;
}
void NativeStepDriver::advance(const MT2FW_MT2_StepBoundary& frame,MT2FW_MT2_StepReceipt& receipt){
    require(!active_&&!failed(),"Step original is active or retains a failed result");
    require(frozen_&&frame.size==sizeof(frame)&&frame.session==frozen_->session&&frame.thread==frozen_->thread&&
        frame.dt_bits==MT2FW_MT2_STEP_FIXED_DT_BITS,"Step original boundary identity differs");
    auto observed=loaded(frozen_->owner);
    require(frame.world_epoch==observed.world_epoch,"Step original world epoch differs");
    game_=observed.game;tasks_=scheduler(game_);resume_workers_=observed.resume_workers;
    // A permanently paused Task manager cannot execute the admitted simulation.
    require(*resume_workers_,"Step bootstrap did not capture a running Task scheduler before Pause");
    passive_dt_=read<uint32_t>(game_+0x1c);receipt_=&receipt;active_frame_=frame;active_=true;pause_attempted_=false;timer_calls_=update_calls_=0;
    receipt.workers_held=0;
    try{
        native_.before_step(frame);write(game_+0x1c,MT2FW_MT2_STEP_FIXED_DT_BITS);
        native_.resume(reinterpret_cast<void*>(tasks_));require(read<uint8_t>(tasks_+0xf8)==0,"Step original Task Resume was not acknowledged");
        receipt.original_attempted=1;native_.original_go(reinterpret_cast<void*>(game_));receipt.original_returned=1;
        require(timer_calls_==1&&update_calls_==1&&read<uint32_t>(game_+0x1c)==MT2FW_MT2_STEP_FIXED_DT_BITS,"Step original Timer/update/fixed dt proof is incomplete");
        if(native_.after_go)native_.after_go();
        pause_and_restore();active_=false;receipt_=nullptr;loaded(frozen_->owner);
    }catch(...){
        const auto original=std::current_exception();
        failure_=exception_cause(original);
        // A failed original is never retried. Make one held-restoration attempt
        // only if Pause has not already been attempted by this driver.
        if(!pause_attempted_){try{pause_and_restore();}catch(...){failure_+="; held restoration failed: "+exception_cause(std::current_exception());}}
        active_=false;receipt_=nullptr;throw std::runtime_error(failure_);
    }
}
}
