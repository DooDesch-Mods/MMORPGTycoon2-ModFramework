#include "native_patches.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace mt2fw::mt2 {
namespace {
std::string copied(const char* input,size_t maximum,const char* field){
    if(!input)throw std::runtime_error(std::string("Native patch ")+field+" is absent");
    size_t size=0;while(size<=maximum&&input[size])++size;
    if(!size||size>maximum)throw std::runtime_error(std::string("Native patch ")+field+" length is invalid");
    return {input,size};
}
const char* target_name(uint32_t value){
    switch(value){case MT2FW_MT2_PATCH_GAME_UPDATE:return "game.update";case MT2FW_MT2_PATCH_OBJECTS_CREATE:return "objects.create";case MT2FW_MT2_PATCH_API_TIME_SET:return "api.time.set";default:throw std::runtime_error("Native patch target is unsupported");}
}
uint64_t number(const Json& value,const char* key){
    if(!value.contains(key))return 0;
    auto& item=value.at(key);if(item.is_number_unsigned())return item.get<uint64_t>();
    if(item.is_number_integer()){auto result=item.get<int64_t>();if(result>=0)return uint64_t(result);}
    if(item.is_string()){size_t used=0;auto text=item.get<std::string>();auto result=std::stoull(text,&used);if(used==text.size())return result;}
    throw std::runtime_error(std::string("Native patch metadata is invalid: ")+key);
}
MT2FW_MT2_PatchEvent event(const PatchCall& call,uint32_t target,uint32_t phase){
    MT2FW_MT2_PatchEvent result{};result.size=sizeof(result);result.target=target;result.phase=phase;
    auto& data=call.data;
    result.invocation=number(data,"invocation");result.generation=number(data,"generation");result.epoch=number(data,"epoch");result.thread=number(data,"thread");
    result.original_returned=data.value("original_returned",false)?1u:0u;
    if(target==MT2FW_MT2_PATCH_GAME_UPDATE)result.dt=data.value("dt",0.f);
    if(target==MT2FW_MT2_PATCH_OBJECTS_CREATE){result.create_flag=data.value("flag",false)?1u:0u;result.result_present=data.value("result_available",false)?1u:0u;}
    if(target==MT2FW_MT2_PATCH_API_TIME_SET){result.paused=data.value("paused",false)?1u:0u;result.speed=data.value("speed",0.f);}
    return result;
}
}
NativePatches::NativePatches(Runtime& runtime,Thread thread,WorldProof proof):runtime_(runtime),thread_(std::move(thread)),world_proof_(std::move(proof)){}
void NativePatches::set_game_thread(uint64_t thread){std::lock_guard lock(gate_);if(!game_thread_)game_thread_=thread;else if(game_thread_!=thread)throw std::runtime_error("Native patch game thread identity changed");}
bool NativePatches::on_thread()const{return game_thread_&&thread_&&thread_()==game_thread_;}
NativePatches::Owner& NativePatches::admit(void* context){
    if(!on_thread())throw std::runtime_error("Native patch call is outside the bound game thread");
    auto owner=static_cast<Owner*>(context);
    if(!owner||owner->service!=this||!owner->active||!owner->plugin->active||owner->plugin->staging)throw std::runtime_error("Native patch owner is inactive");
    return *owner;
}
MT2FW_MT2_Result NativePatches::receipt(MT2FW_MT2_PatchReceipt* output,MT2FW_MT2_Result result,const std::string& cause){
    if(output&&output->size>=sizeof(*output)){*output={sizeof(*output),uint32_t(result)};auto detail=cause.empty()&&result==MT2FW_MT2_BUSY?std::string("Native patch owner still has active callbacks or a retained binding"):cause;auto size=std::min(detail.size(),sizeof(output->cause)-1);std::memcpy(output->cause,detail.data(),size);output->cause[size]=0;}return result;
}
MT2FW_MT2_Result NativePatches::query(const MT2FW_Host* host,uint32_t version,MT2FW_MT2_PatchesV1* output,uint32_t capacity){
    if(!output||capacity<sizeof(*output)||version!=MT2FW_MT2_PATCHES_VERSION)return MT2FW_MT2_INVALID;
    std::lock_guard lock(gate_);auto plugin=runtime_.native_plugin(host);
    if(!plugin||!plugin->active||plugin->staging||plugin->manifest.type!="native"||!on_thread())return MT2FW_MT2_UNAVAILABLE;
    auto& owner=owners_[plugin];if(!owner){owner=std::make_unique<Owner>();owner->service=this;owner->plugin=plugin;}
    if(!owner->active)return MT2FW_MT2_UNAVAILABLE;
    *output={sizeof(*output),MT2FW_MT2_PATCHES_VERSION,owner.get(),
        [](void* context,const MT2FW_MT2_PatchSpec* spec,MT2FW_MT2_PatchReceipt* out){return boundary(out,[&]{auto& value=service(context);std::lock_guard lock(value.gate_);return value.add(value.admit(context),spec);});},
        [](void* context,const char* id,MT2FW_MT2_PatchReceipt* out){return boundary(out,[&]{auto& value=service(context);std::lock_guard lock(value.gate_);return value.remove(value.admit(context),id);});},
        [](void* context,MT2FW_MT2_SessionProvider callback,void* user,uint64_t* token,MT2FW_MT2_PatchReceipt* out){return boundary(out,[&]{auto& value=service(context);std::lock_guard lock(value.gate_);return value.bind(value.admit(context),callback,user,token);});},
        [](void* context,uint64_t token,MT2FW_MT2_PatchReceipt* out){return boundary(out,[&]{auto& value=service(context);std::lock_guard lock(value.gate_);return value.unbind(value.admit(context),token);});}
    };return MT2FW_MT2_OK;
}
MT2FW_MT2_Result NativePatches::add(Owner& owner,const MT2FW_MT2_PatchSpec* input){
    if(!input||input->size<sizeof(*input)||input->mode>MT2FW_MT2_PATCH_MODIFY||(!input->on_before&&!input->on_after))throw std::runtime_error("Native patch specification is invalid");
    PatchSpec spec;spec.id=copied(input->id,MT2FW_MT2_PATCH_ID_BYTES-1,"id");spec.target=target_name(input->target);spec.mode=input->mode==MT2FW_MT2_PATCH_MODIFY?"modify":"observe";spec.priority=input->priority;
    if(owner.patches.contains(spec.id))throw std::runtime_error("Native patch id is already registered or awaiting retirement");
    auto references=[&](const char* const* values,uint32_t count,std::vector<std::string>& output){if(count>MT2FW_MT2_PATCH_MAX_ORDER||(count&&!values))throw std::runtime_error("Native patch ordering bounds are invalid");for(uint32_t i=0;i<count;++i)output.push_back(copied(values[i],MT2FW_MT2_ID_BYTES+MT2FW_MT2_PATCH_ID_BYTES,"ordering reference"));};
    references(input->before,input->before_count,spec.before);references(input->after,input->after_count,spec.after);
    auto before=input->on_before;auto after=input->on_after;auto user=input->user;auto target=input->target;auto modifying=input->mode==MT2FW_MT2_PATCH_MODIFY;
    PatchCallbacks callbacks;
    if(before)callbacks.before=[before,user,target,modifying](const PatchCall& call)->Json {
        auto payload=event(call,target,MT2FW_MT2_PATCH_BEFORE);MT2FW_MT2_PatchEdit edit{sizeof(edit),0,payload.paused,payload.speed};
        auto result=before(user,&payload,&edit);if(result!=MT2FW_MT2_OK)throw std::runtime_error("Native patch before returned error="+std::to_string(result));
        if(edit.size!=sizeof(edit)||edit.replace_arguments>1)throw std::runtime_error("Native patch edit shape is invalid");
        if(!edit.replace_arguments)return nullptr;
        if(!modifying||target!=MT2FW_MT2_PATCH_API_TIME_SET)throw std::runtime_error("Native patch argument replacement is unavailable for this target or mode");
        if(edit.paused>1||!std::isfinite(edit.speed)||edit.speed<0)throw std::runtime_error("Native patch time arguments require boolean paused and finite nonnegative speed");
        return {{"paused",edit.paused!=0},{"speed",edit.speed}};
    };
    if(after)callbacks.after=[after,user,target](const PatchCall& call){auto payload=event(call,target,MT2FW_MT2_PATCH_AFTER);auto result=after(user,&payload);if(result!=MT2FW_MT2_OK)throw std::runtime_error("Native patch after returned error="+std::to_string(result));};
    runtime_.add_patch(*owner.plugin,std::move(spec),std::move(callbacks));owner.patches.emplace(copied(input->id,MT2FW_MT2_PATCH_ID_BYTES-1,"id"),true);return MT2FW_MT2_OK;
}
MT2FW_MT2_Result NativePatches::remove(Owner& owner,const char* input){
    auto id=copied(input,MT2FW_MT2_PATCH_ID_BYTES-1,"id");auto found=owner.patches.find(id);if(found==owner.patches.end())throw std::runtime_error("Native patch id is not owned by this plugin");
    if(found->second){auto ready=runtime_.remove_patch(*owner.plugin,id);found->second=false;if(!ready)return MT2FW_MT2_BUSY;}
    if(runtime_.patches.busy(owner.plugin))return MT2FW_MT2_BUSY;owner.patches.erase(found);return MT2FW_MT2_OK;
}
MT2FW_MT2_Result NativePatches::bind(Owner& owner,MT2FW_MT2_SessionProvider callback,void* user,uint64_t* token){
    if(owner.plugin->manifest.id!=bridge_id)throw std::runtime_error("Session provider belongs to the supported native Multiplayer bridge only");
    if(!callback||!token)throw std::runtime_error("Session provider callback or token output is absent");
    if(provider_.owner)return MT2FW_MT2_BUSY;*token=next_token_++;provider_={&owner,callback,user,*token,true};return MT2FW_MT2_OK;
}
MT2FW_MT2_Result NativePatches::unbind(Owner& owner,uint64_t token){
    if(provider_.owner!=&owner||provider_.token!=token)throw std::runtime_error("Session provider token is not owned by this plugin");
    provider_.active=false;if(owner.inflight)return MT2FW_MT2_BUSY;provider_={};return MT2FW_MT2_OK;
}
bool NativePatches::retire(Plugin& plugin){
    std::lock_guard lock(gate_);auto found=owners_.find(&plugin);bool ready=runtime_.patches.retire(&plugin);
    if(found==owners_.end())return ready;auto& owner=*found->second;owner.active=false;
    if(provider_.owner==&owner){provider_.active=false;if(owner.inflight)return false;provider_={};}
    if(!ready||owner.inflight)return false;owner.patches.clear();return true;
}
bool NativePatches::busy()const{std::lock_guard lock(gate_);for(auto& [plugin,owner]:owners_)if(owner->inflight||runtime_.patches.busy(plugin))return true;return false;}
bool NativePatches::authority_result(uint32_t role,const std::string& reason,std::string& output){
    bool allowed=role==MT2FW_MT2_SESSION_STANDALONE;
    last_role_=role;if(last_reason_!=reason){runtime_.log("patches",allowed?"info":"warn",reason);last_reason_=reason;}output=reason;return allowed;
}
bool NativePatches::authorize(const std::string& operation,std::string& reason){
    std::lock_guard lock(gate_);
    if(operation!="api.time.set"&&operation!="api.gameplay.action")return authority_result(MT2FW_MT2_SESSION_UNKNOWN,"Patch mutation target is unsupported",reason);
    if(!on_thread())return authority_result(MT2FW_MT2_SESSION_UNKNOWN,"Patch authority rejected: caller is outside the bound game thread",reason);
    uint64_t epoch=0;std::string proof;
    bool proven=false;try{proven=world_proof_&&world_proof_(epoch,proof);}catch(const std::exception& error){proof=std::string("native world proof failed: ")+error.what();}catch(...){proof="native world proof failed: unknown native exception";}
    if(!proven||!epoch)return authority_result(MT2FW_MT2_SESSION_UNKNOWN,"Patch authority rejected: "+(proof.empty()?std::string("native world proof is unavailable"):proof),reason);
    Plugin* bridge=nullptr;
    for(auto& plugin:runtime_.plugins())if(plugin->manifest.type=="native"&&plugin->manifest.id==bridge_id){
        bool present=plugin->active||plugin->native_loaded||plugin->native_module||plugin->native_pins||plugin->native_retained||plugin->native_retirement_failed||plugin->native_shutdown_pending;
        if(present){if(bridge)return authority_result(MT2FW_MT2_SESSION_UNKNOWN,"Multiple present supported Multiplayer bridge instances have unknown session authority",reason);bridge=plugin.get();}
    }
    if(!bridge)return authority_result(MT2FW_MT2_SESSION_STANDALONE,"Standalone native world acknowledged; supported Multiplayer bridge is absent or fully unloaded",reason);
    if(!bridge->active||!bridge->native_loaded)return authority_result(MT2FW_MT2_SESSION_UNKNOWN,"Supported Multiplayer bridge is inactive or retained with unacknowledged session state",reason);
    if(!provider_.owner||!provider_.active||!provider_.owner->active||provider_.owner->plugin!=bridge)return authority_result(MT2FW_MT2_SESSION_UNKNOWN,"Multiplayer bridge is active without an acknowledged session provider",reason);
    if(provider_.owner->inflight)return authority_result(MT2FW_MT2_SESSION_UNKNOWN,"Session provider query is already in progress; nested authority is unknown",reason);
    auto provider=provider_;++provider.owner->inflight;MT2FW_MT2_SessionQuery query{sizeof(query),0,epoch,thread_()};MT2FW_MT2_SessionReply reply{sizeof(reply),MT2FW_MT2_SESSION_UNKNOWN,0};MT2FW_MT2_Result result=MT2FW_MT2_FAILED;std::string failure;
    try{result=provider.callback(provider.user,&query,&reply);}catch(const std::exception& error){failure=error.what();}catch(...){failure="unknown native exception";}
    --provider.owner->inflight;
    if(!failure.empty())return authority_result(MT2FW_MT2_SESSION_UNKNOWN,"Session provider failed: "+failure,reason);
    if(result!=MT2FW_MT2_OK)return authority_result(MT2FW_MT2_SESSION_UNKNOWN,"Session provider returned error="+std::to_string(result),reason);
    uint64_t after_epoch=0;std::string after_proof;
    bool still_proven=false;try{still_proven=world_proof_(after_epoch,after_proof);}catch(const std::exception& error){after_proof=error.what();}catch(...){after_proof="unknown native exception";}
    if(!provider_.active||provider_.token!=provider.token||!provider.owner->active||!bridge->active||!bridge->native_loaded||reply.size!=sizeof(reply)||reply.epoch!=epoch||!still_proven||after_epoch!=epoch)return authority_result(MT2FW_MT2_SESSION_UNKNOWN,"Session provider acknowledgement is stale or native world changed"+(after_proof.empty()?std::string():": "+after_proof),reason);
    if(reply.role==MT2FW_MT2_SESSION_HOST)return authority_result(reply.role,"Trusted Multiplayer provider acknowledged Host; shared Host-action bridge is unavailable, patch mutation denied",reason);
    if(reply.role==MT2FW_MT2_SESSION_STANDALONE)return authority_result(reply.role,"Trusted Multiplayer provider acknowledged disconnected standalone session",reason);
    if(reply.role==MT2FW_MT2_SESSION_CLIENT)return authority_result(reply.role,"Trusted Multiplayer provider reports Client; patch mutation denied",reason);
    return authority_result(MT2FW_MT2_SESSION_UNKNOWN,"Trusted Multiplayer provider reports unknown session authority",reason);
}
Json NativePatches::status()const{
    std::lock_guard lock(gate_);return {{"role",last_role_==MT2FW_MT2_SESSION_HOST?"host":last_role_==MT2FW_MT2_SESSION_STANDALONE?"standalone":last_role_==MT2FW_MT2_SESSION_CLIENT?"client":"unknown"},{"reason",last_reason_},{"provider_owner",provider_.owner?provider_.owner->plugin->manifest.id:""},{"provider_active",provider_.active},{"bound_thread",std::to_string(game_thread_)}};
}
}
