#include "native_services.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

namespace mt2fw::mt2 {
namespace {
void error(Plugin& plugin,const std::string& message){plugin.runtime->log(plugin.manifest.id,"error",message);}
uint64_t thread_id(){
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return std::hash<std::thread::id>{}(std::this_thread::get_id());
#endif
}
}
NativeServices::NativeServices(fs::path game,fs::path profile,Lookup lookup):game_(std::move(game)),profile_(std::move(profile)),lookup_(std::move(lookup)){frame_slots_.push_back(std::make_unique<FrameRun>());}
void NativeServices::backend(Install install,Remove remove){install_=std::move(install);remove_=std::move(remove);}
void NativeServices::reserve(const std::string& name){if(targets_.contains(name))throw std::runtime_error("Native hook name duplicated: "+name);targets_[name]={nullptr,true};}
void NativeServices::target(const std::string& name,void* address){
    if(!address||targets_.contains(name))throw std::runtime_error("Native hook target absent or duplicated: "+name);
    for(auto& [key,value]:targets_)if(value.address==address)throw std::runtime_error("Native hook address already catalogued: "+key);
    targets_[name]={address,false};
}
NativeServices::Owner* NativeServices::owner(void* context){auto result=static_cast<Owner*>(context);return result&&result->alive.load(std::memory_order_acquire)?result:nullptr;}
MT2FW_MT2_Result NativeServices::query(const MT2FW_Host* host,uint32_t version,MT2FW_MT2_ServiceV1* output,uint32_t capacity){
    if(!output||capacity<sizeof(*output)||version!=MT2FW_MT2_SERVICE_VERSION)return MT2FW_MT2_INVALID;
    auto plugin=lookup_(host);if(!plugin||!plugin->active||plugin->staging||plugin->manifest.type!="native")return MT2FW_MT2_UNAVAILABLE;
    try{
        auto& stored=owners_[plugin];if(!stored){
            stored=std::make_unique<Owner>();stored->service=this;stored->plugin=plugin;
            stored->game=path_text(game_);stored->profile=path_text(profile_);stored->game_data=path_text(profile_/"game-data");
            stored->directory=path_text(plugin->directory);stored->data=path_text(confined(profile_,"data/"+plugin->manifest.id));
        }
        auto& value=*stored;if(!value.alive.load())return MT2FW_MT2_UNAVAILABLE;
        *output={sizeof(*output),MT2FW_MT2_SERVICE_VERSION,&value,value.game.c_str(),value.profile.c_str(),value.game_data.c_str(),value.directory.c_str(),value.data.c_str(),
            frame_subscribe,frame_unsubscribe,hook_install,hook_remove,section_register,scope_begin,scope_end,capture_start,capture_read,capture_stop,section_read};
        return MT2FW_MT2_OK;
    }catch(const std::exception& problem){error(*plugin,std::string("Native service query failed: ")+problem.what());return MT2FW_MT2_FAILED;}
}
uint64_t MT2FW_CALL NativeServices::frame_subscribe(void* context,MT2FW_MT2_FrameBegin begin,MT2FW_MT2_FrameEnd end,void* user){
    auto value=owner(context);if(!value||!begin||!end)return 0;
    try{auto& service=*value->service;if(service.pairs_.size()>=128){error(*value->plugin,"Native frame subscription budget exceeded (128)");return 0;}
        auto token=service.next_token_++;service.pairs_.push_back({token,value,begin,end,user});return token;
    }catch(const std::exception& problem){error(*value->plugin,std::string("Native frame subscription failed: ")+problem.what());return 0;}
}
MT2FW_MT2_Result MT2FW_CALL NativeServices::frame_unsubscribe(void* context,uint64_t token){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;auto& pairs=value->service->pairs_;
    auto found=std::find_if(pairs.begin(),pairs.end(),[&](auto& pair){return pair.owner==value&&pair.token==token;});
    if(found==pairs.end())return MT2FW_MT2_INVALID;
    pairs.erase(found);return MT2FW_MT2_OK;
}
void NativeServices::begin_frame(void* game,float dt){
    if(frame_depth_>=64)throw std::runtime_error("Native update nesting budget exceeded (64)");
    if(frame_depth_==frame_slots_.size())frame_slots_.push_back(std::make_unique<FrameRun>());
    auto& run=*frame_slots_[frame_depth_++];auto& frame=run.frame;run.count=0;
    frame.game=game;frame.dt=dt;frame.original_returned=0;frame.id=++next_frame_;current_frame_.store(frame.id);
    std::array<Pair,128> snapshot{};auto count=pairs_.size();std::copy(pairs_.begin(),pairs_.end(),snapshot.begin());
    auto epoch=active_capture_.load(std::memory_order_acquire);if(epoch)record(MT2FW_MT2_FRAME_BEGIN,0,frame.id,epoch);
    for(size_t i=0;i<count;++i){auto& pair=snapshot[i];
        if(!pair.owner->alive.load()||!pair.owner->plugin->active)continue;
        auto& plugin=*pair.owner->plugin;++plugin.native_pins;
        try{
            if(pair.begin(pair.user,&frame))run.entered[run.count++]=pair;
            else plugin.runtime->release_native(plugin);
        }catch(const std::exception& problem){plugin.runtime->disable(plugin,std::string("Native frame begin failed: ")+problem.what());plugin.runtime->release_native(plugin);}
        catch(...){plugin.runtime->disable(plugin,"Native frame begin failed: unknown native exception");plugin.runtime->release_native(plugin);}
    }
}
void NativeServices::end_frame(bool original_returned){
    if(!frame_depth_)throw std::runtime_error("Native update end has no matching begin");
    auto& run=*frame_slots_[frame_depth_-1];auto& frame=run.frame;frame.original_returned=original_returned?1:0;
    for(size_t i=run.count;i>0;--i){auto& pair=run.entered[i-1];
        auto& plugin=*pair.owner->plugin;
        try{pair.end(pair.user,&frame);}catch(const std::exception& problem){plugin.runtime->disable(plugin,std::string("Native frame end failed: ")+problem.what());}
        catch(...){plugin.runtime->disable(plugin,"Native frame end failed: unknown native exception");}
        plugin.runtime->release_native(plugin);
    }
    run.count=0;
    auto epoch=active_capture_.load(std::memory_order_acquire);if(epoch)record(MT2FW_MT2_FRAME_END,0,frame.id,epoch);
    --frame_depth_;if(frame_depth_)current_frame_.store(frame_slots_[frame_depth_-1]->frame.id);
}
MT2FW_MT2_Result MT2FW_CALL NativeServices::hook_install(void* context,const char* name,void* detour,void** original,uint64_t* token){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;
    if(!name||!detour||!original||!token)return MT2FW_MT2_INVALID;
    auto& service=*value->service;
    try{
        auto found=service.targets_.find(name);
        if(found==service.targets_.end()||found->second.reserved){error(*value->plugin,std::string("Native hook target is unknown or shared: ")+name);return MT2FW_MT2_UNAVAILABLE;}
        if(!service.install_||!service.remove_)return MT2FW_MT2_UNAVAILABLE;
        for(auto& hook:service.hooks_)if(hook.name==name){error(*value->plugin,std::string("Native hook target already owned: ")+name);return MT2FW_MT2_BUSY;}
        auto id=service.next_token_++;
        service.hooks_.push_back({id,value,name});
        std::string cause;
        bool installed=false;
        try{installed=service.install_(found->second.address,detour,original,cause);}
        catch(const std::exception& problem){cause=problem.what();}
        catch(...){cause="unknown native exception";}
        if(!installed){
            error(*value->plugin,std::string("Native hook install failed: ")+name+": "+cause);
            auto rollback=hook_remove(context,id);
            if(rollback!=MT2FW_MT2_OK)error(*value->plugin,std::string("Native hook rollback retained ownership: ")+name);
            return MT2FW_MT2_FAILED;
        }
        *token=id;return MT2FW_MT2_OK;
    }catch(const std::exception& problem){error(*value->plugin,std::string("Native hook install failed: ")+problem.what());return MT2FW_MT2_FAILED;}
}
MT2FW_MT2_Result MT2FW_CALL NativeServices::hook_remove(void* context,uint64_t token){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;auto& service=*value->service;
    auto found=std::find_if(service.hooks_.begin(),service.hooks_.end(),[&](auto& hook){return hook.owner==value&&hook.token==token;});
    if(found==service.hooks_.end())return MT2FW_MT2_INVALID;
    try{std::string cause;if(!service.remove_(service.targets_.at(found->name).address,cause)){error(*value->plugin,"Native hook remove failed: "+found->name+": "+cause);return MT2FW_MT2_FAILED;}
        service.hooks_.erase(found);return MT2FW_MT2_OK;
    }catch(const std::exception& problem){error(*value->plugin,std::string("Native hook remove failed: ")+problem.what());return MT2FW_MT2_FAILED;}
}
bool NativeServices::retire(Plugin& plugin){
    auto found=owners_.find(&plugin);if(found==owners_.end())return true;auto& value=*found->second;
    bool complete=true;
    for(auto it=hooks_.begin();it!=hooks_.end();){
        if(it->owner!=&value){++it;continue;}
        std::string cause;
        try{if(remove_&&remove_(targets_.at(it->name).address,cause)){it=hooks_.erase(it);continue;}}
        catch(const std::exception& problem){cause=problem.what();}
        error(plugin,"Native hook retirement failed: "+it->name+": "+cause);complete=false;++it;
    }
    bool was_alive=value.alive.exchange(false,std::memory_order_acq_rel);
    std::erase_if(pairs_,[&](auto& pair){return pair.owner==&value;});
    if(was_alive&&active_capture_.load())dropped_.fetch_add(1);
    {std::lock_guard lock(trace_gate_);if(capture_owner_==&value)active_capture_.store(0,std::memory_order_release);}
    return complete;
}
uint32_t MT2FW_CALL NativeServices::section_register(void* context,const char* name){
    auto value=owner(context);if(!value||!name)return 0;
    try{auto length=std::strlen(name);if(!length||length>=MT2FW_MT2_NAME_BYTES||value->plugin->manifest.id.size()>=MT2FW_MT2_ID_BYTES)return 0;
        auto& service=*value->service;auto count=service.section_count_.load();
        for(uint32_t i=0;i<count;++i)if(service.sections_[i].owner==value&&service.sections_[i].name==name)return i+1;
        if(count==service.sections_.size()){error(*value->plugin,"Native trace section budget exceeded (4096)");return 0;}
        service.sections_[count]={value,name};service.section_count_.store(count+1,std::memory_order_release);return count+1;
    }catch(const std::exception& problem){error(*value->plugin,std::string("Native trace section registration failed: ")+problem.what());return 0;}
}
void NativeServices::record(uint32_t kind,uint32_t section,uint64_t token,uint64_t epoch) noexcept{
    std::unique_lock lock(trace_gate_,std::try_to_lock);
    if(!lock.owns_lock()){dropped_.fetch_add(1);return;}
    if(epoch!=active_capture_.load(std::memory_order_acquire))return;
    ++sequence_;
    if(trace_count_==trace_.size()){dropped_.fetch_add(1);return;}
    auto now=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    auto slot=(trace_head_+trace_count_)%trace_.size();
    trace_[slot]={epoch,sequence_,now,thread_id(),token,current_frame_.load(),section,kind};++trace_count_;
}
MT2FW_MT2_Scope MT2FW_CALL NativeServices::scope_begin(void* context,uint32_t section){
    auto raw=static_cast<Owner*>(context);if(!raw)return {};auto& service=*raw->service;
    auto epoch=service.active_capture_.load(std::memory_order_acquire);if(!epoch)return {};
    if(!raw->alive.load()||!section||section>service.section_count_.load(std::memory_order_acquire)||service.sections_[section-1].owner!=raw)return {};
    auto token=service.next_scope_.fetch_add(1);service.record(MT2FW_MT2_SCOPE_BEGIN,section,token,epoch);return {epoch,token};
}
void MT2FW_CALL NativeServices::scope_end(void* context,uint32_t section,MT2FW_MT2_Scope scope){
    if(!scope.token)return;auto raw=static_cast<Owner*>(context);if(!raw)return;auto& service=*raw->service;
    if(!scope.epoch||scope.epoch!=service.active_capture_.load(std::memory_order_acquire))return;
    if(!section||section>service.section_count_.load(std::memory_order_acquire)||service.sections_[section-1].owner!=raw)return;
    service.record(MT2FW_MT2_SCOPE_END,section,scope.token,scope.epoch);
}
MT2FW_MT2_Result MT2FW_CALL NativeServices::capture_start(void* context,uint32_t capacity,uint64_t* epoch){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;if(!epoch||capacity<64||capacity>1048576)return MT2FW_MT2_INVALID;
    auto& service=*value->service;
    try{std::lock_guard lock(service.trace_gate_);if(service.active_capture_.load())return MT2FW_MT2_BUSY;
        service.trace_.assign(capacity,{});service.trace_head_=0;service.trace_count_=0;service.sequence_=0;service.dropped_.store(0);
        service.capture_owner_=value;*epoch=++service.capture_epoch_;service.active_capture_.store(*epoch,std::memory_order_release);return MT2FW_MT2_OK;
    }catch(const std::exception& problem){error(*value->plugin,std::string("Native trace capture start failed: ")+problem.what());return MT2FW_MT2_FAILED;}
}
MT2FW_MT2_Result MT2FW_CALL NativeServices::capture_read(void* context,MT2FW_MT2_TraceEvent* output,uint32_t capacity,MT2FW_MT2_TraceStatus* status){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;if(!status||status->size<sizeof(*status)||(!output&&capacity))return MT2FW_MT2_INVALID;
    auto& service=*value->service;std::lock_guard lock(service.trace_gate_);if(service.capture_owner_!=value)return MT2FW_MT2_UNAVAILABLE;
    auto count=uint32_t(std::min(size_t(capacity),service.trace_count_));
    for(uint32_t i=0;i<count;++i)output[i]=service.trace_[(service.trace_head_+i)%service.trace_.size()];
    if(count)service.trace_head_=(service.trace_head_+count)%service.trace_.size();service.trace_count_-=count;
    *status={sizeof(*status),service.active_capture_.load()?1u:0u,service.capture_epoch_,service.dropped_.load(),service.trace_count_,count,0};return MT2FW_MT2_OK;
}
MT2FW_MT2_Result MT2FW_CALL NativeServices::capture_stop(void* context){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;auto& service=*value->service;
    std::lock_guard lock(service.trace_gate_);if(service.capture_owner_!=value)return MT2FW_MT2_UNAVAILABLE;
    service.active_capture_.store(0,std::memory_order_release);return MT2FW_MT2_OK;
}
MT2FW_MT2_Result MT2FW_CALL NativeServices::section_read(void* context,uint32_t section,MT2FW_MT2_Section* result){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;
    if(!result||result->size<sizeof(*result)||!section||section>value->service->section_count_.load(std::memory_order_acquire))return MT2FW_MT2_INVALID;
    auto& source=value->service->sections_[section-1];*result={sizeof(*result),section};
    std::memcpy(result->owner,source.owner->plugin->manifest.id.c_str(),source.owner->plugin->manifest.id.size()+1);
    std::memcpy(result->name,source.name.c_str(),source.name.size()+1);return MT2FW_MT2_OK;
}
}
