#include "native_patches.h"
#include "native_routes.h"
#include <iostream>
#include <stdexcept>
#include <type_traits>

using namespace mt2fw;
using namespace mt2fw::mt2;
namespace {
int checks=0,originals=0;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
Plugin& plugin(Runtime& runtime,const char* id){
    // The fixture supplies owned synthetic plugins without loading any DLL.
    auto value=std::make_unique<Plugin>(&runtime);value->manifest.id=id;value->manifest.type="native";value->manifest.multiplayer="shared";value->manifest.capabilities={"world.read","time.write"};
    value->active=true;value->staging=false;value->patch_staging=false;value->native_loaded=true;
    auto& result=*value;auto& storage=const_cast<std::vector<std::unique_ptr<Plugin>>&>(runtime.plugins());storage.push_back(std::move(value));return result;
}
struct Observer {
    std::vector<std::string>* order=nullptr;
    std::vector<MT2FW_MT2_PatchEvent> before,after;
    bool replace=false,fail=false;
};
MT2FW_MT2_Result MT2FW_CALL before(void* user,const MT2FW_MT2_PatchEvent* call,MT2FW_MT2_PatchEdit* edit){
    auto& observer=*static_cast<Observer*>(user);observer.before.push_back(*call);if(observer.order)observer.order->push_back("native.before");
    if(observer.replace){edit->replace_arguments=1;edit->paused=0;edit->speed=2.5f;}
    return observer.fail?MT2FW_MT2_FAILED:MT2FW_MT2_OK;
}
MT2FW_MT2_Result MT2FW_CALL after(void* user,const MT2FW_MT2_PatchEvent* call){auto& observer=*static_cast<Observer*>(user);observer.after.push_back(*call);if(observer.order)observer.order->push_back("native.after");return MT2FW_MT2_OK;}
struct Provider {
    uint32_t role=MT2FW_MT2_SESSION_UNKNOWN;
    bool stale=false,fail=false,throws=false,retire=false,reenter=false,nested_allowed=true;
    int calls=0;NativePatches* service=nullptr;Plugin* owner=nullptr;bool retired=true;
};
MT2FW_MT2_Result MT2FW_CALL provide(void* user,const MT2FW_MT2_SessionQuery* query,MT2FW_MT2_SessionReply* reply){
    auto& value=*static_cast<Provider*>(user);++value.calls;
    if(value.throws)throw std::runtime_error("synthetic-provider-failure");
    if(value.reenter){std::string reason;value.nested_allowed=value.service->authorize("api.time.set",reason);}
    if(value.retire)value.retired=value.service->retire(*value.owner);
    reply->role=value.role;reply->epoch=query->epoch+(value.stale?1:0);return value.fail?MT2FW_MT2_FAILED:MT2FW_MT2_OK;
}
NativeRoutes* nested_routes=nullptr;MT2FW_MT2_CreateCall nested_call{};bool nest=false,throw_original=false,detach_original=false,detached=true,retirement_blocked=false;
void* original(void* manager,const void*,const void*,const void*,void*,void*,void*,bool flag){
    ++originals;check(flag,"Create original flag unchanged");
    if(nest){nest=false;nested_routes->create(nested_call,original);}
    if(detach_original){detach_original=false;detached=nested_routes->detach_patch_runtime();retirement_blocked=nested_routes->retirement_blocked();}
    if(throw_original)throw std::runtime_error("synthetic-original-failure");return manager;
}
MT2FW_MT2_PatchSpec spec(const char* id,uint32_t target,Observer& observer,uint32_t mode=MT2FW_MT2_PATCH_OBSERVE){
    MT2FW_MT2_PatchSpec result{};result.size=sizeof(result);result.id=id;result.target=target;result.mode=mode;result.on_before=before;result.on_after=after;result.user=&observer;return result;
}
}
int main(){try{
    static_assert(std::is_standard_layout_v<MT2FW_MT2_PatchEvent>);
    static_assert(std::is_trivially_copyable_v<MT2FW_MT2_PatchEvent>);
    static_assert(std::is_standard_layout_v<MT2FW_MT2_PatchesV1>);
    auto root=fs::current_path()/"artifacts/native-patches-tests";fs::create_directories(root/"logs");
    int requests=0;Json effective;Runtime runtime(root,[&](const std::string& operation,const Json& data){check(operation=="time.set","only request time setter is used");++requests;effective=data;return Json{{"paused",data.at("paused")},{"speed",data.at("speed")}};});
    runtime.epoch=6;runtime.world_ready("fixture-world");uint64_t thread=101;bool world=true;bool proof_throws=false;
    NativePatches service(runtime,[&]{return thread;},[&](uint64_t& epoch,std::string& cause){if(proof_throws)throw std::runtime_error("synthetic-world-proof-failure");epoch=runtime.epoch;cause=world?"synthetic native world acknowledged":"synthetic world absent";return world;});
    runtime.set_patch_authority([&](const std::string& operation,std::string& reason){return service.authorize(operation,reason);});service.set_game_thread(thread);
    auto& owner=plugin(runtime,"test.native");MT2FW_MT2_PatchesV1 api{};
    check(service.query(&owner.host,1,&api,sizeof(api))==MT2FW_MT2_OK,"exact active native host queries extension");
    auto copy=owner.host;MT2FW_MT2_PatchesV1 invalid{};check(service.query(&copy,1,&invalid,sizeof(invalid))==MT2FW_MT2_UNAVAILABLE,"copied host rejected");
    check(service.query(&owner.host,2,&invalid,sizeof(invalid))==MT2FW_MT2_INVALID,"unknown version rejected");
    check(service.query(&owner.host,1,&invalid,sizeof(invalid)-1)==MT2FW_MT2_INVALID,"short output rejected");
    thread=102;check(service.query(&owner.host,1,&invalid,sizeof(invalid))==MT2FW_MT2_UNAVAILABLE,"worker discovery rejected");thread=101;
    MT2FW_MT2_PatchReceipt receipt{sizeof(receipt)};Observer observer;auto native_spec=spec("native-create",MT2FW_MT2_PATCH_OBJECTS_CREATE,observer);
    thread=102;
    check(api.register_patch(api.context,&native_spec,&receipt)==MT2FW_MT2_FAILED&&std::string(receipt.cause).find("outside the bound game thread")!=std::string::npos,"worker registration rejects before accessing mutable plugin state");
    thread=101;
    check(api.register_patch(nullptr,&native_spec,&receipt)==MT2FW_MT2_FAILED&&receipt.cause[0],"null context returns visible receipt error");
    check(api.register_patch(api.context,&native_spec,&receipt)==MT2FW_MT2_OK,"typed native Create observer registered");
    check(api.register_patch(api.context,&native_spec,&receipt)==MT2FW_MT2_FAILED&&receipt.cause[0],"duplicate id fails visibly");
    auto bad=spec("bad-create-writer",MT2FW_MT2_PATCH_OBJECTS_CREATE,observer,MT2FW_MT2_PATCH_MODIFY);
    check(api.register_patch(api.context,&bad,&receipt)==MT2FW_MT2_FAILED,"Create argument mutation rejected");
    auto& no_capability=plugin(runtime,"test.no-capability");no_capability.manifest.capabilities.clear();MT2FW_MT2_PatchesV1 denied{};
    check(service.query(&no_capability.host,1,&denied,sizeof(denied))==MT2FW_MT2_OK,"authenticated plugin queries before capability admission");
    check(denied.register_patch(denied.context,&native_spec,&receipt)==MT2FW_MT2_FAILED,"native registration uses Runtime capability guard");
    std::vector<std::string> order;observer.order=&order;
    PatchSpec shared;shared.id="shared-core";shared.target="objects.create";shared.before={"test.native:native-create"};
    runtime.add_patch(owner,shared,{[&](const PatchCall&)->Json{order.push_back("core.before");return nullptr;},[&](const PatchCall&){order.push_back("core.after");}});
    NativeRoutes routes([&](const MT2FW_Host* host){return host==&owner.host?&owner:nullptr;});routes.set_game_thread(thread);routes.set_patch_runtime(&runtime);nested_routes=&routes;
    int object=1;MT2FW_MT2_CreateCall call{sizeof(call),1,0,thread,&object};nested_call=call;
    check(routes.create(call,original)==&object&&originals==1,"shared Create preserves actual original result and one call");
    check(order==std::vector<std::string>{"core.before","native.before","native.after","core.after"},"native and core subscriptions share explicit ordering");
    check(observer.before.size()==1&&observer.after.size()==1&&observer.before[0].invocation&&observer.before[0].generation,"native payload has registry identity");
    check(observer.before[0].epoch==7&&observer.before[0].thread==thread&&observer.before[0].create_flag==1,"native payload copies scalar context");
    check(observer.after[0].original_returned==1&&observer.after[0].result_present==1&&observer.after[0].invocation==observer.before[0].invocation,"native after has exact return availability and paired identity");
    throw_original=true;bool thrown=false;try{routes.create(call,original);}catch(const std::runtime_error& error){thrown=std::string(error.what())=="synthetic-original-failure";}throw_original=false;
    check(thrown&&originals==2&&observer.after.back().original_returned==0&&!observer.after.back().result_present,"original exception survives native patch envelope without replay");
    auto count=observer.before.size();nest=true;routes.create(call,original);
    check(originals==4&&observer.before.size()==count+2&&observer.after.size()==count+2,"nested calls keep both native pairs");
    check(observer.before[count].invocation!=observer.before[count+1].invocation&&observer.after[count].invocation==observer.before[count+1].invocation&&observer.after[count+1].invocation==observer.before[count].invocation,"nested invocation identities close in stack order");
    auto worker=call;worker.thread=102;routes.create(worker,original);check(observer.after.back().thread==102&&originals==5,"worker native observer preserves real thread and original call");
    check(api.unregister_patch(api.context,"native-create",&receipt)==MT2FW_MT2_OK,"owner unregisters native observer");
    auto prior=observer.before.size();routes.create(call,original);check(observer.before.size()==prior,"unregistered observer is absent from later native calls");
    check(runtime.remove_patch(owner,"shared-core"),"shared observer removed before zero-subscriber lifetime check");
    detach_original=true;routes.create(call,original);
    check(!detached&&retirement_blocked,"Create retains Runtime through its original even with zero patch subscribers");
    check(!routes.retirement_blocked()&&routes.detach_patch_runtime(),"Runtime detachment confirms only after Create invocation closes");
    routes.create(call,original);check(observer.before.size()==prior,"late shared route entry after detachment does not use Runtime");
    std::string reason;check(service.authorize("api.time.set",reason)&&reason.find("Standalone")!=std::string::npos,"positive world proof and absent bridge acknowledge standalone");
    world=false;check(!service.authorize("api.time.set",reason)&&reason.find("synthetic world absent")!=std::string::npos,"missing native world denies mutation");world=true;
    proof_throws=true;check(!service.authorize("api.time.set",reason)&&reason.find("synthetic-world-proof-failure")!=std::string::npos,"world proof exception becomes visible unknown authority");proof_throws=false;
    thread=102;check(!service.authorize("api.time.set",reason),"worker mutation denied");thread=101;
    Observer writer;writer.replace=true;auto write_spec=spec("time-writer",MT2FW_MT2_PATCH_API_TIME_SET,writer,MT2FW_MT2_PATCH_MODIFY);
    check(api.register_patch(api.context,&write_spec,&receipt)==MT2FW_MT2_OK,"typed time request writer registered");
    runtime.request(owner,"time.set",{{"paused",true},{"speed",1.f}});
    check(requests==1&&!effective.at("paused").get<bool>()&&effective.at("speed").get<float>()==2.5f,"native writer changes Framework time request arguments once");
    auto& bridge=plugin(runtime,"doodesch.multiplayer");
    check(!service.authorize("api.time.set",reason)&&reason.find("without an acknowledged")!=std::string::npos,"active old Multiplayer bridge without provider denies mutation");
    bool denied_request=false;try{runtime.request(owner,"time.set",{{"paused",true},{"speed",1.f}});}catch(const std::runtime_error&){denied_request=true;}
    check(denied_request&&requests==1,"old Multiplayer bridge denial prevents the request original");
    MT2FW_MT2_PatchesV1 bridge_api{};check(service.query(&bridge.host,1,&bridge_api,sizeof(bridge_api))==MT2FW_MT2_OK,"trusted bridge queries own native extension");
    Provider provider;provider.service=&service;provider.owner=&bridge;uint64_t token=0;
    check(api.bind_session_provider(api.context,provide,&provider,&token,&receipt)==MT2FW_MT2_FAILED&&receipt.cause[0],"ordinary native plugin cannot claim session authority");
    check(bridge_api.bind_session_provider(bridge_api.context,provide,&provider,&token,&receipt)==MT2FW_MT2_OK&&token,"trusted bridge owns one provider");
    uint64_t second=0;check(bridge_api.bind_session_provider(bridge_api.context,provide,&provider,&second,&receipt)==MT2FW_MT2_BUSY&&second==0,"provider cannot be silently replaced");
    check(!service.authorize("api.time.set",reason)&&reason.find("unknown")!=std::string::npos,"unknown provider role denies mutation");
    provider.role=MT2FW_MT2_SESSION_CLIENT;check(!service.authorize("api.time.set",reason)&&reason.find("Client")!=std::string::npos,"client role denies mutation");
    provider.role=MT2FW_MT2_SESSION_HOST;check(!service.authorize("api.time.set",reason)&&reason.find("Host-action bridge is unavailable")!=std::string::npos,"trusted Host role does not imply an available shared action bridge");
    check(service.status().at("role")=="host","denied mutation retains the observed Host role");
    check(!runtime.multiplayer,"production-default multiplayer flag remains false for Host denial check");
    denied_request=false;try{runtime.request(owner,"time.set",{{"paused",false},{"speed",1.f}});}catch(const std::runtime_error& error){denied_request=std::string(error.what()).find("Host-action bridge is unavailable")!=std::string::npos;}
    check(denied_request&&requests==1,"Host with the default false flag cannot execute the adapter without a shared action bridge");
    provider.reenter=true;check(!service.authorize("api.time.set",reason)&&!provider.nested_allowed,"recursive provider queries deny inner authority without repeating the provider");provider.reenter=false;
    runtime.multiplayer=true;denied_request=false;try{runtime.request(owner,"time.set",{{"paused",false},{"speed",1.f}});}catch(const std::runtime_error&){denied_request=true;}
    check(denied_request&&requests==1,"Host patch authority does not bypass the existing general multiplayer write fence");runtime.multiplayer=false;
    provider.stale=true;check(!service.authorize("api.time.set",reason)&&reason.find("stale")!=std::string::npos,"stale epoch acknowledgement denies mutation");provider.stale=false;
    provider.fail=true;check(!service.authorize("api.time.set",reason)&&reason.find("error=")!=std::string::npos,"provider failed return denies mutation");provider.fail=false;
    provider.throws=true;check(!service.authorize("api.time.set",reason)&&reason.find("synthetic-provider-failure")!=std::string::npos,"provider exception denies mutation with original cause");provider.throws=false;
    provider.role=MT2FW_MT2_SESSION_STANDALONE;check(service.authorize("api.time.set",reason)&&reason.find("disconnected")!=std::string::npos,"trusted provider can acknowledge disconnected standalone");
    runtime.request(owner,"time.set",{{"paused",true},{"speed",1.f}});check(requests==2,"acknowledged disconnected standalone can execute the request adapter");
    check(bridge_api.unbind_session_provider(bridge_api.context,token+1,&receipt)==MT2FW_MT2_FAILED,"foreign provider token rejected");
    check(bridge_api.unbind_session_provider(bridge_api.context,token,&receipt)==MT2FW_MT2_OK,"provider retirement acknowledged");
    check(!service.authorize("api.time.set",reason),"active bridge never falls back to standalone after unbind");
    check(bridge_api.bind_session_provider(bridge_api.context,provide,&provider,&token,&receipt)==MT2FW_MT2_OK,"provider can bind again after acknowledged retirement");
    provider.retire=true;provider.role=MT2FW_MT2_SESSION_HOST;
    check(!service.authorize("api.time.set",reason)&&!provider.retired,"provider self-retirement retains callback lease and invalidates acknowledgement");
    check(service.retire(bridge),"provider owner retires only after callback returns");bridge.active=false;
    check(!service.authorize("api.time.set",reason)&&reason.find("inactive or retained")!=std::string::npos,"inactive loaded bridge does not imply disconnection");
    bridge.native_loaded=false;bridge.native_pins=1;check(!service.authorize("api.time.set",reason),"pinned supported bridge retains unknown session authority");bridge.native_pins=0;
    bridge.native_retained=true;check(!service.authorize("api.time.set",reason),"retained supported bridge retains unknown session authority");bridge.native_retained=false;
    bridge.native_retirement_failed=true;check(!service.authorize("api.time.set",reason),"failed bridge retirement does not imply disconnection");bridge.native_retirement_failed=false;
    bridge.native_shutdown_pending=true;check(!service.authorize("api.time.set",reason),"pending bridge shutdown does not imply disconnection");bridge.native_shutdown_pending=false;
    bridge.native_module=reinterpret_cast<void*>(uintptr_t(1));check(!service.authorize("api.time.set",reason),"loaded bridge module does not imply disconnection");bridge.native_module=nullptr;
    check(service.authorize("api.time.set",reason)&&reason.find("fully unloaded")!=std::string::npos,"fully unloaded inactive bridge uses fresh native standalone proof");
    check(service.retire(owner)&&!service.busy(),"native observer and writer owner retire after invocations close");
    check(api.register_patch(api.context,&native_spec,&receipt)==MT2FW_MT2_FAILED,"retired native service context rejects new registration");
    runtime.set_patch_authority({});
    std::cout<<"Native patch checks passed: "<<checks<<"\n";return 0;
}catch(const std::exception& error){std::cerr<<"Native patch check failed after "<<checks<<": "<<error.what()<<"\n";return 1;}}
