#include "native_routes.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include "../src/core/crypto.h"

using namespace mt2fw;
using namespace mt2fw::mt2;
namespace {
int checks=0,creates=0,removes=0;
bool throw_create=false,throw_remove=false;
MT2FW_MT2_CreateCall expected_create{};
void* expected_manager=nullptr;void* expected_array=nullptr;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
void* original_create(void* manager,const void* type,const void* variant,const void* pose,void* a,void* b,void* c,bool flag){
    ++creates;check(manager==expected_create.manager&&type==expected_create.type&&variant==expected_create.variant&&pose==expected_create.pose&&a==expected_create.a&&b==expected_create.b&&c==expected_create.c&&flag==(expected_create.flag!=0),"Create original arguments unchanged");
    if(throw_create)throw std::runtime_error("original-create");return manager;
}
void original_remove(void* manager,void* array){++removes;check(manager==expected_manager&&array==expected_array,"Remove original arguments unchanged");if(throw_remove)throw std::runtime_error("original-remove");}
struct Observer {int before=0,after=0;uint64_t invocation=0;uint32_t returned=0;void* result=nullptr;};
struct RetiringObserver {NativeRoutes* service;Plugin* plugin;bool retired=true;int before=0,after=0;};
void MT2FW_CALL retire_before(void* user,const MT2FW_MT2_CreateCall*){auto& value=*static_cast<RetiringObserver*>(user);++value.before;value.retired=value.service->retire(*value.plugin);}
void MT2FW_CALL retire_after(void* user,const MT2FW_MT2_CreateCall*,void*,uint32_t){++static_cast<RetiringObserver*>(user)->after;}
void MT2FW_CALL before(void* user,const MT2FW_MT2_CreateCall* call){auto& value=*static_cast<Observer*>(user);++value.before;value.invocation=call->invocation;}
void MT2FW_CALL after(void* user,const MT2FW_MT2_CreateCall* call,void* result,uint32_t returned){auto& value=*static_cast<Observer*>(user);++value.after;check(call->invocation==value.invocation,"Create pair invocation identity");value.result=result;value.returned=returned;}
struct Gate {int mode=0,calls=0;uintptr_t slot=0,address=0;MT2FW_MT2_Result first=MT2FW_MT2_FAILED,second=MT2FW_MT2_FAILED;};
MT2FW_MT2_RemoveDisposition MT2FW_CALL authority(void* user,const MT2FW_MT2_RemoveCall* call,MT2FW_MT2_RemoveNext next,void* invocation){
    auto& value=*static_cast<Gate*>(user);++value.calls;value.slot=call->native_return_slot;value.address=call->native_return_address;
    if(value.mode==0)return MT2FW_MT2_REMOVE_SUPPRESSED;
    if(value.mode==3)throw std::runtime_error("authority-error");
    if(value.mode==4)return MT2FW_MT2_REMOVE_UNKNOWN;
    value.first=next(invocation);if(value.mode==2)value.second=next(invocation);
    return value.first==MT2FW_MT2_OK?MT2FW_MT2_REMOVE_FORWARDED:MT2FW_MT2_REMOVE_UNKNOWN;
}
}
int main(){try{
    auto root=fs::current_path()/"artifacts/native-routes-tests";fs::create_directories(root/"logs");Runtime runtime(root);Plugin a(&runtime),b(&runtime);
    a.manifest.id="test.multiplayer";b.manifest.id="test.other";
    for(auto value:{&a,&b}){value->active=true;value->staging=false;value->manifest.type="native";}
    NativeRoutes service([&](const MT2FW_Host* host){return host==&a.host?&a:host==&b.host?&b:nullptr;});
    constexpr uintptr_t image=0x100000;std::array<unsigned char,8192> pristine{},live{};
    for(size_t i=0;i<pristine.size();++i)pristine[i]=static_cast<unsigned char>(i%128);live=pristine;
    int prepared=0,enabled=0,removed=0;bool fail_prepare=false,fail_enable=false,fail_remove=false;
    NativeRoutes::Backend backend;
    backend.pristine=[&](uint64_t rva,uint32_t offset,void* output,size_t count,std::string& cause){if(rva+offset+count>pristine.size()){cause="pristine bounds";return false;}std::memcpy(output,pristine.data()+rva+offset,count);return true;};
    backend.read=[&](uintptr_t address,void* output,size_t count,std::string& cause){if(address<image||address-image+count>live.size()){cause="live bounds";return false;}std::memcpy(output,live.data()+address-image,count);return true;};
    backend.create=[&](void* address,void*,void** original,std::string& cause){++prepared;if(fail_prepare){cause="already-created-by-foreign-owner";return false;}*original=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(address)+0x10000);return true;};
    backend.enable=[&](void* address,void*,std::string& cause){++enabled;if(fail_enable){cause="synthetic-enable-failure";return false;}live[reinterpret_cast<uintptr_t>(address)-image]=0xe9;return true;};
    backend.prove=[&](void* address,void*,std::string& cause){if(live[reinterpret_cast<uintptr_t>(address)-image]==0xe9)return true;cause="wrong-route";return false;};
    backend.remove=[&](void* address,std::string& cause){++removed;if(fail_remove){cause="synthetic-remove-failure";return false;}auto relative=reinterpret_cast<uintptr_t>(address)-image;std::memcpy(live.data()+relative,pristine.data()+relative,32);return true;};
    service.configure(image,backend);service.set_game_thread(101);service.catalog("unique",64);service.catalog("failure",128);service.catalog("objects.move",512,true);
    MT2FW_MT2_RoutesV1 api{},other{};check(service.query(&a.host,1,&api,sizeof(api))==MT2FW_MT2_OK,"query actual host");check(service.query(&b.host,1,&other,sizeof(other))==MT2FW_MT2_OK,"query second host");
    auto copied=a.host;check(service.query(&copied,1,&api,sizeof(api))==MT2FW_MT2_UNAVAILABLE,"copied host rejected");
    check(service.query(&a.host,2,&api,sizeof(api))==MT2FW_MT2_INVALID,"wrong service version rejected");
    Observer observer;auto subscription=api.create_subscribe(api.context,before,after,&observer);check(subscription!=0,"Create observer registered");
    int object=1,token=2;expected_create={sizeof(expected_create),1,0,101,&object,&token,&token,&token,&object,&object,&object};
    check(service.create(expected_create,original_create)==&object&&creates==1&&observer.before==1&&observer.after==1&&observer.returned&&observer.result==&object,"Create original once and actual result");
    throw_create=true;bool exception=false;try{service.create(expected_create,original_create);}catch(const std::runtime_error& problem){exception=std::string(problem.what())=="original-create";}throw_create=false;
    check(exception&&observer.before==2&&observer.after==2&&!observer.returned&&creates==2,"Create original exception retained and after marked incomplete");
    check(other.create_unsubscribe(other.context,subscription)==MT2FW_MT2_INVALID,"foreign observer token rejected");
    check(api.create_unsubscribe(api.context,subscription)==MT2FW_MT2_OK,"observer unsubscribe confirmed");
    expected_manager=&object;expected_array=&token;MT2FW_MT2_RemoveCall call{sizeof(call),0,0,101,&object,&token,0x123456,0xabcdef};Gate gate;uint64_t binding=0;
    check(api.remove_bind(api.context,authority,&gate,&binding)==MT2FW_MT2_OK,"Remove authority bound");
    uint64_t foreign=0;check(other.remove_bind(other.context,authority,&gate,&foreign)==MT2FW_MT2_BUSY,"single authority owner");
    service.remove(call,original_remove);check(removes==0&&gate.calls==1&&gate.slot==0x123456&&gate.address==0xabcdef,"owned intent suppression with original entry provenance");
    gate.mode=1;service.remove(call,original_remove);check(removes==1&&gate.first==MT2FW_MT2_OK,"authorized forward once");
    gate.mode=2;service.remove(call,original_remove);check(removes==2&&gate.second==MT2FW_MT2_INVALID,"duplicate continuation never repeats original");
    gate.mode=3;service.remove(call,original_remove);check(removes==2,"authority failure never implicitly forwards");
    gate.mode=4;service.remove(call,original_remove);check(removes==2,"unknown outcome never repeats original");
    gate.mode=1;throw_remove=true;exception=false;try{service.remove(call,original_remove);}catch(const std::runtime_error& problem){exception=std::string(problem.what())=="original-remove";}throw_remove=false;
    check(exception&&removes==3&&gate.first==MT2FW_MT2_FAILED,"original exception rethrown after authority acknowledgement");
    auto worker=call;worker.thread=102;auto authority_calls=gate.calls;service.remove(worker,original_remove);check(removes==4&&gate.calls==authority_calls,"worker path bypasses game-thread authority and forwards once");
    check(other.remove_unbind(other.context,binding)==MT2FW_MT2_INVALID,"foreign authority token rejected");
    check(api.remove_unbind(api.context,binding)==MT2FW_MT2_OK,"authority unbound");service.remove(call,original_remove);check(removes==5,"unowned route forwards once");
    MT2FW_MT2_RouteReceipt receipt{sizeof(receipt)};void* detour=&object;
    check(api.hook_prepare(api.context,"objects.move",detour,&receipt)==MT2FW_MT2_UNAVAILABLE,"shared route cannot become private detour");
    check(api.hook_prepare(api.context,"unknown",detour,&receipt)==MT2FW_MT2_UNAVAILABLE,"unknown target rejected");
    check(api.hook_enable(api.context,"unique",&receipt)==MT2FW_MT2_UNAVAILABLE,"enable before prepare rejected");
    check(api.hook_prepare(api.context,"unique",detour,&receipt)==MT2FW_MT2_OK&&receipt.state==MT2FW_MT2_ROUTE_PREPARED&&receipt.trampoline==image+64+0x10000&&enabled==0,"prepare publishes actual disabled trampoline");
    check(other.hook_prepare(other.context,"unique",detour,&receipt)==MT2FW_MT2_BUSY&&prepared==1,"target ownership unique");
    check(api.hook_enable(api.context,"unique",&receipt)==MT2FW_MT2_OK&&receipt.state==MT2FW_MT2_ROUTE_ENABLED&&enabled==1,"explicit enable after publication");
    check(api.hook_enable(api.context,"unique",&receipt)==MT2FW_MT2_INVALID&&enabled==1,"enabled hook not repeated");
    check(api.code_verify(api.context,"unique",0,pristine.data()+64,16,&receipt)==MT2FW_MT2_OK,"pristine guard and owned active route accepted together");
    service.set_game_thread(GetCurrentThreadId());
    std::string body_cause;auto body_sha=digest(std::string(reinterpret_cast<const char*>(pristine.data()+64),64));
    check(service.verify_body(64,64,body_sha,body_cause),"Gameplay full-body proof accepts acknowledged active hook");
    check(!service.verify_body(64,64,std::string(64,'0'),body_cause)&&body_cause.find("SHA256")!=std::string::npos,"Gameplay rejects incorrect pristine body hash");
    check(service.verify_body(64,64,"",body_cause),"Gameplay helper entry uses exact-build pristine proof");
    live[110]^=1;
    check(!service.verify_body(64,64,body_sha,body_cause)&&body_cause.find("live body")!=std::string::npos,"Gameplay detects changed bytes beyond the hook entry");live[110]=pristine[110];
    check(!service.verify_body(64,65537,body_sha,body_cause),"Gameplay body reads stay bounded");
    service.set_game_thread(101);
    if(GetCurrentThreadId()!=101)check(!service.verify_body(64,64,body_sha,body_cause)&&body_cause.find("game thread")!=std::string::npos,"Gameplay code verification rejects another thread");
    check(api.code_verify(api.context,"unique",36,pristine.data()+100,8,&receipt)==MT2FW_MT2_OK,"unpatched body guard accepted");
    live[100]^=1;check(api.code_verify(api.context,"unique",36,pristine.data()+100,8,&receipt)==MT2FW_MT2_FAILED,"foreign body modification rejected");live[100]=pristine[100];
    unsigned char wrong[2]={0,0};check(api.code_verify(api.context,"unique",0,wrong,2,&receipt)==MT2FW_MT2_FAILED,"incorrect pristine expectation rejected");
    live[64]=0xcc;check(api.route_read(api.context,"unique",&receipt)==MT2FW_MT2_FAILED,"unowned replacement route rejected");auto remove_count=removed;
    check(api.hook_remove(api.context,"unique",&receipt)==MT2FW_MT2_FAILED&&removed==remove_count,"foreign route is not overwritten during cleanup");live[64]=0xe9;
    check(other.hook_remove(other.context,"unique",&receipt)==MT2FW_MT2_UNAVAILABLE,"foreign plugin cannot remove owned target");
    check(api.hook_remove(api.context,"unique",&receipt)==MT2FW_MT2_OK&&receipt.state==MT2FW_MT2_ROUTE_REMOVED&&live[64]==pristine[64],"owned cleanup restores pristine route");
    fail_prepare=true;remove_count=removed;check(api.hook_prepare(api.context,"failure",detour,&receipt)==MT2FW_MT2_FAILED&&receipt.state==MT2FW_MT2_ROUTE_PRISTINE,"known failed create never acquires a foreign MinHook slot");
    check(api.hook_remove(api.context,"failure",&receipt)==MT2FW_MT2_UNAVAILABLE&&removed==remove_count,"failed create cannot remove somebody else's hook");fail_prepare=false;
    check(api.hook_prepare(api.context,"failure",detour,&receipt)==MT2FW_MT2_OK,"second target prepare");fail_enable=true;
    check(api.hook_enable(api.context,"failure",&receipt)==MT2FW_MT2_FAILED&&receipt.state==MT2FW_MT2_ROUTE_UNKNOWN,"failed enable remains unknown");auto enable_count=enabled;
    check(api.hook_enable(api.context,"failure",&receipt)==MT2FW_MT2_INVALID&&enabled==enable_count,"unknown enable never auto-replayed");fail_enable=false;
    check(api.hook_remove(api.context,"failure",&receipt)==MT2FW_MT2_OK,"disabled owned target can retire after failed enable");
    live[512]=0xe9;std::string cause;check(service.publish_shared("objects.move",detour,reinterpret_cast<void*>(0x900000),cause),"shared route published with actual owner proof");
    check(api.route_read(api.context,"objects.move",&receipt)==MT2FW_MT2_OK&&std::string(receipt.owner)=="framework"&&receipt.entry==image+512,"shared route receipt identifies Framework owner");
    check(api.code_verify(api.context,"objects.move",0,pristine.data()+512,12,&receipt)==MT2FW_MT2_OK,"Move admits original body plus current Framework observer");
    check(!service.drain_errors().empty(),"original causes retained for game-thread diagnostics");
    check(service.retire(a)&&!service.busy(),"owned routes and observers retire");
    check(api.create_subscribe(api.context,before,after,&observer)==0,"retired owner cannot register");
    check(other.route_read(other.context,"objects.move",&receipt)==MT2FW_MT2_OK,"other mod remains active");
    Plugin c(&runtime);c.manifest.id="test.retiring";c.manifest.type="native";c.active=true;c.staging=false;
    NativeRoutes retiring([&](const MT2FW_Host* host){return host==&c.host?&c:nullptr;});MT2FW_MT2_RoutesV1 retiring_api{};
    check(retiring.query(&c.host,1,&retiring_api,sizeof(retiring_api))==MT2FW_MT2_OK,"retirement observer owner");
    RetiringObserver retiring_observer{&retiring,&c};check(retiring_api.create_subscribe(retiring_api.context,retire_before,retire_after,&retiring_observer)!=0,"retirement observer registered");
    retiring.create(expected_create,original_create);
    check(!retiring_observer.retired&&retiring_observer.before==1&&retiring_observer.after==1,"retirement keeps entered observer through original and after");
    check(retiring.retire(c)&&!retiring.busy(),"retirement confirms only after shared invocation closes");
    retiring.create(expected_create,original_create);check(retiring_observer.before==1&&retiring_observer.after==1,"retired observer is absent from future calls");
    std::cout<<checks<<" native route checks passed\n";return 0;
}catch(const std::exception& problem){std::cerr<<"Native route check failed after "<<checks<<": "<<problem.what()<<"\n";return 1;}}
