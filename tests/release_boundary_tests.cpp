#include "native_ownership.h"
#include <cstring>
#include <iostream>

using namespace mt2fw;
using namespace mt2fw::mt2;
namespace {
int checks=0;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
struct Delegate {
    NativeOwnership* service=nullptr;
    Plugin* plugin=nullptr;
    MT2FW_MT2_ReleaseV1* api=nullptr;
    int calls=0,mode=0;
    MT2FW_MT2_ReleaseCall call{};
    std::string text;
    uint32_t inside=MT2FW_MT2_OK;
    bool retired=true,blocked=false;
};
MT2FW_MT2_Result MT2FW_CALL decide(void* user,const MT2FW_MT2_ReleaseCall* call,MT2FW_MT2_ReleaseDecision* output){
    auto& d=*static_cast<Delegate*>(user);++d.calls;d.call=*call;d.text.assign(call->text,call->length);
    MT2FW_MT2_ReleaseReceipt receipt{sizeof(receipt)};
    check(d.api->read(d.api->context,&receipt)==MT2FW_MT2_OK&&receipt.inflight==1,"release callback lease is visible");
    output->disposition=MT2FW_MT2_RELEASE_FORWARD;
    if(d.mode==1)output->disposition=MT2FW_MT2_RELEASE_SUPPRESS;
    if(d.mode==2){output->disposition=MT2FW_MT2_RELEASE_FAILED;std::strcpy(output->cause,"original celebration owner changed");}
    if(d.mode==3)throw std::runtime_error("original release proof failed");
    if(d.mode==4)output->disposition=99;
    if(d.mode==5)output->size=1;
    if(d.mode==6)std::memset(output->cause,'x',sizeof(output->cause));
    if(d.mode==7){std::strcpy(output->cause,"consumer retained an uncertain original result");return MT2FW_MT2_FAILED;}
    if(d.mode==8){d.inside=d.api->unbind(d.api->context,call->registration,&receipt);output->disposition=MT2FW_MT2_RELEASE_SUPPRESS;}
    if(d.mode==9){d.inside=d.service->release_command(*call);}
    if(d.mode==10){d.retired=d.service->retire(*d.plugin);d.blocked=d.service->retirement_blocked();output->disposition=MT2FW_MT2_RELEASE_SUPPRESS;}
    if(d.mode==11)throw 7;
    return MT2FW_MT2_OK;
}
void MT2FW_CALL passive(void* user,const MT2FW_MT2_CommandCall* call){
    check(call->readable&&call->length==7&&!std::memcmp(call->text,"LevelUp",7),"passive V1 receives its original borrowed text");
    ++*static_cast<int*>(user);
}
}
int main(){try{
    auto root=fs::current_path()/"artifacts/release-boundary-tests";fs::create_directories(root/"logs");Runtime runtime(root);
    Plugin a(&runtime),b(&runtime),other(&runtime);
    for(auto plugin:{&a,&b,&other}){plugin->active=true;plugin->staging=false;plugin->manifest.type="native";}
    a.manifest.id=b.manifest.id="doodesch.multiplayer";other.manifest.id="test.other";
    auto lookup=[&](const MT2FW_Host* host){return host==&a.host?&a:host==&b.host?&b:host==&other.host?&other:nullptr;};
    constexpr uintptr_t image=0x100000,caller=image+0x4faf6a;
    NativeRoutes routes(lookup);NativeOwnership service(image,routes,lookup,{},caller);service.set_game_thread(99);
    MT2FW_MT2_ReleaseV1 api{},second{},unavailable{};
    check(service.query_release(&a.host,1,&api,sizeof(api))==MT2FW_MT2_OK,"exact multiplayer host obtains separate extension");
    check(service.query_release(&b.host,1,&second,sizeof(second))==MT2FW_MT2_OK,"another actual host has its own context");
    auto copied=a.host;
    check(service.query_release(&copied,1,&unavailable,sizeof(unavailable))==MT2FW_MT2_UNAVAILABLE,"copied host is rejected");
    check(service.query_release(&other.host,1,&unavailable,sizeof(unavailable))==MT2FW_MT2_UNAVAILABLE,"unrelated native mod cannot control release");
    check(service.query_release(&a.host,2,&unavailable,sizeof(unavailable))==MT2FW_MT2_INVALID,"wrong version is rejected");
    check(service.query_release(&a.host,1,&unavailable,sizeof(unavailable)-1)==MT2FW_MT2_INVALID,"short API capacity is rejected");
    a.staging=true;check(service.query_release(&a.host,1,&unavailable,sizeof(unavailable))==MT2FW_MT2_UNAVAILABLE,"staging host is rejected");a.staging=false;
    a.active=false;check(service.query_release(&a.host,1,&unavailable,sizeof(unavailable))==MT2FW_MT2_UNAVAILABLE,"inactive host is rejected");a.active=true;
    a.manifest.type="lua";check(service.query_release(&a.host,1,&unavailable,sizeof(unavailable))==MT2FW_MT2_UNAVAILABLE,"Lua host is rejected");a.manifest.type="native";
    NativeOwnership old_service(image,routes,lookup,{});
    check(old_service.query_release(&a.host,1,&unavailable,sizeof(unavailable))==MT2FW_MT2_UNAVAILABLE,"no configured physical caller leaves extension unavailable");
    int receiver=1,native_string=2;
    MT2FW_MT2_ReleaseCall call{sizeof(call),1,0,0,0,0,99,&receiver,&native_string,caller,"LevelUp",7};
    check(service.release_command(call)==MT2FW_MT2_RELEASE_FORWARD,"unbound original LevelUp forwards");
    Delegate delegate;delegate.service=&service;delegate.plugin=&a;delegate.api=&api;
    MT2FW_MT2_ReleaseReceipt receipt{sizeof(receipt)};
    check(api.bind(api.context,nullptr,&delegate,&receipt)==MT2FW_MT2_INVALID,"null delegate is rejected");
    receipt.size=1;check(api.bind(api.context,decide,&delegate,&receipt)==MT2FW_MT2_INVALID,"short receipt is rejected");receipt.size=sizeof(receipt);
    auto bind=[&]{check(api.bind(api.context,decide,&delegate,&receipt)==MT2FW_MT2_OK&&receipt.bound&&receipt.registration&&!receipt.failed,"exact owner binds one fresh registration");return receipt.registration;};
    auto token=bind();
    check(std::strcmp(receipt.owner,"doodesch.multiplayer")==0,"registration receipt names its exact owner");
    check(second.bind(second.context,decide,&delegate,&receipt)==MT2FW_MT2_BUSY,"second host cannot replace a registration");
    check(second.unbind(second.context,token,&receipt)==MT2FW_MT2_INVALID,"foreign host cannot unbind");
    check(second.read(second.context,&receipt)==MT2FW_MT2_UNAVAILABLE,"foreign context cannot read another registration");
    auto local=call;local.native_return=caller+1;
    check(service.release_command(local)==MT2FW_MT2_RELEASE_FORWARD&&!delegate.calls,"another actual caller remains local");
    local=call;local.text="Quit";local.length=4;
    check(service.release_command(local)==MT2FW_MT2_RELEASE_FORWARD&&!delegate.calls,"unrelated dispatcher command remains local");
    check(service.release_command(call)==MT2FW_MT2_RELEASE_FORWARD&&delegate.calls==1,"explicit candidate Forward does not suppress local release");
    check(delegate.call.receiver==&receiver&&delegate.call.native_command==&native_string&&delegate.call.native_return==caller&&delegate.call.thread==99&&delegate.call.registration==token&&delegate.call.invocation&&delegate.text=="LevelUp","original receiver GNU object caller thread registration and text are preserved");
    auto invocation=delegate.call.invocation;delegate.mode=1;
    check(service.release_command(call)==MT2FW_MT2_RELEASE_SUPPRESS&&delegate.calls==2&&delegate.call.invocation>invocation,"synchronous suppression and monotonic invocation");
    check(api.read(api.context,&receipt)==MT2FW_MT2_OK&&receipt.last_disposition==MT2FW_MT2_RELEASE_SUPPRESS&&!receipt.inflight,"receipt acknowledges decision and completed lease only");
    MT2FW_MT2_OwnershipV1 v1{};check(service.query(&a.host,1,&v1,sizeof(v1))==MT2FW_MT2_OK,"unchanged passive V1 remains available");
    int observed=0;auto passive_token=v1.command_subscribe(v1.context,passive,&observed);
    MT2FW_MT2_CommandCall observation{sizeof(observation),1,0,0,0,99,&receiver,"LevelUp",7};service.observe_command(observation);
    check(observed==1&&passive_token,"passive V1 subscription remains independent");
    check(v1.command_unsubscribe(v1.context,passive_token)==MT2FW_MT2_OK,"passive V1 unsubscribe remains independent");
    auto unbind=[&]{check(api.unbind(api.context,token,&receipt)==MT2FW_MT2_OK&&!receipt.bound&&!receipt.registration,"explicit unbind retires registration");};
    for(int mode:{2,3,4,5,6,7,11}){
        unbind();token=bind();delegate.mode=mode;auto before=delegate.calls;
        check(service.release_command(call)==MT2FW_MT2_RELEASE_FAILED&&delegate.calls==before+1,"failure invalid decision or exception blocks candidate");
        check(api.read(api.context,&receipt)==MT2FW_MT2_FAILED&&receipt.failed&&receipt.cause[0]&&!receipt.inflight,"original failure cause and completed lease remain readable");
        auto cause=std::string(receipt.cause);
        if(mode==2)check(cause=="original celebration owner changed","consumer failure cause preserved");
        if(mode==3)check(cause.find("original release proof failed")!=std::string::npos,"native exception cause preserved");
        if(mode==7)check(cause=="consumer retained an uncertain original result","failed callback cannot forward despite Forward disposition");
        check(service.release_command(call)==MT2FW_MT2_RELEASE_FAILED&&delegate.calls==before+1,"failed registration never silently retries");
        check(api.read(api.context,&receipt)==MT2FW_MT2_FAILED&&std::string(receipt.cause)==cause,"later failures do not replace original cause");
        check(service.release_command(local)==MT2FW_MT2_RELEASE_FORWARD,"failed release registration leaves unrelated command local");
    }
    unbind();token=bind();delegate.mode=0;auto wrong_thread=call;wrong_thread.thread=100;
    check(service.release_command(wrong_thread)==MT2FW_MT2_RELEASE_FAILED,"wrong game thread blocks candidate before delegate");
    unbind();token=bind();auto unreadable=call;unreadable.readable=0;unreadable.read_error=998;
    check(service.release_command(unreadable)==MT2FW_MT2_RELEASE_FAILED,"unreadable expected dispatcher command preserves read failure");
    check(api.read(api.context,&receipt)==MT2FW_MT2_FAILED&&std::string(receipt.cause).find("998")!=std::string::npos,"original native read cause retained");
    unbind();token=bind();delegate.mode=8;
    check(service.release_command(call)==MT2FW_MT2_RELEASE_SUPPRESS&&delegate.inside==MT2FW_MT2_BUSY,"unbind during callback waits for its borrowed storage");
    auto before=delegate.calls;
    check(service.release_command(call)==MT2FW_MT2_RELEASE_FORWARD&&delegate.calls==before,"disabled registration gets no later callback");
    unbind();auto old_token=token;token=bind();
    check(token!=old_token&&api.unbind(api.context,old_token,&receipt)==MT2FW_MT2_INVALID,"stale registration cannot unbind a replacement");
    delegate.mode=9;
    check(service.release_command(call)==MT2FW_MT2_RELEASE_FAILED&&delegate.inside==MT2FW_MT2_RELEASE_FAILED,"recursive candidate fails outer decision without replay");
    unbind();token=bind();delegate.mode=10;
    check(service.release_command(call)==MT2FW_MT2_RELEASE_SUPPRESS&&!delegate.retired&&delegate.blocked,"plugin retirement waits for current delegate");
    check(service.retire(a)&&!service.retirement_blocked(),"plugin retirement completes after callback returns");
    check(api.bind(api.context,decide,&delegate,&receipt)==MT2FW_MT2_UNAVAILABLE,"retired host cannot reactivate delegate");
    before=delegate.calls;check(service.release_command(call)==MT2FW_MT2_RELEASE_FORWARD&&delegate.calls==before,"retired delegate receives no later invocation");
    check(!service.drain_errors().empty(),"original causes remain in mod-owned error reporting");
    std::cout<<checks<<" release boundary checks passed\n";return 0;
}catch(const std::exception& problem){std::cerr<<"Release boundary failed after "<<checks<<": "<<problem.what()<<"\n";return 1;}}
