#include "native_steps.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace mt2fw::mt2 {
namespace {
void require(bool condition,const std::string& cause){if(!condition)throw std::runtime_error(cause);}
bool uuid(const char* value){
    const auto end=static_cast<const char*>(std::memchr(value,0,MT2FW_MT2_STEP_UUID_BYTES));
    return end&&end!=value&&std::all_of(value,end,[](unsigned char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_';});
}
}
NativeStepKernel::NativeStepKernel(const MT2FW_Host* owner,Inspect inspect,Advance advance,Thread thread)
    :owner_(owner),inspect_(std::move(inspect)),advance_(std::move(advance)),thread_(std::move(thread)){
    require(owner_&&inspect_&&advance_&&thread_,"Native step driver is incomplete");
}
void NativeStepKernel::copy(MT2FW_MT2_StepReceipt* out)const{if(out&&out->size>=sizeof(*out))*out=receipt_;}
MT2FW_MT2_Result NativeStepKernel::reject(MT2FW_MT2_Result result,const std::string& cause){rejection_=cause;return result;}
void NativeStepKernel::fail(const std::string& cause){
    if(receipt_.phase==MT2FW_MT2_STEP_FAILED)return;
    cause_=cause.empty()?"Native step failed without a cause":cause;
    receipt_.phase=MT2FW_MT2_STEP_FAILED;receipt_.outcome=MT2FW_MT2_STEP_UNKNOWN;
    const auto count=std::min(cause_.size(),sizeof(receipt_.cause)-1);
    std::memcpy(receipt_.cause,cause_.data(),count);receipt_.cause[count]=0;
}
template<class F>MT2FW_MT2_Result NativeStepKernel::control(F&& call,MT2FW_MT2_StepReceipt* out)noexcept{
    if(!out||out->size<sizeof(*out))return MT2FW_MT2_INVALID;
    try{
        rejection_.clear();const auto result=call();copy(out);
        if(result!=MT2FW_MT2_OK&&!rejection_.empty()){
            const auto count=std::min(rejection_.size(),sizeof(out->cause)-1);
            std::memcpy(out->cause,rejection_.data(),count);out->cause[count]=0;
        }
        return result;
    }
    catch(const std::exception& error){fail(error.what());copy(out);return MT2FW_MT2_FAILED;}
    catch(...){fail("Native step driver raised an unknown exception");copy(out);return MT2FW_MT2_FAILED;}
}
NativeStepKernel::Proof NativeStepKernel::current(bool bootstrap){
    require(bound_thread_&&thread_()==bound_thread_,"Native step caller changed thread");
    auto proof=inspect_(owner_,spec_);
    require(proof.thread==bound_thread_&&proof.owner&&proof.provider&&proof.context,
        "Native step owner/provider/context is not current: "+proof.cause);
    require(!proof.lab_gate&&proof.exclusive_routes,"Native step Go/Timer ownership conflicts: "+proof.cause);
    if(receipt_.actual_role)require(proof.role==receipt_.actual_role,"Native step actual session role changed");
    if(bootstrap){
        require(proof.bootstrap&&proof.quiescent&&proof.world_epoch,"Native step bootstrap is unavailable: "+proof.cause);
        require(proof.role==MT2FW_MT2_SESSION_HOST||proof.role==MT2FW_MT2_SESSION_CLIENT,"Native step actual session role is unknown");
        if(receipt_.world_epoch)require(proof.world_epoch==receipt_.world_epoch,"Native step world epoch changed");
    }
    return proof;
}
MT2FW_MT2_Result NativeStepKernel::query(const MT2FW_Host* owner,uint32_t version,MT2FW_MT2_StepServiceDraftV1* out,uint32_t capacity){
    if(owner!=owner_||!out||capacity<sizeof(*out)||version!=MT2FW_MT2_STEP_DRAFT_VERSION)return MT2FW_MT2_INVALID;
    *out={sizeof(*out),version,this,
        [](void* p,const MT2FW_MT2_StepSpec* s,MT2FW_MT2_StepReceipt* r){return static_cast<NativeStepKernel*>(p)->bind(s,r);},
        [](void* p,const MT2FW_MT2_StepActivation* a,MT2FW_MT2_StepReceipt* r){return static_cast<NativeStepKernel*>(p)->activate(a,r);},
        [](void* p,uint64_t lease,MT2FW_MT2_StepReceipt* r){return static_cast<NativeStepKernel*>(p)->read(lease,r);},
        [](void* p,uint64_t lease,MT2FW_MT2_StepReceipt* r){return static_cast<NativeStepKernel*>(p)->unbind(lease,r);},
        [](void*,uint64_t,MT2FW_MT2_StepReceipt*){return MT2FW_MT2_UNAVAILABLE;}};
    return MT2FW_MT2_OK;
}
MT2FW_MT2_Result NativeStepKernel::bind(const MT2FW_MT2_StepSpec* input,MT2FW_MT2_StepReceipt* out){
    return control([&]{
        if(bound_||held())return reject(MT2FW_MT2_BUSY,"Native step lease or failed hold is already retained");
        if(!input||input->size!=sizeof(*input)||!input->session||!input->session_provider_token||!input->frozen_peer_mask||
            input->dt_bits!=MT2FW_MT2_STEP_FIXED_DT_BITS||!uuid(input->snapshot_uuid)||!input->boundary||!input->service||!input->proof||!input->save_request||
            std::any_of(std::begin(input->reserved),std::end(input->reserved),[](uint8_t v){return v!=0;}))return reject(MT2FW_MT2_INVALID,"Native step spec fields, fixed dt or callbacks are invalid");
        const auto proof=inspect_(owner_,*input);
        if(!proof.owner||!proof.provider||!proof.context)return reject(MT2FW_MT2_UNAVAILABLE,"Native step bridge/provider/session context is unavailable: "+proof.cause);
        if(!proof.exclusive_routes||proof.lab_gate)return reject(MT2FW_MT2_UNAVAILABLE,"Native step Go/Timer owner conflicts with lab or is not exclusive: "+proof.cause);
        if(!proof.thread||proof.thread!=thread_())return reject(MT2FW_MT2_UNAVAILABLE,"Native step bind is outside its Framework game thread");
        require(next_lease_!=std::numeric_limits<uint64_t>::max(),"Native step lease space exhausted");
        spec_=*input;bound_thread_=proof.thread;bound_=true;retiring_=false;cause_.clear();terminal_error_.clear();
        receipt_={sizeof(receipt_),MT2FW_MT2_STEP_ARMED};receipt_.lease=next_lease_++;receipt_.session=spec_.session;
        receipt_.thread=bound_thread_;receipt_.dt_bits=spec_.dt_bits;receipt_.actual_role=proof.role;
        return MT2FW_MT2_OK;
    },out);
}
MT2FW_MT2_Result NativeStepKernel::activate(const MT2FW_MT2_StepActivation* start,MT2FW_MT2_StepReceipt* out){
    return control([&]{
        if(!bound_||retiring_||entered_||(receipt_.phase!=MT2FW_MT2_STEP_ARMED&&receipt_.phase!=MT2FW_MT2_STEP_LOADING))return reject(MT2FW_MT2_UNAVAILABLE,"Native step activation requires an armed, non-entered lease");
        if(!start||start->size!=sizeof(*start)||start->reserved||start->lease!=receipt_.lease||start->session!=spec_.session||
            !start->first_tick||start->first_tick==std::numeric_limits<uint64_t>::max()||!uuid(start->snapshot_uuid)||
            std::strcmp(start->snapshot_uuid,spec_.snapshot_uuid)!=0)return reject(MT2FW_MT2_INVALID,"Native step activation session, snapshot, lease or first tick differs");
        const auto proof=current(false);
        if(!proof.bootstrap||!proof.quiescent||!proof.world_epoch)return reject(MT2FW_MT2_UNAVAILABLE,"Native step bootstrap is not ready: "+proof.cause);
        if(receipt_.bootstrap_held!=1||receipt_.bootstrap_coverage!=MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS)
            return reject(MT2FW_MT2_UNAVAILABLE,"Native step activation lacks retained initial Developer coverage");
        if((proof.role!=MT2FW_MT2_SESSION_HOST&&proof.role!=MT2FW_MT2_SESSION_CLIENT)||proof.world_epoch!=start->world_epoch||
            !std::equal(proof.witness.begin(),proof.witness.end(),start->initial_witness_sha256))return reject(MT2FW_MT2_INVALID,"Native step activation role, world epoch or local witness differs");
        receipt_.phase=MT2FW_MT2_STEP_HELD;receipt_.world_epoch=proof.world_epoch;receipt_.actual_role=proof.role;
        next_tick_=start->first_tick;receipt_.tick=next_tick_;receipt_.completed_tick=next_tick_-1;receipt_.workers_held=1;
        return MT2FW_MT2_OK;
    },out);
}
MT2FW_MT2_Result NativeStepKernel::read(uint64_t lease,MT2FW_MT2_StepReceipt* out){
    return control([&]{
        if(lease!=receipt_.lease||!bound_thread_||thread_()!=bound_thread_)return reject(MT2FW_MT2_INVALID,"Native step receipt lease or thread differs");
        return MT2FW_MT2_OK;
    },out);
}
MT2FW_MT2_Result NativeStepKernel::unbind(uint64_t lease,MT2FW_MT2_StepReceipt* out){
    return control([&]{
        if(!bound_||lease!=receipt_.lease||thread_()!=bound_thread_)return reject(MT2FW_MT2_INVALID,"Native step retirement lease or thread differs");
        retiring_=true;
        if(entered_||receipt_.callbacks_inflight)return reject(MT2FW_MT2_BUSY,"Native step callback or original is still in flight");
        spec_.boundary=nullptr;spec_.service=nullptr;spec_.user=nullptr;bound_=false;
        // Callback retirement does not permit an unobserved original Go.
        if(receipt_.phase!=MT2FW_MT2_STEP_FAILED)receipt_.phase=MT2FW_MT2_STEP_RETIRING;
        return MT2FW_MT2_OK;
    },out);
}
MT2FW_MT2_Result MT2FW_CALL NativeStepKernel::next_call(void* context,uint64_t invocation,MT2FW_MT2_StepReceipt* out){
    if(!context)return MT2FW_MT2_INVALID;
    return static_cast<NativeStepKernel*>(context)->next(invocation,out);
}
MT2FW_MT2_Result NativeStepKernel::next(uint64_t invocation,MT2FW_MT2_StepReceipt* out){
    return control([&]{
        if(!entered_||retiring_||!bound_||invocation!=invocation_||thread_()!=bound_thread_)return reject(MT2FW_MT2_INVALID,"Native step continuation is stale, retired or outside its callback/thread");
        if(next_attempted_){fail("Native step continuation was attempted twice; no replay");return MT2FW_MT2_FAILED;}
        next_attempted_=true;current(true);receipt_.phase=MT2FW_MT2_STEP_ENTERED;
        MT2FW_MT2_StepBoundary frame{sizeof(frame),receipt_.actual_role,receipt_.lease,spec_.session,receipt_.world_epoch,next_tick_,invocation_,bound_thread_,spec_.dt_bits,0};
        advance_(frame,receipt_);
        require(!retiring_&&receipt_.phase!=MT2FW_MT2_STEP_FAILED,"Native step retired or reentered during the original");
        require(receipt_.lease==frame.lease&&receipt_.session==frame.session&&receipt_.world_epoch==frame.world_epoch&&receipt_.invocation==frame.invocation&&
            receipt_.actual_role==frame.actual_role&&receipt_.thread==frame.thread&&receipt_.tick==frame.tick,
            "Native step driver changed immutable boundary identity");
        require(receipt_.original_attempted==1&&receipt_.original_returned==1&&receipt_.timer_calls==1&&receipt_.dt_bits==spec_.dt_bits&&receipt_.workers_held==1,
            "Native step original/Timer/held proof is incomplete");
        current(true);receipt_.outcome=MT2FW_MT2_STEP_CONFIRMED;
        return MT2FW_MT2_OK;
    },out);
}
MT2FW_MT2_StepReceipt NativeStepKernel::boundary(){
    if(entered_||servicing_){fail("Native step boundary reentered");return receipt_;}
    if(receipt_.phase!=MT2FW_MT2_STEP_HELD||retiring_||!bound_)return receipt_;
    try{
        // Framework admission precedes the ordinary live membership proof.
        if(recovery_boundary_){validate_recovery_boundary();if(recovery_boundary_())return receipt_;}
        auto current_or_recovery=[&]{
            try{current(true);return true;}
            catch(...){
                // Transport may change after the first cohort observation.
                // Only the still-idle outer hold can enter guarded recovery.
                if(recovery_boundary_){validate_recovery_boundary();if(recovery_boundary_())return false;}
                throw;
            }
        };
        if(!current_or_recovery())return receipt_;
        entered_=true;++receipt_.callbacks_inflight;
        const auto service=spec_.service(spec_.user,&receipt_);
        if(service==MT2FW_MT2_BUSY){
            // BUSY from the held service means it yielded before command work.
            // It never authorizes another service call or an original Step.
            --receipt_.callbacks_inflight;entered_=false;
            validate_recovery_boundary();
            require(recovery_boundary_&&recovery_boundary_(),"Native step held service yielded without an actual recovery interruption");
            return receipt_;
        }
        if(service!=MT2FW_MT2_OK)throw std::runtime_error("Native step held service failed result="+std::to_string(service));
        require(!retiring_&&(receipt_.phase==MT2FW_MT2_STEP_HELD||(exiting_&&receipt_.phase==MT2FW_MT2_STEP_RETIRING)),"Native step retired or failed during held service");
        if(exiting_){receipt_.phase=MT2FW_MT2_STEP_RETIRING;--receipt_.callbacks_inflight;entered_=false;return receipt_;}
        --receipt_.callbacks_inflight;entered_=false;
        if(!current_or_recovery())return receipt_;
        entered_=true;++receipt_.callbacks_inflight;
        require(next_invocation_!=std::numeric_limits<uint64_t>::max(),"Native step invocation space exhausted");
        invocation_=next_invocation_++;next_attempted_=false;
        receipt_.invocation=invocation_;
        receipt_.tick=next_tick_;
        receipt_.original_attempted=receipt_.original_returned=receipt_.timer_calls=0;receipt_.outcome=MT2FW_MT2_STEP_NOT_ATTEMPTED;
        MT2FW_MT2_StepBoundary frame{sizeof(frame),receipt_.actual_role,receipt_.lease,spec_.session,receipt_.world_epoch,next_tick_,invocation_,bound_thread_,spec_.dt_bits,0};
        const auto disposition=spec_.boundary(spec_.user,&frame,next_call,this);
        require(!retiring_&&receipt_.phase!=MT2FW_MT2_STEP_FAILED,"Native step retired or failed inside boundary callback");
        if(disposition==MT2FW_MT2_STEP_KEEP_HELD)require(!next_attempted_,"Native step claimed held after an original attempt");
        else if(disposition==MT2FW_MT2_STEP_COMPLETED){
            require(next_attempted_&&receipt_.outcome==MT2FW_MT2_STEP_CONFIRMED,"Native step completion lacks actual original proof");
            current(true);receipt_.completed_tick=next_tick_;require(next_tick_<std::numeric_limits<uint64_t>::max()-1,"Native step tick space exhausted");
            ++next_tick_;
        }else throw std::runtime_error("Native step consumer reported unknown command effects");
        receipt_.phase=MT2FW_MT2_STEP_HELD;
    }catch(const std::exception& error){fail(error.what());}
    catch(...){fail("Native step callback raised an unknown exception");}
    if(receipt_.callbacks_inflight)--receipt_.callbacks_inflight;
    entered_=false;invocation_=0;return receipt_;
}
void NativeStepKernel::lifecycle(const MT2FW_MT2_StepReceipt& value){
    require(!entered_&&!servicing_&&bound_&&thread_()==bound_thread_,"Step lifecycle publication is outside its outer boundary");
    require(value.seed_attempted<=1&&value.seed_returned<=value.seed_attempted&&value.bootstrap_held<=value.seed_returned&&
        value.generation&&(!receipt_.generation||receipt_.generation==value.generation),"Step lifecycle acknowledgement is invalid or changed generation");
    require(value.seed_attempted>=receipt_.seed_attempted&&value.seed_returned>=receipt_.seed_returned&&value.bootstrap_held>=receipt_.bootstrap_held,
        "Step lifecycle acknowledgement went backwards");
    require(value.bootstrap_coverage==(value.bootstrap_held?MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS:0u),
        "Step bootstrap coverage is missing, unknown or present before bootstrap");
    receipt_.bootstrap_coverage=value.bootstrap_coverage;
    receipt_.seed_attempted=value.seed_attempted;receipt_.seed_returned=value.seed_returned;
    receipt_.bootstrap_held=value.bootstrap_held;receipt_.generation=value.generation;
    receipt_.workers_held=value.workers_held;
    if(value.bootstrap_held){
        require(value.world_epoch&&std::any_of(std::begin(value.initial_witness_sha256),std::end(value.initial_witness_sha256),[](auto b){return b!=0;}),
            "Step bootstrap publication lacks world epoch or canonical witness");
        receipt_.world_epoch=value.world_epoch;
        std::copy(std::begin(value.initial_witness_sha256),std::end(value.initial_witness_sha256),receipt_.initial_witness_sha256);
    }
    if(receipt_.phase==MT2FW_MT2_STEP_ARMED&&value.phase==MT2FW_MT2_STEP_LOADING)receipt_.phase=MT2FW_MT2_STEP_LOADING;
}
MT2FW_MT2_StepReceipt NativeStepKernel::maintenance(){
    // A failed lease may refresh local UI without borrowing Next or requiring
    // a live peer. Native execution uncertainty and its first cause stay intact.
    if(receipt_.phase==MT2FW_MT2_STEP_FAILED){
        if(!bound_||retiring_||!spec_.service){terminal_error_="Terminal service callback owner is retired";return receipt_;}
        if(!bound_thread_||thread_()!=bound_thread_){terminal_error_="Terminal service requires its bound thread";return receipt_;}
        if(entered_||servicing_||invocation_||receipt_.callbacks_inflight){terminal_error_="Terminal service reentered or native callback remains in flight";return receipt_;}
        if(receipt_.workers_held!=1){terminal_error_="Terminal service lacks confirmed held workers";return receipt_;}
        terminal_error_.clear();servicing_=true;++receipt_.callbacks_inflight;
        try{
            const auto result=spec_.service(spec_.user,&receipt_);
            require(result==MT2FW_MT2_OK,"Terminal local service failed result="+std::to_string(result));
            require(!retiring_&&bound_&&(receipt_.phase==MT2FW_MT2_STEP_FAILED||(exiting_&&receipt_.phase==MT2FW_MT2_STEP_RETIRING)),"Terminal service changed its retained lease");
        }catch(const std::exception& error){terminal_error_=error.what();}
        catch(...){terminal_error_="Terminal local service raised an unknown exception";}
        --receipt_.callbacks_inflight;servicing_=false;return receipt_;
    }
    if(entered_||servicing_){fail("Step startup service reentered");return receipt_;}
    if(!bound_||retiring_||(receipt_.phase!=MT2FW_MT2_STEP_ARMED&&receipt_.phase!=MT2FW_MT2_STEP_LOADING))return receipt_;
    try{
        current(false);servicing_=true;++receipt_.callbacks_inflight;
        const auto result=spec_.service(spec_.user,&receipt_);
        require(result==MT2FW_MT2_OK,"Step startup service failed result="+std::to_string(result));
        require(!retiring_&&receipt_.phase!=MT2FW_MT2_STEP_FAILED,"Step retired or failed in startup service");
    }catch(const std::exception& error){fail(error.what());}
    catch(...){fail("Step startup service raised an unknown exception");}
    if(receipt_.callbacks_inflight)--receipt_.callbacks_inflight;
    servicing_=false;return receipt_;
}
void NativeStepKernel::validate_save_request(){
    require(bound_&&!retiring_&&!exiting_&&spec_.save_request&&receipt_.bootstrap_held==1&&receipt_.world_epoch&&
        (receipt_.phase==MT2FW_MT2_STEP_HELD||receipt_.phase==MT2FW_MT2_STEP_ENTERED),
        "Native Save request lacks its active ordered world");
    const bool entered=receipt_.phase==MT2FW_MT2_STEP_ENTERED;
    if(entered)require(entered_&&!servicing_&&next_attempted_&&invocation_&&receipt_.invocation==invocation_&&
        receipt_.callbacks_inflight==1&&receipt_.original_attempted==1&&receipt_.original_returned==0&&receipt_.workers_held==0,
        "Native Save intent lacks its exact in-flight original invocation");
    const auto proof=current(!entered);
    require(proof.world_epoch==receipt_.world_epoch,"Native Save request world epoch changed");
}
void NativeStepKernel::validate_native_exit_request()const{
    require(bound_&&!invocation_&&thread_()==bound_thread_&&!retiring_&&receipt_.callbacks_inflight==1&&receipt_.workers_held==1&&
        ((entered_&&!servicing_&&receipt_.phase==MT2FW_MT2_STEP_HELD)||
         (!entered_&&servicing_&&(receipt_.phase==MT2FW_MT2_STEP_FAILED||
            (recovery_service_&&receipt_.phase==MT2FW_MT2_STEP_HELD)))),
        "Step native exit requires its quiescent held service before Next");
}
void NativeStepKernel::validate_recovery_boundary(bool service)const{
    require(bound_&&!retiring_&&!exiting_&&thread_()==bound_thread_&&!entered_&&!invocation_&&
        receipt_.phase==MT2FW_MT2_STEP_HELD&&receipt_.workers_held==1&&
        receipt_.completed_tick<std::numeric_limits<uint64_t>::max()&&next_tick_==receipt_.completed_tick+1&&
        (service?(servicing_&&recovery_service_&&receipt_.callbacks_inflight==1):(!servicing_&&receipt_.callbacks_inflight==0)),
        "Recovery boundary has an active invocation, unknown result, wrong thread or incomplete local tick");
}
void NativeStepKernel::recovery_service(const std::function<void()>& call){
    validate_recovery_boundary();servicing_=recovery_service_=true;++receipt_.callbacks_inflight;
    try{call();}catch(...){--receipt_.callbacks_inflight;servicing_=recovery_service_=false;throw;}
    --receipt_.callbacks_inflight;servicing_=recovery_service_=false;
}
void NativeStepKernel::native_exit_requested(){
    validate_native_exit_request();
    exiting_=true;receipt_.phase=MT2FW_MT2_STEP_RETIRING;receipt_.outcome=MT2FW_MT2_STEP_NOT_ATTEMPTED;
    receipt_.original_attempted=receipt_.original_returned=receipt_.timer_calls=0;receipt_.invocation=0;
}
bool NativeStepKernel::reset_after_world_exit(){
    if(bound_||entered_||servicing_||receipt_.callbacks_inflight)return false;
    const auto proof=inspect_(owner_,spec_);
    if(!proof.world_exited||!proof.quiescent||thread_()!=bound_thread_)return false;
    receipt_={sizeof(receipt_),MT2FW_MT2_STEP_PASSIVE};bound_thread_=0;retiring_=exiting_=false;spec_={};cause_.clear();return true;
}
}
