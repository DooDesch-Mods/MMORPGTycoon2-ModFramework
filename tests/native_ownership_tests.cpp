#include "native_ownership.h"
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_version.h>
#include <cstring>
#include <iostream>

using namespace mt2fw;
using namespace mt2fw::mt2;
namespace {
int checks=0,shutdowns=0,free_calls=0;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
void MT2FW_CALL free_entry(void*){++free_calls;}
void MT2FW_CALL shutdown_plugin(){++shutdowns;}
struct Commands {int calls=0;std::string text;void* receiver=nullptr;uint64_t thread=0,invocation=0;uint32_t readable=0;bool fail=false;NativeOwnership* service=nullptr;Plugin* retire_plugin=nullptr;bool retired=true,blocked=false;};
void MT2FW_CALL command(void* user,const MT2FW_MT2_CommandCall* call){auto& value=*static_cast<Commands*>(user);++value.calls;value.receiver=call->receiver;value.thread=call->thread;value.invocation=call->invocation;value.readable=call->readable;if(call->readable)value.text.assign(call->text,call->length);if(value.retire_plugin){value.retired=value.service->retire(*value.retire_plugin);value.blocked=value.service->retirement_blocked();}if(value.fail)throw std::runtime_error("synthetic command failure");}
struct Inputs {int calls=0;std::vector<uint32_t> types;const void* original=nullptr;bool mutate=false,fail=false;MT2FW_MT2_OwnershipV1* api=nullptr;uint64_t token=0;MT2FW_MT2_Result unsubscribed=MT2FW_MT2_FAILED;};
void MT2FW_CALL input(void* user,const MT2FW_MT2_PolledEvent* call){
    auto& value=*static_cast<Inputs*>(user);check(call->event_bytes==sizeof(SDL_Event)&&call->sdl_version==SDL_VERSION&&call->thread==99,"Actual SDL event byte/version/thread contract");check(call->event!=value.original,"Input observer cannot alter the original event structure");
    SDL_Event event{};std::memcpy(&event,call->event,sizeof(event));++value.calls;value.types.push_back(event.type);
    if(value.mutate)static_cast<SDL_Event*>(const_cast<void*>(call->event))->type=SDL_EVENT_QUIT;
    if(value.api)value.unsubscribed=value.api->input_unsubscribe(value.api->context,value.token);
    if(value.fail)throw std::runtime_error("synthetic input observer failure");
}
}
int main(){try{
    auto root=fs::current_path()/"artifacts/native-ownership-tests";fs::create_directories(root/"logs");Runtime runtime(root);Plugin a(&runtime),b(&runtime);
    a.manifest.id="test.multiplayer";b.manifest.id="test.other";
    for(auto plugin:{&a,&b}){plugin->active=true;plugin->staging=false;plugin->manifest.type="native";}
    Plugin* extra=nullptr;auto lookup=[&](const MT2FW_Host* host){return host==&a.host?&a:host==&b.host?&b:extra&&host==&extra->host?extra:nullptr;};
    constexpr uintptr_t image=0x100000;NativeRoutes routes(lookup);NativeRoutes::Backend hooks;
    std::map<uintptr_t,NativeRoutes::Bytes> code,original;
    for(auto rva:{uintptr_t(64),uintptr_t(0x1264b60)}){NativeRoutes::Bytes bytes{};bytes.fill(0x90);code[image+rva]=bytes;original[image+rva]=bytes;}
    int created=0,enabled=0,disabled=0,removed=0;
    hooks.pristine=[&](uint64_t rva,uint32_t offset,void* output,size_t count,std::string& cause){auto found=original.find(image+rva);if(found==original.end()||offset+count>32){cause="unknown pristine route";return false;}std::memcpy(output,found->second.data()+offset,count);return true;};
    hooks.read=[&](uintptr_t address,void* output,size_t count,std::string& cause){auto found=code.find(address);if(found==code.end()||count>32){cause="unknown live route";return false;}std::memcpy(output,found->second.data(),count);return true;};
    hooks.create=[&](void* address,void*,void** trampoline,std::string&){++created;*trampoline=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(address)+0x10000);return true;};
    hooks.enable=[&](void* address,void*,std::string&){++enabled;code.at(reinterpret_cast<uintptr_t>(address))[0]=0xe9;return true;};
    hooks.prove=[&](void* address,void*,std::string& cause){if(code.at(reinterpret_cast<uintptr_t>(address))[0]==0xe9)return true;cause="wrong owned route";return false;};
    hooks.disable=[&](void* address,std::string&){++disabled;code.at(reinterpret_cast<uintptr_t>(address))=original.at(reinterpret_cast<uintptr_t>(address));return true;};
    hooks.remove=[&](void* address,std::string&){++removed;code.at(reinterpret_cast<uintptr_t>(address))=original.at(reinterpret_cast<uintptr_t>(address));return true;};
    routes.configure(image,hooks);routes.catalog("ordinary",64);routes.catalog("sized",0x1264b60);
    NativeOwnership::Snapshot live;live.slot=image+0x1c52a00;live.malloc_slot=image+0x1c52a70;live.current=0x700000;live.malloc_current=0x710000;live.protection=2;
    live.free_export.resolved=live.current;live.malloc_export.resolved=live.malloc_current;
    live.imported_crt.base=0x300000;live.free_provider.base=0x400000;live.malloc_provider.base=0x500000;
    live.free_export.owner=live.free_provider.base;live.malloc_export.owner=live.malloc_provider.base;
    for(auto proof:{&live.imported_crt,&live.free_provider,&live.malloc_provider}){proof->pinned=1;proof->bytes=1024;std::memset(proof->sha256,'a',64);std::strcpy(proof->path,"synthetic-module.dll");}
    bool prepare_fail=false,read_fail=false,race=false,restore_fail=false,before_fail=false;
    int exchanges=0,reads=0;bool repair_seen=false;NativeOwnership::Backend imports;
    imports.prepare=[&](NativeOwnership::Snapshot& output,std::string& cause){if(prepare_fail){cause="wrong exact import name";return false;}output=live;return true;};
    imports.read=[&](NativeOwnership::Snapshot& output,std::string& cause){++reads;if(read_fail){cause="synthetic provider read failure";return false;}output=live;return true;};
    imports.replacement=[](uintptr_t value,std::string&){return value==reinterpret_cast<uintptr_t>(&free_entry);};
    imports.exchange=[&](uintptr_t expected,uintptr_t replacement,uint32_t protection,bool repair){
        ++exchanges;repair_seen=repair;NativeOwnership::Exchange result;
        if(before_fail){result.win32=5;return result;}
        result.attempted=true;result.before=live.protection;live.protection=4;
        if(race)live.current=0xdeadbeef;
        result.observed=live.current;result.exchanged=live.current==expected;if(result.exchanged)live.current=replacement;
        result.protection_restored=!restore_fail;if(result.protection_restored)live.protection=protection;else result.win32=5;
        if(!result.exchanged&&!result.win32)result.win32=5023;return result;
    };
    NativeOwnership service(image,routes,lookup,imports);MT2FW_MT2_OwnershipV1 api{},other{};MT2FW_MT2_RoutesV1 route_api{};
    check(service.query(&a.host,1,&api,sizeof(api))==MT2FW_MT2_OK,"actual ownership host");
    check(service.query(&b.host,1,&other,sizeof(other))==MT2FW_MT2_OK,"second ownership host");
    auto copied=a.host;check(service.query(&copied,1,&api,sizeof(api))==MT2FW_MT2_UNAVAILABLE,"copied host identity rejected");
    check(service.query(&a.host,2,&api,sizeof(api))==MT2FW_MT2_INVALID,"wrong extension version rejected");
    check(routes.query(&a.host,1,&route_api,sizeof(route_api))==MT2FW_MT2_OK,"existing RoutesV1 remains available");
    MT2FW_MT2_RouteReceipt route{sizeof(route)};
    check(route_api.hook_prepare(route_api.context,"ordinary",reinterpret_cast<void*>(&free_entry),&route)==MT2FW_MT2_OK,"ordinary route prepared");auto trampoline=route.trampoline;
    check(route_api.hook_enable(route_api.context,"ordinary",&route)==MT2FW_MT2_OK,"ordinary route enabled");
    check(other.hook_disable(other.context,"ordinary",&route)==MT2FW_MT2_UNAVAILABLE,"foreign disable rejected");
    check(api.hook_disable(api.context,"ordinary",&route)==MT2FW_MT2_OK&&route.state==5&&route.trampoline==trampoline&&disabled==1&&!removed,"disable retains actual trampoline");
    check(route_api.hook_remove(route_api.context,"ordinary",&route)==MT2FW_MT2_BUSY&&!removed,"retained original cannot be freed by remove");
    check(api.hook_disable(api.context,"ordinary",&route)==MT2FW_MT2_OK&&disabled==1,"already-disabled readback has no second mutation");
    check(api.hook_resume(api.context,"ordinary",&route)==MT2FW_MT2_OK&&route.trampoline==trampoline&&route.state==2&&created==1,"resume uses same hook and trampoline");
    check(api.hook_disable(api.context,"ordinary",&route)==MT2FW_MT2_OK&&disabled==2,"resumed route disables without removal");
    check(route_api.hook_prepare(route_api.context,"sized",reinterpret_cast<void*>(&free_entry),&route)==MT2FW_MT2_OK,"native free sized route prepared");
    check(route_api.hook_remove(route_api.context,"sized",&route)==MT2FW_MT2_BUSY&&a.native_retained&&!removed,"native free originals sticky from preparation");
    Commands observed;auto subscription=api.command_subscribe(api.context,command,&observed);check(subscription!=0,"Mode observer subscribed");
    int receiver=1;MT2FW_MT2_CommandCall call{sizeof(call),1,0,0,0,99,&receiver,"LevelUp",7};service.observe_command(call);
    check(observed.calls==1&&observed.text=="LevelUp"&&observed.receiver==&receiver&&observed.thread==99&&observed.invocation,"passive original LevelUp receiver/thread/text observed");
    observed.fail=true;service.observe_command(call);check(observed.calls==2,"observer failure contained");observed.fail=false;
    call.readable=0;call.text=nullptr;call.length=0;call.read_error=998;service.observe_command(call);check(observed.calls==3&&!observed.readable,"unreadable native command stays distinguishable");
    check(other.command_unsubscribe(other.context,subscription)==MT2FW_MT2_INVALID,"foreign observer unsubscribe rejected");
    check(api.command_unsubscribe(api.context,subscription)==MT2FW_MT2_OK,"Mode observer unsubscribe");service.observe_command(call);check(observed.calls==3,"unsubscribed observer removed");
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=SDL_SCANCODE_SPACE;
    Inputs first_input,second_input;first_input.original=second_input.original=&event;first_input.mutate=true;
    auto first_token=api.input_subscribe(api.context,input,&first_input),second_token=other.input_subscribe(other.context,input,&second_input);
    check(first_token&&second_token,"Independent polled input observers registered");
    MT2FW_MT2_PolledEvent polled{sizeof(polled),sizeof(event),SDL_VERSION,0,0,99,&event};service.observe_input(polled);
    check(event.type==SDL_EVENT_KEY_DOWN&&first_input.types.back()==SDL_EVENT_KEY_DOWN&&second_input.types.back()==SDL_EVENT_KEY_DOWN,"Polled event and other observers survive copy mutation");
    event.type=SDL_EVENT_KEY_UP;service.observe_input(polled);check(first_input.types.back()==SDL_EVENT_KEY_UP&&second_input.types.back()==SDL_EVENT_KEY_UP,"Unbound key-up reaches passive observer");
    first_input.fail=true;event.type=SDL_EVENT_MOUSE_MOTION;service.observe_input(polled);check(second_input.types.back()==SDL_EVENT_MOUSE_MOTION&&event.type==SDL_EVENT_MOUSE_MOTION,"Observer failure does not suppress returned mouse event or other mods");first_input.fail=false;
    check(other.input_unsubscribe(other.context,first_token)==MT2FW_MT2_INVALID,"Foreign input observer token rejected");
    check(api.input_unsubscribe(api.context,first_token)==MT2FW_MT2_OK,"Input observer unsubscribed");int calls=first_input.calls;service.observe_input(polled);check(first_input.calls==calls&&second_input.calls==4,"Only subscribed input observers continue");
    check(other.input_unsubscribe(other.context,second_token)==MT2FW_MT2_OK,"Second input observer unsubscribed");
    Inputs self_unsubscribe;self_unsubscribe.original=&event;self_unsubscribe.api=&other;self_unsubscribe.token=other.input_subscribe(other.context,input,&self_unsubscribe);
    service.observe_input(polled);check(self_unsubscribe.unsubscribed==MT2FW_MT2_BUSY,"Unsubscribe cannot release in-flight input userdata");
    calls=self_unsubscribe.calls;service.observe_input(polled);check(self_unsubscribe.calls==calls,"Pending unsubscribe stops future polled callbacks");
    check(other.input_unsubscribe(other.context,self_unsubscribe.token)==MT2FW_MT2_OK,"Input unsubscribe confirms after callback returns");
    MT2FW_MT2_FreeReceipt receipt{sizeof(receipt)};prepare_fail=true;
    check(api.free_prepare(api.context,free_entry,&receipt)==MT2FW_MT2_FAILED&&exchanges==0&&std::string(receipt.cause).find("wrong exact import name")!=std::string::npos,"exact import mismatch prevents mutation");prepare_fail=false;
    live.free_provider.pinned=0;check(api.free_prepare(api.context,free_entry,&receipt)==MT2FW_MT2_FAILED&&!exchanges,"unpinned provider rejected");live.free_provider.pinned=1;
    check(api.free_prepare(api.context,free_entry,&receipt)==MT2FW_MT2_OK&&receipt.state==1&&receipt.original==0x700000&&receipt.malloc_original==0x710000&&receipt.initial_protection==2&&receipt.live_confirmed,"prepare publishes verified original before CAS");
    check(receipt.free_provider.pinned&&receipt.free_export.owner==receipt.free_provider.base&&std::strlen(receipt.free_provider.sha256)==64,"provider identity retained");
    check(other.free_prepare(other.context,free_entry,&receipt)==MT2FW_MT2_BUSY,"single free-IAT owner");
    check(other.free_enable(other.context,&receipt)==MT2FW_MT2_UNAVAILABLE&&!exchanges,"foreign enable rejected");
    check(api.free_enable(api.context,&receipt)==MT2FW_MT2_OK&&receipt.state==2&&receipt.exchange_attempted&&receipt.exchanged&&receipt.protection_restored&&live.current==reinterpret_cast<uintptr_t>(&free_entry),"CAS enable and protection confirmed");
    int count=exchanges;check(api.free_enable(api.context,&receipt)==MT2FW_MT2_INVALID&&exchanges==count,"enabled CAS cannot replay");
    check(api.free_remove(api.context,&receipt)==MT2FW_MT2_INVALID&&exchanges==count,"enabled metadata cannot retire");
    check(api.free_read(api.context,&receipt)==MT2FW_MT2_OK&&receipt.current==reinterpret_cast<uintptr_t>(&free_entry),"live free route readback");
    check(api.free_disable(api.context,&receipt)==MT2FW_MT2_OK&&receipt.state==5&&live.current==0x700000&&live.protection==2,"owned CAS restore and original protection");
    count=exchanges;check(api.free_disable(api.context,&receipt)==MT2FW_MT2_OK&&exchanges==count,"already-original disable confirmed without exchange");
    race=true;check(api.free_enable(api.context,&receipt)==MT2FW_MT2_FAILED&&receipt.state==3&&!receipt.exchanged&&live.current==0xdeadbeef,"CAS race preserves foreign pointer");race=false;count=exchanges;
    check(api.free_enable(api.context,&receipt)==MT2FW_MT2_INVALID&&exchanges==count,"unknown CAS is never replayed");
    check(api.free_disable(api.context,&receipt)==MT2FW_MT2_FAILED&&exchanges==count&&live.current==0xdeadbeef,"foreign pointer never restored blindly");
    live.current=0x700000;check(api.free_disable(api.context,&receipt)==MT2FW_MT2_OK,"explicit cleanup acknowledges measured original");
    restore_fail=true;check(api.free_enable(api.context,&receipt)==MT2FW_MT2_FAILED&&receipt.state==3&&receipt.exchanged&&!receipt.protection_restored&&live.protection==4,"swapped pointer with failed protection stays unknown");
    auto primary=std::string(receipt.cause);count=exchanges;check(api.free_read(api.context,&receipt)==MT2FW_MT2_FAILED&&receipt.live_confirmed&&std::string(receipt.cause)==primary&&exchanges==count,"unknown read preserves original cause without mutation");
    restore_fail=false;check(api.free_disable(api.context,&receipt)==MT2FW_MT2_OK&&repair_seen&&live.current==0x700000&&live.protection==2,"explicit owned recovery restores initial protection");
    before_fail=true;check(api.free_enable(api.context,&receipt)==MT2FW_MT2_FAILED&&!receipt.exchange_attempted&&receipt.state==5&&live.current==0x700000,"never-executed exchange distinct from unknown mutation");before_fail=false;
    check(api.free_enable(api.context,&receipt)==MT2FW_MT2_OK,"explicit later enable after never-executed failure");
    read_fail=true;count=exchanges;check(api.free_read(api.context,&receipt)==MT2FW_MT2_FAILED&&!receipt.live_confirmed&&!receipt.current&&exchanges==count,"failed readback cannot look confirmed");read_fail=false;
    check(api.free_disable(api.context,&receipt)==MT2FW_MT2_OK,"owned cleanup after failed observation");
    live.malloc_current^=8;count=exchanges;check(api.free_enable(api.context,&receipt)==MT2FW_MT2_FAILED&&exchanges==count,"malloc sibling/provider drift blocks free mutation");live.malloc_current=0x710000;
    check(api.free_disable(api.context,&receipt)==MT2FW_MT2_OK,"explicit original receipt after sibling recovery");
    check(api.free_remove(api.context,&receipt)==MT2FW_MT2_OK&&receipt.state==4&&!receipt.owner[0],"confirmed disabled import metadata retires");
    check(api.free_prepare(api.context,free_entry,&receipt)==MT2FW_MT2_OK,"new owned preparation after confirmed metadata retirement");
    check(api.free_enable(api.context,&receipt)==MT2FW_MT2_OK,"new owned generation enable");
    restore_fail=true;check(!service.retire(a)&&service.retirement_blocked()&&live.protection==4,"Failed retirement protection remains blocked and visible");restore_fail=false;
    check(service.retire(a)&&service.has_retention()&&!service.retirement_blocked()&&live.current==0x700000&&live.protection==2,"Explicit measured import retirement restores route and retains owner state");
    check(routes.retire(a)&&routes.has_retention()&&!routes.retirement_blocked()&&removed==0,"automatic route retirement never frees retained originals");
    check(api.free_enable(api.context,&receipt)==MT2FW_MT2_UNAVAILABLE,"retired owner cannot reactivate import");
    check(api.hook_resume(api.context,"ordinary",&route)==MT2FW_MT2_UNAVAILABLE,"retired owner cannot resume code hook");
    a.native_loaded=true;a.native_shutdown=shutdown_plugin;a.native_shutdown_pending=true;a.native_pins=1;runtime.release_native(a);
    check(a.native_pins==0&&a.native_loaded&&!shutdowns,"retained native state survives final callback lease");
    Plugin c(&runtime);c.manifest.id="test.command.retirement";c.manifest.type="native";c.active=true;c.staging=false;extra=&c;MT2FW_MT2_OwnershipV1 command_api{};
    check(service.query(&c.host,1,&command_api,sizeof(command_api))==MT2FW_MT2_OK,"Independent command lifecycle owner");
    Commands retiring;retiring.service=&service;retiring.retire_plugin=&c;check(command_api.command_subscribe(command_api.context,command,&retiring)!=0,"Retiring command observer registered");
    service.observe_command(call);check(!retiring.retired&&retiring.blocked&&retiring.calls==1,"Command retirement retains current callback storage");
    check(service.retire(c)&&!service.retirement_blocked(),"Command retirement confirms after callback completion");service.observe_command(call);check(retiring.calls==1,"Retired command observer receives no later callback");
    check(!service.drain_errors().empty()&&!free_calls,"original failures retained and no extra free dispatcher executes");
    std::cout<<checks<<" native ownership checks passed\n";return 0;
}catch(const std::exception& problem){std::cerr<<"Native ownership check failed after "<<checks<<": "<<problem.what()<<"\n";return 1;}}
