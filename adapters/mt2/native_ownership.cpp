#include "native_ownership.h"
#include <algorithm>
#include <cstring>
#include <cstddef>

namespace mt2fw::mt2 {
namespace {
constexpr const char* game_sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
template<size_t N>void text(char(&output)[N],const std::string& value){auto length=std::min(value.size(),N-1);std::memcpy(output,value.data(),length);output[length]=0;}
bool output_ok(MT2FW_MT2_FreeReceipt* output){return output&&output->size>=sizeof(*output);}
}
NativeOwnership::NativeOwnership(uintptr_t image,NativeRoutes& routes,std::function<Plugin*(const MT2FW_Host*)> lookup,Backend backend):image_(image),routes_(routes),lookup_(std::move(lookup)),backend_(std::move(backend)){errors_.reserve(32);commands_.reserve(64);inputs_.reserve(64);}
void NativeOwnership::report(Owner& owner,const std::string& cause){std::lock_guard lock(gate_);if(reported_==cause){++repeated_;return;}reported_=cause;if(errors_.size()<32)errors_.push_back({owner.plugin,cause});else ++repeated_;}
void NativeOwnership::boundary_error(void* context,const char* cause)noexcept{
    auto value=static_cast<Owner*>(context);
    try{if(value)value->service->report(*value,cause);else std::fprintf(stderr,"Native ownership: %s\n",cause);}
    catch(const std::exception& problem){std::fprintf(stderr,"Native ownership: %s; logging failed: %s\n",cause,problem.what());}
    catch(...){std::fprintf(stderr,"Native ownership: %s; unknown logging failure\n",cause);}
}
std::vector<std::pair<Plugin*,std::string>> NativeOwnership::drain_errors(){std::lock_guard lock(gate_);if(errors_.empty())return {};auto result=std::move(errors_);errors_.clear();errors_.reserve(32);return result;}
MT2FW_MT2_Result NativeOwnership::query(const MT2FW_Host* host,uint32_t version,MT2FW_MT2_OwnershipV1* output,uint32_t capacity){
    if(!output||capacity<sizeof(*output)||version!=MT2FW_MT2_OWNERSHIP_VERSION)return MT2FW_MT2_INVALID;
    auto plugin=lookup_(host);if(!plugin||!plugin->active||plugin->staging||plugin->manifest.type!="native")return MT2FW_MT2_UNAVAILABLE;
    std::lock_guard lock(gate_);auto& value=owners_[plugin];if(!value)value=std::make_unique<Owner>(Owner{this,plugin});if(!value->active)return MT2FW_MT2_UNAVAILABLE;
    *output={sizeof(*output),MT2FW_MT2_OWNERSHIP_VERSION,value.get(),
        [](void* c,const char* name,MT2FW_MT2_RouteReceipt* result){return boundary(c,[&]{auto value=owner(c);return value?value->service->routes_.disable(*value->plugin,name,result):MT2FW_MT2_UNAVAILABLE;});},
        [](void* c,const char* name,MT2FW_MT2_RouteReceipt* result){return boundary(c,[&]{auto value=owner(c);return value?value->service->routes_.resume(*value->plugin,name,result):MT2FW_MT2_UNAVAILABLE;});},
        [](void* c,MT2FW_MT2_FreeFn replacement,MT2FW_MT2_FreeReceipt* result){return boundary(c,[&]{return free_prepare(c,replacement,result);});},
        [](void* c,MT2FW_MT2_FreeReceipt* result){return boundary(c,[&]{return free_enable(c,result);});},
        [](void* c,MT2FW_MT2_FreeReceipt* result){return boundary(c,[&]{return free_disable(c,result);});},
        [](void* c,MT2FW_MT2_FreeReceipt* result){return boundary(c,[&]{return free_read(c,result);});},
        [](void* c,MT2FW_MT2_FreeReceipt* result){return boundary(c,[&]{return free_remove(c,result);});},
        [](void* c,MT2FW_MT2_CommandObserver callback,void* user)->uint64_t{try{return command_subscribe(c,callback,user);}catch(const std::exception& problem){boundary_error(c,problem.what());return 0;}catch(...){boundary_error(c,"Unknown command subscription exception");return 0;}},
        [](void* c,uint64_t token){return boundary(c,[&]{return command_unsubscribe(c,token);});},
        [](void* c,MT2FW_MT2_InputObserver callback,void* user)->uint64_t{try{return input_subscribe(c,callback,user);}catch(const std::exception& problem){boundary_error(c,problem.what());return 0;}catch(...){boundary_error(c,"Unknown input subscription exception");return 0;}},
        [](void* c,uint64_t token){return boundary(c,[&]{return input_unsubscribe(c,token);});}
    };return MT2FW_MT2_OK;
}
void NativeOwnership::receipt(MT2FW_MT2_FreeReceipt& output){
    output={};output.size=sizeof(output);output.state=state_;output.generation=generation_;output.slot=proof_.slot;output.current=live_confirmed_?proof_.current:0;
    output.original=proof_.free_export.resolved;output.replacement=replacement_;output.malloc_slot=proof_.malloc_slot;output.malloc_original=proof_.malloc_export.resolved;
    output.exchange_attempted=last_.attempted;output.exchanged=last_.exchanged;output.protection_restored=last_.protection_restored;output.initial_protection=initial_protection_;output.current_protection=live_confirmed_?proof_.protection:0;output.live_confirmed=live_confirmed_;output.win32_error=last_.win32;output.seh_code=last_.seh;
    output.imported_crt=proof_.imported_crt;output.free_provider=proof_.free_provider;output.malloc_provider=proof_.malloc_provider;output.free_export=proof_.free_export;output.malloc_export=proof_.malloc_export;
    text(output.owner,free_owner_?free_owner_->plugin->manifest.id:"");text(output.game_sha256,game_sha);text(output.cause,cause_);
}
MT2FW_MT2_Result NativeOwnership::failed(Owner& value,const std::string& cause,MT2FW_MT2_FreeReceipt* output){cause_=cause;report(value,cause);if(output)receipt(*output);return MT2FW_MT2_FAILED;}
bool NativeOwnership::authenticate(Snapshot& value,std::string& cause){
    live_confirmed_=false;if(!backend_.read(value,cause))return false;
    if(value.slot!=proof_.slot||value.malloc_slot!=proof_.malloc_slot){cause="Exact named free/malloc import slot identity changed";return false;}
    if(value.free_export.resolved!=proof_.free_export.resolved||value.malloc_export.resolved!=proof_.malloc_export.resolved){cause="Prepared CRT resolved export changed";return false;}
    if(value.imported_crt.base!=proof_.imported_crt.base||value.free_provider.base!=proof_.free_provider.base||value.malloc_provider.base!=proof_.malloc_provider.base){cause="Prepared imported CRT or resolved provider module changed";return false;}
    if(value.malloc_current!=proof_.malloc_export.resolved){cause="Malloc sibling import actual="+std::to_string(value.malloc_current)+" expected="+std::to_string(proof_.malloc_export.resolved);return false;}
    proof_=value;live_confirmed_=true;return true;
}
MT2FW_MT2_Result MT2FW_CALL NativeOwnership::free_prepare(void* context,MT2FW_MT2_FreeFn replacement,MT2FW_MT2_FreeReceipt* output){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;if(!replacement||!output_ok(output))return MT2FW_MT2_INVALID;auto& service=*value->service;std::lock_guard lock(service.gate_);
    if(service.free_owner_){service.receipt(*output);return MT2FW_MT2_BUSY;}
    Snapshot actual{};std::string cause;auto address=reinterpret_cast<uintptr_t>(replacement);
    if(!service.backend_.prepare(actual,cause)||!service.backend_.replacement(address,cause))return service.failed(*value,"Free import preparation failed: "+cause,output);
    if(actual.slot!=service.image_+0x1c52a00||actual.malloc_slot!=service.image_+0x1c52a70||!actual.free_export.resolved||!actual.malloc_export.resolved||actual.current!=actual.free_export.resolved||actual.malloc_current!=actual.malloc_export.resolved||!actual.imported_crt.pinned||!actual.free_provider.pinned||!actual.malloc_provider.pinned||actual.free_export.owner!=actual.free_provider.base||actual.malloc_export.owner!=actual.malloc_provider.base||!actual.protection||address==actual.free_export.resolved){service.proof_=actual;service.live_confirmed_=true;return service.failed(*value,"Named free/malloc slots, pinned CRT providers or original exports differ",output);}
    service.proof_=actual;service.initial_protection_=actual.protection;service.replacement_=address;service.free_owner_=value;service.state_=MT2FW_MT2_ROUTE_PREPARED;++service.generation_;service.last_={};service.live_confirmed_=true;service.retained_=false;service.cause_.clear();service.receipt(*output);return MT2FW_MT2_OK;
}
MT2FW_MT2_Result MT2FW_CALL NativeOwnership::free_enable(void* context,MT2FW_MT2_FreeReceipt* output){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;if(!output_ok(output))return MT2FW_MT2_INVALID;auto& service=*value->service;std::lock_guard lock(service.gate_);
    if(service.free_owner_!=value)return MT2FW_MT2_UNAVAILABLE;
    if(service.state_!=MT2FW_MT2_ROUTE_PREPARED&&service.state_!=MT2FW_MT2_ROUTE_DISABLED_RETAINED){service.receipt(*output);return MT2FW_MT2_INVALID;}
    Snapshot actual{};std::string cause;
    bool admitted=service.authenticate(actual,cause);
    if(admitted&&actual.current!=service.proof_.free_export.resolved){admitted=false;cause="Free import actual="+std::to_string(actual.current)+" expected="+std::to_string(service.proof_.free_export.resolved);}
    if(admitted&&actual.protection!=service.initial_protection_){admitted=false;cause="Free import protection actual="+std::to_string(actual.protection)+" expected="+std::to_string(service.initial_protection_);}
    if(!admitted){service.state_=MT2FW_MT2_ROUTE_UNKNOWN;return service.failed(*value,"Free import enable admission failed: "+cause,output);}
    auto previous=service.state_;service.state_=MT2FW_MT2_ROUTE_UNKNOWN;service.last_={};service.retained_=true;value->plugin->native_retained=true;
    service.last_=service.backend_.exchange(service.proof_.free_export.resolved,service.replacement_,service.initial_protection_,false);
    bool read=service.authenticate(actual,cause);
    if(service.last_.exchanged&&service.last_.protection_restored&&read&&actual.current==service.replacement_&&actual.protection==service.initial_protection_){service.state_=MT2FW_MT2_ROUTE_ENABLED;service.cause_.clear();service.receipt(*output);return MT2FW_MT2_OK;}
    if(!service.last_.attempted&&read&&actual.current==service.proof_.free_export.resolved&&actual.protection==service.initial_protection_)service.state_=previous;
    return service.failed(*value,"Free import enable was not confirmed: exchange="+std::to_string(service.last_.exchanged)+" protection="+std::to_string(service.last_.protection_restored)+" win32="+std::to_string(service.last_.win32)+" seh="+std::to_string(service.last_.seh)+(cause.empty()?"":"; "+cause),output);
}
MT2FW_MT2_Result NativeOwnership::disable_free(Owner& value,MT2FW_MT2_FreeReceipt* output){
    if(!output_ok(output))return MT2FW_MT2_INVALID;if(free_owner_!=&value)return MT2FW_MT2_UNAVAILABLE;
    Snapshot actual{};std::string cause;if(!authenticate(actual,cause)){state_=MT2FW_MT2_ROUTE_UNKNOWN;return failed(value,"Free import disable admission failed: "+cause,output);}
    if(actual.current!=proof_.free_export.resolved&&actual.current!=replacement_){state_=MT2FW_MT2_ROUTE_UNKNOWN;return failed(value,"Free import belongs to a foreign route; no restore attempted",output);}
    if(actual.current==proof_.free_export.resolved&&actual.protection==initial_protection_){state_=MT2FW_MT2_ROUTE_DISABLED_RETAINED;cause_.clear();receipt(*output);return MT2FW_MT2_OK;}
    bool repair=last_.attempted&&!last_.protection_restored;
    state_=MT2FW_MT2_ROUTE_UNKNOWN;last_={};retained_=true;value.plugin->native_retained=true;
    last_=backend_.exchange(actual.current,proof_.free_export.resolved,initial_protection_,repair);
    bool read=authenticate(actual,cause);
    if(last_.exchanged&&last_.protection_restored&&read&&actual.current==proof_.free_export.resolved&&actual.protection==initial_protection_){state_=MT2FW_MT2_ROUTE_DISABLED_RETAINED;cause_.clear();receipt(*output);return MT2FW_MT2_OK;}
    return failed(value,"Free import disable was not confirmed: exchange="+std::to_string(last_.exchanged)+" protection="+std::to_string(last_.protection_restored)+" win32="+std::to_string(last_.win32)+" seh="+std::to_string(last_.seh)+(cause.empty()?"":"; "+cause),output);
}
MT2FW_MT2_Result MT2FW_CALL NativeOwnership::free_disable(void* context,MT2FW_MT2_FreeReceipt* output){auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;auto& service=*value->service;std::lock_guard lock(service.gate_);return service.disable_free(*value,output);}
MT2FW_MT2_Result MT2FW_CALL NativeOwnership::free_read(void* context,MT2FW_MT2_FreeReceipt* output){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;if(!output_ok(output))return MT2FW_MT2_INVALID;auto& service=*value->service;std::lock_guard lock(service.gate_);if(service.free_owner_!=value)return MT2FW_MT2_UNAVAILABLE;
    if(service.state_==MT2FW_MT2_ROUTE_UNKNOWN){Snapshot actual{};std::string read_cause;if(!service.authenticate(actual,read_cause)){service.live_confirmed_=false;service.report(*value,"Free import unknown readback failed: "+read_cause);}service.receipt(*output);return MT2FW_MT2_FAILED;}
    Snapshot actual{};std::string cause;auto wanted=service.state_==MT2FW_MT2_ROUTE_ENABLED?service.replacement_:service.proof_.free_export.resolved;
    if(!service.authenticate(actual,cause)||actual.current!=wanted||actual.protection!=service.initial_protection_){service.state_=MT2FW_MT2_ROUTE_UNKNOWN;return service.failed(*value,"Free import live route/protection differs: "+cause,output);}
    service.receipt(*output);return service.state_==MT2FW_MT2_ROUTE_UNKNOWN?MT2FW_MT2_FAILED:MT2FW_MT2_OK;
}
MT2FW_MT2_Result MT2FW_CALL NativeOwnership::free_remove(void* context,MT2FW_MT2_FreeReceipt* output){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;if(!output_ok(output))return MT2FW_MT2_INVALID;auto& service=*value->service;std::lock_guard lock(service.gate_);if(service.free_owner_!=value)return MT2FW_MT2_UNAVAILABLE;
    if(service.state_!=MT2FW_MT2_ROUTE_PREPARED&&service.state_!=MT2FW_MT2_ROUTE_DISABLED_RETAINED){service.receipt(*output);return MT2FW_MT2_INVALID;}
    Snapshot actual{};std::string cause;if(!service.authenticate(actual,cause)||actual.current!=service.proof_.free_export.resolved||actual.protection!=service.initial_protection_){service.state_=MT2FW_MT2_ROUTE_UNKNOWN;return service.failed(*value,"Free import metadata retirement has no confirmed original route: "+cause,output);}
    service.state_=MT2FW_MT2_ROUTE_REMOVED;service.free_owner_=nullptr;service.cause_.clear();service.receipt(*output);return MT2FW_MT2_OK;
}
bool NativeOwnership::retire(Plugin& plugin){
    std::lock_guard lock(gate_);auto found=owners_.find(&plugin);if(found==owners_.end())return true;auto& value=*found->second;value.active=false;
    for(auto& command:commands_)if(command.owner==&value)command.active=false;
    for(auto& input:inputs_)if(input.owner==&value)input.active=false;
    if(value.inflight){report(value,"Mode command observer retirement waits for in-flight callback");return false;}
    std::erase_if(commands_,[&](auto& command){return command.owner==&value;});
    std::erase_if(inputs_,[&](auto& input){return input.owner==&value;});
    if(free_owner_!=&value)return true;MT2FW_MT2_FreeReceipt output{sizeof(output)};
    if(disable_free(value,&output)!=MT2FW_MT2_OK)return false;
    if(retained_){plugin.native_retained=true;return true;}free_owner_=nullptr;state_=MT2FW_MT2_ROUTE_REMOVED;return true;
}
bool NativeOwnership::has_retention()const{std::lock_guard lock(gate_);return free_owner_&&retained_;}
bool NativeOwnership::retirement_blocked()const{std::lock_guard lock(gate_);for(auto& [plugin,value]:owners_)if(value->inflight)return true;return free_owner_&&(!retained_||state_!=MT2FW_MT2_ROUTE_DISABLED_RETAINED);}
uint64_t MT2FW_CALL NativeOwnership::command_subscribe(void* context,MT2FW_MT2_CommandObserver callback,void* user){
    auto value=owner(context);if(!value||!callback)return 0;auto& service=*value->service;std::lock_guard lock(service.gate_);
    if(service.commands_.size()>=64){service.report(*value,"Mode command observer budget exceeded (64)");return 0;}
    auto token=service.next_observer_token_++;service.commands_.push_back({token,value,callback,user});return token;
}
MT2FW_MT2_Result MT2FW_CALL NativeOwnership::command_unsubscribe(void* context,uint64_t token){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;auto& service=*value->service;std::lock_guard lock(service.gate_);
    auto found=std::find_if(service.commands_.begin(),service.commands_.end(),[&](auto& command){return command.owner==value&&command.token==token;});
    if(found==service.commands_.end())return MT2FW_MT2_INVALID;found->active=false;if(value->inflight)return MT2FW_MT2_BUSY;service.commands_.erase(found);return MT2FW_MT2_OK;
}
void NativeOwnership::observe_command(const MT2FW_MT2_CommandCall& input){
    std::array<Command,64> snapshot{};size_t count=0;auto call=input;
    {std::lock_guard lock(gate_);call.invocation=++next_command_invocation_;for(auto& command:commands_)if(command.active&&command.owner->active){snapshot[count++]=command;++command.owner->inflight;}}
    for(size_t i=0;i<count;++i){auto& command=snapshot[i];
        try{command.callback(command.user,&call);}catch(const std::exception& problem){report(*command.owner,std::string("Mode command observer failed: ")+problem.what());}catch(...){report(*command.owner,"Mode command observer failed: unknown native exception");}
        std::lock_guard lock(gate_);--command.owner->inflight;
    }
}
uint64_t MT2FW_CALL NativeOwnership::input_subscribe(void* context,MT2FW_MT2_InputObserver callback,void* user){
    auto value=owner(context);if(!value||!callback)return 0;auto& service=*value->service;std::lock_guard lock(service.gate_);
    if(service.inputs_.size()>=64){service.report(*value,"Polled input observer budget exceeded (64)");return 0;}
    auto token=service.next_observer_token_++;service.inputs_.push_back({token,value,callback,user});return token;
}
MT2FW_MT2_Result MT2FW_CALL NativeOwnership::input_unsubscribe(void* context,uint64_t token){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;auto& service=*value->service;std::lock_guard lock(service.gate_);
    auto found=std::find_if(service.inputs_.begin(),service.inputs_.end(),[&](auto& input){return input.owner==value&&input.token==token;});
    if(found==service.inputs_.end())return MT2FW_MT2_INVALID;found->active=false;if(value->inflight)return MT2FW_MT2_BUSY;service.inputs_.erase(found);return MT2FW_MT2_OK;
}
void NativeOwnership::observe_input(const MT2FW_MT2_PolledEvent& input){
    if(!input.event||!input.event_bytes||input.event_bytes>MT2FW_MT2_EVENT_COPY_BYTES)return;
    std::array<Input,64> snapshot{};size_t count=0;auto call=input;
    {std::lock_guard lock(gate_);call.invocation=++next_input_invocation_;for(auto& observer:inputs_)if(observer.active&&observer.owner->active){snapshot[count++]=observer;++observer.owner->inflight;}}
    alignas(std::max_align_t) std::array<unsigned char,MT2FW_MT2_EVENT_COPY_BYTES> copied{};
    for(size_t i=0;i<count;++i){auto& observer=snapshot[i];std::memcpy(copied.data(),input.event,input.event_bytes);call.event=copied.data();
        try{observer.callback(observer.user,&call);}catch(const std::exception& problem){report(*observer.owner,std::string("Polled input observer failed: ")+problem.what());}catch(...){report(*observer.owner,"Polled input observer failed: unknown native exception");}
        std::lock_guard lock(gate_);--observer.owner->inflight;
    }
}
}
