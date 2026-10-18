#include "native_routes.h"
#include <algorithm>
#include <cstring>
#include <optional>

namespace mt2fw::mt2 {
namespace {
constexpr const char* game_sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
template<size_t N>void copy(char(&output)[N],const std::string& value){auto length=std::min(value.size(),N-1);std::memcpy(output,value.data(),length);output[length]=0;}
bool output_ok(MT2FW_MT2_RouteReceipt* out){return out&&out->size>=sizeof(*out);}
void patch_error(Runtime& runtime,const std::string& cause) noexcept {
    try{runtime.queue_patch_error("framework",cause);}
    catch(const std::exception& error){std::fprintf(stderr,"Native Create patch: %s; logging failed: %s\n",cause.c_str(),error.what());}
    catch(...){std::fprintf(stderr,"Native Create patch: %s; logging failed with unknown native exception\n",cause.c_str());}
}
}
NativeRoutes::NativeRoutes(std::function<Plugin*(const MT2FW_Host*)> lookup):lookup_(std::move(lookup)){create_pairs_.reserve(64);errors_.reserve(128);}
NativeRoutes::PatchLease::PatchLease(NativeRoutes& value):service(value){std::lock_guard lock(service.gate_);runtime=service.patch_runtime_;if(runtime)++service.patch_inflight_;}
NativeRoutes::PatchLease::~PatchLease(){if(runtime){std::lock_guard lock(service.gate_);--service.patch_inflight_;}}
void NativeRoutes::configure(uintptr_t image,Backend backend){image_=image;backend_=std::move(backend);}
void NativeRoutes::catalog(const std::string& name,uint64_t rva,bool shared){
    std::lock_guard lock(gate_);if(!rva||targets_.contains(name))throw std::runtime_error("Native route catalog target absent or duplicated: "+name);
    for(auto& [key,value]:targets_)if(value.rva==rva)throw std::runtime_error("Native route catalog RVA aliases: "+key);
    Target value{rva,shared};value.retained=!shared&&rva==0x1264b60;
    std::string cause;if(!backend_.pristine||!backend_.pristine(rva,0,value.pristine.data(),value.pristine.size(),cause))throw std::runtime_error("Native route pristine admission failed: "+name+": "+cause);
    value.current=value.pristine;targets_.emplace(name,std::move(value));
}
NativeRoutes::Owner* NativeRoutes::owner(void* context){auto result=static_cast<Owner*>(context);return result&&result->active?result:nullptr;}
void NativeRoutes::report(Owner& value,const std::string& cause){
    std::lock_guard lock(gate_);
    if(value.last_error==cause){++value.suppressed_errors;return;}
    value.last_error=cause;if(errors_.size()<128)errors_.push_back({value.plugin,cause});else ++value.suppressed_errors;
}
std::vector<std::pair<Plugin*,std::string>> NativeRoutes::drain_errors(){std::lock_guard lock(gate_);if(errors_.empty())return {};auto result=std::move(errors_);errors_.clear();errors_.reserve(128);return result;}
MT2FW_MT2_Result NativeRoutes::query(const MT2FW_Host* host,uint32_t version,MT2FW_MT2_RoutesV1* output,uint32_t capacity){
    if(!output||capacity<sizeof(*output)||version!=MT2FW_MT2_ROUTES_VERSION)return MT2FW_MT2_INVALID;
    auto plugin=lookup_(host);if(!plugin||!plugin->active||plugin->staging||plugin->manifest.type!="native")return MT2FW_MT2_UNAVAILABLE;
    std::lock_guard lock(gate_);auto& stored=owners_[plugin];if(!stored){stored=std::make_unique<Owner>();stored->service=this;stored->plugin=plugin;}
    if(!stored->active)return MT2FW_MT2_UNAVAILABLE;
    *output={sizeof(*output),MT2FW_MT2_ROUTES_VERSION,stored.get(),
        [](void* c,MT2FW_MT2_CreateBefore before,MT2FW_MT2_CreateAfter after,void* u)->uint64_t{return boundary(c,uint64_t(0),[&]{return create_subscribe(c,before,after,u);});},
        [](void* c,uint64_t token){return boundary(c,MT2FW_MT2_FAILED,[&]{return create_unsubscribe(c,token);});},
        [](void* c,MT2FW_MT2_RemoveAuthority callback,void* u,uint64_t* token){return boundary(c,MT2FW_MT2_FAILED,[&]{return remove_bind(c,callback,u,token);});},
        [](void* c,uint64_t token){return boundary(c,MT2FW_MT2_FAILED,[&]{return remove_unbind(c,token);});},
        [](void* c,const char* name,void* detour,MT2FW_MT2_RouteReceipt* out){return boundary(c,MT2FW_MT2_FAILED,[&]{return hook_prepare(c,name,detour,out);});},
        [](void* c,const char* name,MT2FW_MT2_RouteReceipt* out){return boundary(c,MT2FW_MT2_FAILED,[&]{return hook_enable(c,name,out);});},
        [](void* c,const char* name,MT2FW_MT2_RouteReceipt* out){return boundary(c,MT2FW_MT2_FAILED,[&]{return hook_remove(c,name,out);});},
        [](void* c,const char* name,MT2FW_MT2_RouteReceipt* out){return boundary(c,MT2FW_MT2_FAILED,[&]{return route_read(c,name,out);});},
        [](void* c,const char* name,uint32_t offset,const void* expected,uint32_t length,MT2FW_MT2_RouteReceipt* out){return boundary(c,MT2FW_MT2_FAILED,[&]{return code_verify(c,name,offset,expected,length,out);});}
    };return MT2FW_MT2_OK;
}
uint64_t MT2FW_CALL NativeRoutes::create_subscribe(void* context,MT2FW_MT2_CreateBefore before,MT2FW_MT2_CreateAfter after,void* user){
    auto value=owner(context);if(!value||!before||!after)return 0;auto& service=*value->service;std::lock_guard lock(service.gate_);
    if(service.create_pairs_.size()==64){service.report(*value,"Native CreateBuilding observer budget exceeded (64)");return 0;}
    auto token=service.next_token_++;service.create_pairs_.push_back({token,value,before,after,user});return token;
}
MT2FW_MT2_Result MT2FW_CALL NativeRoutes::create_unsubscribe(void* context,uint64_t token){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;auto& service=*value->service;std::lock_guard lock(service.gate_);
    auto found=std::find_if(service.create_pairs_.begin(),service.create_pairs_.end(),[&](auto& pair){return pair.owner==value&&pair.token==token;});
    if(found==service.create_pairs_.end())return MT2FW_MT2_INVALID;found->active=false;
    if(value->inflight)return MT2FW_MT2_BUSY;service.create_pairs_.erase(found);return MT2FW_MT2_OK;
}
void* NativeRoutes::create(const MT2FW_MT2_CreateCall& input,CreateOriginal original){
    PatchLease patch_lease(*this);auto patch_runtime=patch_lease.runtime;
    auto call=input;call.invocation=next_invocation_.fetch_add(1);
    std::array<CreatePair,64> snapshot{};size_t count=0;
    {std::lock_guard lock(gate_);for(auto& pair:create_pairs_)if(pair.active&&pair.owner->active){snapshot[count++]=pair;++pair.owner->inflight;}}
    std::array<bool,64> entered{};
    for(size_t i=0;i<count;++i){try{snapshot[i].before(snapshot[i].user,&call);entered[i]=true;}
        catch(const std::exception& problem){report(*snapshot[i].owner,std::string("CreateBuilding before failed: ")+problem.what());}
        catch(...){report(*snapshot[i].owner,"CreateBuilding before failed: unknown native exception");}}
    std::optional<PatchRegistry::Run> patch_run;
    if(patch_runtime){
        try{patch_run.emplace(patch_runtime->patches.begin("objects.create",{{"epoch",std::to_string(patch_runtime->patch_epoch())},{"thread",std::to_string(call.thread)},{"flag",call.flag!=0},{"lua_allowed",game_thread_.load()==call.thread}},nullptr,false));}
        catch(const std::exception& error){patch_error(*patch_runtime,std::string("Native Create patch begin failed: ")+error.what());}
        catch(...){patch_error(*patch_runtime,"Native Create patch begin failed: unknown native exception");}
    }
    void* result=nullptr;std::exception_ptr exception;
    try{result=original(input.manager,input.type,input.variant,input.pose,input.a,input.b,input.c,input.flag!=0);}catch(...){exception=std::current_exception();}
    if(patch_run){
        try{patch_runtime->patches.end(*patch_run,{{"original_returned",!exception},{"result_available",result!=nullptr}});}
        catch(const std::exception& error){patch_error(*patch_runtime,std::string("Native Create patch end failed: ")+error.what());}
        catch(...){patch_error(*patch_runtime,"Native Create patch end failed: unknown native exception");}
    }
    for(size_t i=count;i>0;--i){auto& pair=snapshot[i-1];if(entered[i-1]){
        try{pair.after(pair.user,&call,result,exception?0:1);}
        catch(const std::exception& problem){report(*pair.owner,std::string("CreateBuilding after failed: ")+problem.what());}
        catch(...){report(*pair.owner,"CreateBuilding after failed: unknown native exception");}}
        std::lock_guard lock(gate_);--pair.owner->inflight;
    }
    if(exception)std::rethrow_exception(exception);return result;
}
MT2FW_MT2_Result MT2FW_CALL NativeRoutes::remove_bind(void* context,MT2FW_MT2_RemoveAuthority callback,void* user,uint64_t* token){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;if(!callback||!token)return MT2FW_MT2_INVALID;
    auto& service=*value->service;std::lock_guard lock(service.gate_);if(service.authority_.owner)return MT2FW_MT2_BUSY;
    *token=service.next_token_++;service.authority_={*token,value,callback,user,true};return MT2FW_MT2_OK;
}
MT2FW_MT2_Result MT2FW_CALL NativeRoutes::remove_unbind(void* context,uint64_t token){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;auto& service=*value->service;std::lock_guard lock(service.gate_);
    auto& authority=service.authority_;if(authority.owner!=value||authority.token!=token)return MT2FW_MT2_INVALID;
    authority.active=false;if(value->inflight)return MT2FW_MT2_BUSY;authority={};return MT2FW_MT2_OK;
}
MT2FW_MT2_Result MT2FW_CALL NativeRoutes::next_remove(void* context){
    auto& value=*static_cast<Continuation*>(context);
    if(value.attempted){value.duplicate=true;return MT2FW_MT2_INVALID;}value.attempted=true;
    try{value.original(value.manager,value.array);value.returned=true;return MT2FW_MT2_OK;}
    catch(...){value.exception=std::current_exception();return MT2FW_MT2_FAILED;}
}
void NativeRoutes::remove(const MT2FW_MT2_RemoveCall& input,RemoveOriginal original){
    auto call=input;call.invocation=next_invocation_.fetch_add(1);Authority authority;
    {std::lock_guard lock(gate_);if(authority_.active&&authority_.owner->active&&game_thread_.load()==call.thread){authority=authority_;++authority.owner->inflight;}}
    if(!authority.owner){original(call.manager,call.array);return;}
    Continuation continuation{input.manager,input.array,original};auto disposition=MT2FW_MT2_REMOVE_UNKNOWN;
    try{disposition=authority.callback(authority.user,&call,next_remove,&continuation);}
    catch(const std::exception& problem){report(*authority.owner,std::string("DestroyProps authority failed: ")+problem.what());}
    catch(...){report(*authority.owner,"DestroyProps authority failed: unknown native exception");}
    bool suppressed=disposition==MT2FW_MT2_REMOVE_SUPPRESSED&&!continuation.attempted;
    bool forwarded=disposition==MT2FW_MT2_REMOVE_FORWARDED&&continuation.attempted&&continuation.returned&&!continuation.duplicate;
    if(!suppressed&&!forwarded)report(*authority.owner,"DestroyProps outcome unknown; original is not replayed");
    {std::lock_guard lock(gate_);--authority.owner->inflight;}
    if(continuation.exception)std::rethrow_exception(continuation.exception);
}
bool NativeRoutes::validate(Target& target,std::string& cause){
    Bytes actual{};if(!backend_.read(image_+target.rva,actual.data(),actual.size(),cause))return false;
    if(actual!=target.current){cause="Current native route differs from acknowledged adapter route";return false;}
    if(target.detour&&target.current!=target.pristine&&(!backend_.prove||!backend_.prove(reinterpret_cast<void*>(image_+target.rva),target.detour,cause)))return false;
    return true;
}
void NativeRoutes::receipt(const Target& target,MT2FW_MT2_RouteReceipt& output){
    output={sizeof(output),target.state,target.generation,target.rva,image_+target.rva,reinterpret_cast<uintptr_t>(target.trampoline),reinterpret_cast<uintptr_t>(target.detour)};
    std::memcpy(output.pristine,target.pristine.data(),32);std::memcpy(output.current,target.current.data(),32);
    copy(output.owner,target.owner?target.owner->plugin->manifest.id:target.shared?"framework":"");copy(output.game_sha256,game_sha);copy(output.cause,target.cause);
}
bool NativeRoutes::publish_shared(const std::string& name,void* detour,void* trampoline,std::string& cause){
    std::lock_guard lock(gate_);auto found=targets_.find(name);
    if(found==targets_.end()||!found->second.shared||!detour||!trampoline){cause="Shared native route publication requires catalog and actual trampoline";return false;}
    auto& target=found->second;Bytes current{};
    if(!backend_.prove||!backend_.prove(reinterpret_cast<void*>(image_+target.rva),detour,cause)||!backend_.read(image_+target.rva,current.data(),32,cause))return false;
    if(current==target.pristine){cause="Shared native route is not enabled";return false;}
    target.detour=detour;target.trampoline=trampoline;target.current=current;target.state=MT2FW_MT2_ROUTE_ENABLED;++target.generation;return true;
}
bool NativeRoutes::publish_framework_hook(void* target,void* detour,void* trampoline,std::string& cause){
    std::lock_guard lock(gate_);for(auto& [name,value]:targets_)if(image_+value.rva==reinterpret_cast<uintptr_t>(target)){
        if(!value.shared){cause="Framework hook collides with named Multiplayer target: "+name;return false;}
        return publish_shared(name,detour,trampoline,cause);
    }return true;
}
MT2FW_MT2_Result MT2FW_CALL NativeRoutes::hook_prepare(void* context,const char* name,void* detour,MT2FW_MT2_RouteReceipt* output){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;if(!name||!detour||!output_ok(output))return MT2FW_MT2_INVALID;
    auto& service=*value->service;std::lock_guard lock(service.gate_);auto found=service.targets_.find(name);
    if(found==service.targets_.end()||found->second.shared)return MT2FW_MT2_UNAVAILABLE;auto& target=found->second;
    if(target.owner){service.receipt(target,*output);return MT2FW_MT2_BUSY;}
    std::string cause;if(!service.validate(target,cause)){target.cause=cause;service.receipt(target,*output);return MT2FW_MT2_FAILED;}
    target.owner=value;target.detour=detour;target.state=MT2FW_MT2_ROUTE_UNKNOWN;++target.generation;
    if(target.retained)value->plugin->native_retained=true;
    bool okay=false,returned=false;try{okay=service.backend_.create(reinterpret_cast<void*>(service.image_+target.rva),detour,&target.trampoline,cause);returned=true;}catch(const std::exception& problem){cause=problem.what();}catch(...){cause="unknown native exception";}
    if(returned&&!okay){target.owner=nullptr;target.detour=nullptr;target.trampoline=nullptr;std::string live;target.state=service.validate(target,live)?MT2FW_MT2_ROUTE_PRISTINE:MT2FW_MT2_ROUTE_UNKNOWN;target.cause="Native prepare failed before ownership: "+cause+(live.empty()?"":"; "+live);service.report(*value,target.cause);service.receipt(target,*output);return MT2FW_MT2_FAILED;}
    target.created=okay;
    if(okay&&target.trampoline&&service.validate(target,cause)){target.state=MT2FW_MT2_ROUTE_PREPARED;target.cause.clear();service.receipt(target,*output);return MT2FW_MT2_OK;}
    target.cause="Native prepare outcome unknown: "+cause;service.report(*value,target.cause);service.receipt(target,*output);return MT2FW_MT2_FAILED;
}
MT2FW_MT2_Result MT2FW_CALL NativeRoutes::hook_enable(void* context,const char* name,MT2FW_MT2_RouteReceipt* output){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;if(!name||!output_ok(output))return MT2FW_MT2_INVALID;auto& service=*value->service;std::lock_guard lock(service.gate_);
    auto found=service.targets_.find(name);if(found==service.targets_.end()||found->second.shared||found->second.owner!=value)return MT2FW_MT2_UNAVAILABLE;auto& target=found->second;
    if(target.state!=MT2FW_MT2_ROUTE_PREPARED){service.receipt(target,*output);return MT2FW_MT2_INVALID;}
    std::string cause;bool okay=false;
    if(service.validate(target,cause)){try{okay=service.backend_.enable(reinterpret_cast<void*>(service.image_+target.rva),target.detour,cause);}catch(const std::exception& problem){cause=problem.what();}catch(...){cause="unknown native exception";}}
    Bytes actual{};
    if(okay&&service.backend_.read(service.image_+target.rva,actual.data(),32,cause)&&actual!=target.pristine){target.current=actual;target.state=MT2FW_MT2_ROUTE_ENABLED;target.cause.clear();service.receipt(target,*output);return MT2FW_MT2_OK;}
    target.state=MT2FW_MT2_ROUTE_UNKNOWN;target.cause="Native enable outcome unknown: "+cause;service.report(*value,target.cause);service.receipt(target,*output);return MT2FW_MT2_FAILED;
}
MT2FW_MT2_Result MT2FW_CALL NativeRoutes::hook_remove(void* context,const char* name,MT2FW_MT2_RouteReceipt* output){
    auto value=static_cast<Owner*>(context);if(!value)return MT2FW_MT2_UNAVAILABLE;if(!name||!output_ok(output))return MT2FW_MT2_INVALID;
    auto& service=*value->service;std::lock_guard lock(service.gate_);auto found=service.targets_.find(name);
    if(found==service.targets_.end()||found->second.shared||found->second.owner!=value)return MT2FW_MT2_UNAVAILABLE;auto& target=found->second;
    if(target.retained){target.cause="Native trampoline and owner are retained until process exit";service.receipt(target,*output);return MT2FW_MT2_BUSY;}
    std::string admission;
    if(!target.created||!service.validate(target,admission)){target.state=MT2FW_MT2_ROUTE_UNKNOWN;target.cause="Native removal has no authenticated current owned route: "+admission;service.receipt(target,*output);return MT2FW_MT2_FAILED;}
    std::string cause;bool okay=false;try{okay=service.backend_.remove(reinterpret_cast<void*>(service.image_+target.rva),cause);}catch(const std::exception& problem){cause=problem.what();}catch(...){cause="unknown native exception";}
    Bytes actual{};
    if(okay&&service.backend_.read(service.image_+target.rva,actual.data(),32,cause)&&actual==target.pristine){target.current=actual;target.state=MT2FW_MT2_ROUTE_REMOVED;target.cause.clear();target.owner=nullptr;target.detour=nullptr;target.trampoline=nullptr;target.created=false;service.receipt(target,*output);return MT2FW_MT2_OK;}
    target.state=MT2FW_MT2_ROUTE_UNKNOWN;target.cause="Native remove outcome unknown: "+cause;service.report(*value,target.cause);service.receipt(target,*output);return MT2FW_MT2_FAILED;
}
MT2FW_MT2_Result MT2FW_CALL NativeRoutes::route_read(void* context,const char* name,MT2FW_MT2_RouteReceipt* output){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;if(!name||!output_ok(output))return MT2FW_MT2_INVALID;
    auto& service=*value->service;std::lock_guard lock(service.gate_);auto found=service.targets_.find(name);if(found==service.targets_.end())return MT2FW_MT2_UNAVAILABLE;
    auto& target=found->second;std::string cause;if(target.state==MT2FW_MT2_ROUTE_UNKNOWN||!service.validate(target,cause)){target.cause=cause.empty()?"Native route outcome remains unknown":cause;service.receipt(target,*output);return MT2FW_MT2_FAILED;}
    service.receipt(target,*output);return MT2FW_MT2_OK;
}
MT2FW_MT2_Result MT2FW_CALL NativeRoutes::code_verify(void* context,const char* name,uint32_t offset,const void* expected,uint32_t length,MT2FW_MT2_RouteReceipt* output){
    if(!expected||!length||offset>=4096||length>4096-offset||!output_ok(output))return MT2FW_MT2_INVALID;
    auto result=route_read(context,name,output);if(result!=MT2FW_MT2_OK)return result;auto value=owner(context);auto& service=*value->service;std::lock_guard lock(service.gate_);auto& target=service.targets_.at(name);
    std::array<unsigned char,4096> pristine{},actual{};std::string cause;
    if(!service.backend_.pristine(target.rva,offset,pristine.data(),length,cause)||std::memcmp(pristine.data(),expected,length)||!service.backend_.read(service.image_+target.rva+offset,actual.data(),length,cause)){
        target.cause="Pristine native code guard differs: "+cause;service.receipt(target,*output);return MT2FW_MT2_FAILED;}
    for(uint32_t i=0;i<length;++i){auto position=offset+i;auto current=position<32?target.current[position]:pristine[i];if(actual[i]!=current){target.cause="Native body or owned route guard differs";service.receipt(target,*output);return MT2FW_MT2_FAILED;}}
    target.cause.clear();service.receipt(target,*output);return MT2FW_MT2_OK;
}
bool NativeRoutes::retire(Plugin& plugin){
    std::lock_guard lock(gate_);auto found=owners_.find(&plugin);if(found==owners_.end())return true;auto& value=*found->second;value.active=false;
    for(auto& pair:create_pairs_)if(pair.owner==&value)pair.active=false;
    if(authority_.owner==&value)authority_.active=false;
    if(value.inflight){report(value,"Native route retirement waits for in-flight invocation");return false;}
    std::erase_if(create_pairs_,[&](auto& pair){return pair.owner==&value;});if(authority_.owner==&value)authority_={};
    bool okay=true;for(auto& [name,target]:targets_)if(target.owner==&value){MT2FW_MT2_RouteReceipt output{sizeof(output)};
        auto result=target.retained?disable(plugin,name.c_str(),&output):hook_remove(&value,name.c_str(),&output);
        if(result!=MT2FW_MT2_OK)okay=false;
    }return okay;
}
bool NativeRoutes::busy()const{std::lock_guard lock(gate_);if(patch_inflight_)return true;for(auto& [plugin,owner]:owners_)if(owner->inflight)return true;for(auto& [name,target]:targets_)if(target.owner)return true;return false;}
bool NativeRoutes::has_retention()const{std::lock_guard lock(gate_);for(auto& [name,target]:targets_)if(target.owner&&target.retained)return true;return false;}
bool NativeRoutes::retirement_blocked()const{
    std::lock_guard lock(gate_);if(patch_inflight_)return true;for(auto& [plugin,owner]:owners_)if(owner->inflight)return true;
    for(auto& [name,target]:targets_)if(target.owner&&(!target.retained||target.state!=MT2FW_MT2_ROUTE_DISABLED_RETAINED))return true;return false;
}
MT2FW_MT2_Result NativeRoutes::disable(Plugin& plugin,const char* name,MT2FW_MT2_RouteReceipt* output){
    if(!name||!output_ok(output))return MT2FW_MT2_INVALID;std::lock_guard lock(gate_);
    auto owned=owners_.find(&plugin);auto found=targets_.find(name);
    if(owned==owners_.end()||found==targets_.end()||found->second.shared||found->second.owner!=owned->second.get())return MT2FW_MT2_UNAVAILABLE;
    auto& target=found->second;target.retained=true;plugin.native_retained=true;
    std::string cause;if(!target.created||!validate(target,cause)){target.state=MT2FW_MT2_ROUTE_UNKNOWN;target.cause="Native disable has no authenticated owned route: "+cause;receipt(target,*output);return MT2FW_MT2_FAILED;}
    if(target.current==target.pristine){target.state=MT2FW_MT2_ROUTE_DISABLED_RETAINED;target.cause.clear();receipt(target,*output);return MT2FW_MT2_OK;}
    bool okay=false;try{okay=backend_.disable&&backend_.disable(reinterpret_cast<void*>(image_+target.rva),cause);}catch(const std::exception& problem){cause=problem.what();}catch(...){cause="unknown native exception";}
    Bytes actual{};
    if(okay&&backend_.read(image_+target.rva,actual.data(),32,cause)&&actual==target.pristine){target.current=actual;target.state=MT2FW_MT2_ROUTE_DISABLED_RETAINED;target.cause.clear();receipt(target,*output);return MT2FW_MT2_OK;}
    target.state=MT2FW_MT2_ROUTE_UNKNOWN;target.cause="Native disable outcome unknown: "+cause;report(*owned->second,target.cause);receipt(target,*output);return MT2FW_MT2_FAILED;
}
MT2FW_MT2_Result NativeRoutes::resume(Plugin& plugin,const char* name,MT2FW_MT2_RouteReceipt* output){
    if(!name||!output_ok(output))return MT2FW_MT2_INVALID;std::lock_guard lock(gate_);auto owned=owners_.find(&plugin);auto found=targets_.find(name);
    if(owned==owners_.end()||!owned->second->active||!plugin.active||found==targets_.end()||found->second.shared||found->second.owner!=owned->second.get())return MT2FW_MT2_UNAVAILABLE;
    auto& target=found->second;if(target.state!=MT2FW_MT2_ROUTE_DISABLED_RETAINED||!target.created){receipt(target,*output);return MT2FW_MT2_INVALID;}
    std::string cause;if(!validate(target,cause)){target.state=MT2FW_MT2_ROUTE_UNKNOWN;target.cause=cause;receipt(target,*output);return MT2FW_MT2_FAILED;}
    target.state=MT2FW_MT2_ROUTE_PREPARED;return hook_enable(owned->second.get(),name,output);
}
}
