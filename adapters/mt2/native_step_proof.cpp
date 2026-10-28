#include "native_step_proof.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace mt2fw::mt2 {
namespace {
void require(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
template<size_t N>std::string bounded(const char(&text)[N]){
    const auto end=static_cast<const char*>(std::memchr(text,0,N));
    require(end!=nullptr,"Step proof text is not terminated");return {text,end};
}
}
static StepNativeObservation read_proof(const MT2FW_Host* owner,const MT2FW_MT2_StepSpec& spec,uint64_t thread,uint32_t index,region_rng::Row* row){
    require(owner&&spec.size==sizeof(spec)&&spec.proof&&thread,"Step proof owner/callback/spec/thread is missing");
    MT2FW_MT2_StepProofQuery query{sizeof(query),MT2FW_MT2_STEP_DRAFT_VERSION,spec.session,spec.session_provider_token,thread,index,0};
    MT2FW_MT2_StepProofReply reply{};reply.size=sizeof(reply);
    const auto result=spec.proof(spec.proof_user,&query,&reply);
    const auto cause=bounded(reply.cause);
    require(result==MT2FW_MT2_OK,"Step proof callback failed result="+std::to_string(result)+": "+cause);
    require(reply.size==sizeof(reply),"Step proof reply size differs");
    require(reply.phase>=MT2FW_MT2_STEP_PROOF_PRELOAD&&reply.phase<=MT2FW_MT2_STEP_PROOF_EXITED,"Step proof phase is unavailable: "+cause);
    require(reply.session==query.session&&reply.session_provider_token==query.session_provider_token&&reply.thread==query.thread,
        "Step proof returned a different current session/provider/thread");
    require(reply.seed_present==1&&reply.frozen_peer_mask>0&&reply.frozen_peer_mask<=255&&reply.resume_workers_known==0&&reply.resume_workers==0&&
        std::all_of(std::begin(reply.initial_witness_sha256),std::end(reply.initial_witness_sha256),[](auto v){return v==0;})&&
        std::all_of(std::begin(reply.reserved),std::end(reply.reserved),[](unsigned char v){return v==0;}),"Step proof flags, shared seed or reserved bytes are invalid");
    StepNativeObservation out;auto& id=out.identity;id.owner=owner;id.session=reply.session;id.provider_token=reply.session_provider_token;
    require((reply.region_rng_mode==1&&reply.region_rng_count==0)||(reply.region_rng_mode==2&&reply.region_rng_count>0&&reply.region_rng_count<=4096),"Step Region RNG mode/count differs");
    require(reply.region_rng_reserved==0&&reply.region_rng_index==index&&
        std::any_of(std::begin(reply.region_rng_sha256),std::end(reply.region_rng_sha256),[](auto v){return v!=0;}),"Step Region RNG row index/reserved/commitment differs");
    if(row){
        require(index<reply.region_rng_count,"Step Region RNG row is outside preserved census");
        row->id=reply.region_rng_id;std::copy(std::begin(reply.region_rng_words),std::end(reply.region_rng_words),row->value.words.begin());row->value.extra=reply.region_rng_extra;
    }else require(reply.region_rng_id==0&&reply.region_rng_extra==0&&
        std::all_of(std::begin(reply.region_rng_words),std::end(reply.region_rng_words),[](auto v){return v==0;}),"Step Region RNG metadata contains an unrequested row");
    id.region_rng_mode=reply.region_rng_mode;id.region_rng_count=reply.region_rng_count;
    std::copy(std::begin(reply.region_rng_sha256),std::end(reply.region_rng_sha256),id.region_rng_sha256.begin());
    id.generation=reply.generation;id.snapshot_bytes=reply.snapshot_bytes;id.thread=reply.thread;id.agreed_seed=reply.agreed_seed;
    id.peer_mask=uint8_t(reply.frozen_peer_mask);id.snapshot_uuid=bounded(reply.snapshot_uuid);
    for(auto byte:reply.snapshot_sha256){id.snapshot_sha256+="0123456789abcdef"[byte>>4];id.snapshot_sha256+="0123456789abcdef"[byte&15];}
    out.phase=static_cast<StepNativeObservation::Phase>(reply.phase);out.world_epoch=reply.world_epoch;
    out.game=uintptr_t(reply.game);out.mode=uintptr_t(reply.mode);out.state=uintptr_t(reply.state);out.world=uintptr_t(reply.world);out.map=uintptr_t(reply.map);
    std::copy(std::begin(reply.initial_witness_sha256),std::end(reply.initial_witness_sha256),out.witness.begin());
    if(reply.resume_workers_known)out.resume_workers=reply.resume_workers!=0;
    out.cause=cause;return out;
}
StepNativeObservation read_step_proof(const MT2FW_Host* owner,const MT2FW_MT2_StepSpec& spec,uint64_t thread){
    return read_proof(owner,spec,thread,UINT32_MAX,nullptr);
}
std::vector<region_rng::Row> NativeStepDriver::region_rng_rows()const{
    require(c_proof_&&frozen_&&source_&&!failed()&&native_.thread()==frozen_->thread,"Step Region RNG copy lacks frozen C proof/thread");
    std::vector<region_rng::Row> rows;
    for(uint32_t i=0;i<frozen_->region_rng_count;++i){
        region_rng::Row row;const auto value=read_proof(frozen_->owner,proof_spec_,native_.thread(),i,&row);
        require(value.phase==StepNativeObservation::Phase::Preload&&value.identity==*frozen_,"Step Region RNG row changed its frozen preload identity");rows.push_back(row);
    }
    region_rng::valid_rows(frozen_->region_rng_mode,rows);return rows;
}
}
