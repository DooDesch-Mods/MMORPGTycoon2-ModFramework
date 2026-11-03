#include "native_services.h"
#include "determinism_presentation_rng.h"
#include <windows.h>
#include <intrin.h>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <thread>
#include <type_traits>
using namespace mt2fw;
namespace mt2=mt2fw::mt2;
using Selection=mt2::determinism::presentation::Selection;
using Service=mt2::NativeServices;
static_assert(sizeof(MT2FW_MT2_ServiceV1)==144&&sizeof(MT2FW_MT2_ServiceV2)==160);
static_assert(sizeof(MT2FW_MT2_CharacterPreviewIntCall)==40&&sizeof(MT2FW_MT2_CharacterPreviewIntSelection)==176);
static_assert(std::is_trivially_destructible_v<Service::CharacterPreviewLease>);
struct Random {uint64_t words[4]={11,22,33,44};int32_t extra=0;uint32_t padding=0;};
static_assert(sizeof(Random)==40);
enum class Mode {Select,Pass,Fail,CppFault,SehFault,BadSize,BadBytes,BadPointer,Shared,Slot,NoAccess,Unbind,Retire,Recursive};
constexpr DWORD original_fault=0xE0770001u,trace_fault=0xE0770002u;
std::unique_ptr<Service> native_services;
Plugin* active_plugin=nullptr;Runtime* active_runtime=nullptr;
MT2FW_MT2_ServiceV2 api{};uint64_t token=0;
uintptr_t image=0,fake_caller=0;Random* shared=nullptr;Random local,unrelated_random;
void* inaccessible=nullptr;Mode mode=Mode::Select;
bool proof_ok=true,original_cpp=false,original_seh=false,trace_seh=false,fields_ok=true;
unsigned checks=0,callbacks=0,originals=0,trace_begins=0,trace_finishes=0,shutdowns=0;
bool expected_pin=false,pin_observations=true;void* before_source=nullptr;uint64_t before_word=0,after_word=0;
MT2FW_MT2_Result unbind_result=MT2FW_MT2_FAILED;
mt2::determinism::presentation::Counters existing_count;
void* existing_source=nullptr;
void need(bool okay,const char* message){++checks;if(!okay)throw std::runtime_error(message);}
Selection existing_select(uintptr_t,uintptr_t,void* source){return existing_source?Selection{existing_source,&existing_count}:Selection{source};}
Selection(*select_source)(uintptr_t,uintptr_t,void*)=existing_select;
size_t begin(uintptr_t helper,uintptr_t caller,void* source){++trace_begins;before_source=source;before_word=static_cast<Random*>(source)->words[0];
    fields_ok=fields_ok&&helper==0x9fc370&&caller==fake_caller;
    pin_observations=pin_observations&&((active_plugin->native_pins!=0)==expected_pin);return 1;}
void finish(size_t,int){++trace_finishes;after_word=static_cast<Random*>(before_source)->words[0];
    pin_observations=pin_observations&&((active_plugin->native_pins!=0)==expected_pin)&&shutdowns==0;
    if(trace_seh)RaiseException(trace_fault,EXCEPTION_NONCONTINUABLE,0,nullptr);}
int original(void* source,int limit){++originals;pin_observations=pin_observations&&((active_plugin->native_pins!=0)==expected_pin)&&shutdowns==0;
    if(original_cpp)throw std::runtime_error("original C++ failure");
    if(original_seh)RaiseException(original_fault,EXCEPTION_NONCONTINUABLE,0,nullptr);
    auto* state=static_cast<Random*>(source);++state->words[0];return static_cast<int>(state->words[0]%static_cast<uint64_t>(limit));}
int(*original_int)(void*,int)=original;
void* fake_return(){return reinterpret_cast<void*>(fake_caller);}
#define _ReturnAddress fake_return
#include "preview_get_int_actual.inc"
#undef _ReturnAddress
DWORD invoke_pod(void* source,int limit,int* output){
    __try{*output=get_int(source,limit);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
void MT2FW_CALL shutdown_plugin(){++shutdowns;}
MT2FW_MT2_Result MT2FW_CALL selector(void*,const MT2FW_MT2_CharacterPreviewIntCall* call,MT2FW_MT2_CharacterPreviewIntSelection* out){
    ++callbacks;fields_ok=fields_ok&&call->size==40&&!call->reserved&&!call->reserved2&&call->receiver==shared&&
        call->return_rva==fake_caller-image&&call->thread==GetCurrentThreadId()&&call->limit==97&&active_plugin->native_pins==1;
    out->source=&local;out->source_bytes=40;
    switch(mode){
    case Mode::Pass:out->size=0;out->source=reinterpret_cast<void*>(1);return MT2FW_MT2_UNAVAILABLE;
    case Mode::Fail:std::memcpy(out->cause,"synthetic selector failure",sizeof("synthetic selector failure"));return MT2FW_MT2_FAILED;
    case Mode::CppFault:throw std::runtime_error("selector C++ failure");
    case Mode::SehFault:RaiseException(original_fault,EXCEPTION_NONCONTINUABLE,0,nullptr);break;
    case Mode::BadSize:out->size=16;break;
    case Mode::BadBytes:out->source_bytes=39;break;
    case Mode::BadPointer:out->source=reinterpret_cast<void*>(1);break;
    case Mode::Shared:out->source=shared;break;
    case Mode::Slot:out->source=reinterpret_cast<void*>(image+0x164b250);break;
    case Mode::NoAccess:out->source=inaccessible;break;
    case Mode::Unbind:unbind_result=api.character_preview_int_unbind(api.v1.context,token);break;
    case Mode::Retire:active_plugin->native_shutdown_pending=true;active_runtime->disable(*active_plugin,"synthetic mid-call retirement");break;
    case Mode::Recursive:{int ignored=0;const auto exception=invoke_pod(shared,97,&ignored);fields_ok=fields_ok&&exception==MT2FW_MT2_CHARACTER_PREVIEW_ABORT;break;}
    case Mode::Select:break;
    }
    return MT2FW_MT2_OK;
}
fs::path test_root(){auto root=fs::current_path()/"artifacts/character-preview-tests";fs::create_directories(root/"logs");return root;}
struct Fixture {
    fs::path root=test_root();Runtime runtime{root};Plugin plugin{&runtime},foreign{&runtime};
    Fixture(){
        image=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,0x1d00000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));need(image!=0,"allocate owned synthetic image");
        shared=reinterpret_cast<Random*>(image+0x1c4e7a0);*shared=Random{};*reinterpret_cast<uintptr_t*>(image+0x164b250)=reinterpret_cast<uintptr_t>(shared);
        fake_caller=image+0x9061c;local=Random{};unrelated_random=Random{};mode=Mode::Select;proof_ok=true;fields_ok=true;
        callbacks=originals=trace_begins=trace_finishes=shutdowns=0;original_cpp=original_seh=trace_seh=false;existing_source=nullptr;existing_count.completed=0;
        expected_pin=true;pin_observations=true;unbind_result=MT2FW_MT2_FAILED;
        active_plugin=&plugin;active_runtime=&runtime;plugin.manifest.id="doodesch.multiplayer";foreign.manifest.id="test.foreign";
        for(auto* p:{&plugin,&foreign}){p->manifest.type="native";p->staging=false;p->active=true;p->directory=root/p->manifest.id;}
        native_services=std::make_unique<Service>(root/"game",root,[&](const MT2FW_Host* host){return host==&plugin.host?&plugin:host==&foreign.host?&foreign:nullptr;});
        native_services->set_game_thread(GetCurrentThreadId());
        native_services->character_preview_backend(image,[](std::string& cause){if(!proof_ok)cause="synthetic owned hook proof failed";return proof_ok;});
        runtime.native_retire=[&](Plugin& value){return native_services->retire(value);};
        api={};need(native_services->query(&plugin.host,2,&api.v1,sizeof(api))==MT2FW_MT2_OK,"query typed V2 service");token=0;
    }
    void bind(){need(api.character_preview_int_bind(api.v1.context,selector,nullptr,&token)==MT2FW_MT2_OK&&token,"bind supported selector");}
    ~Fixture(){native_services.reset();if(!VirtualFree(reinterpret_cast<void*>(image),0,MEM_RELEASE))std::cerr<<"synthetic image release failed\n";}
};
void basic_api(){Fixture f;MT2FW_MT2_ServiceV1 v1{};need(native_services->query(&f.plugin.host,1,&v1,sizeof(v1))==MT2FW_MT2_OK&&v1.version==1&&v1.size==144,"V1 prefix remains unchanged");
    need(api.v1.version==2&&api.v1.size==160&&api.v1.frame_subscribe==v1.frame_subscribe,"V2 retains V1 functions");
    need(native_services->query(&f.plugin.host,2,&v1,sizeof(v1))==MT2FW_MT2_INVALID,"V1 capacity cannot receive V2");
    need(native_services->query(&f.plugin.host,3,&v1,sizeof(v1))==MT2FW_MT2_INVALID,"unknown version rejected");
    MT2FW_MT2_ServiceV2 foreign{};need(native_services->query(&f.foreign.host,2,&foreign.v1,sizeof(foreign))==MT2FW_MT2_OK,"foreign owner may query unchanged service functions");
    uint64_t wrong=0;need(foreign.character_preview_int_bind(foreign.v1.context,selector,nullptr,&wrong)==MT2FW_MT2_UNAVAILABLE,"foreign mod cannot bind selector");
    proof_ok=false;need(api.character_preview_int_bind(api.v1.context,selector,nullptr,&wrong)==MT2FW_MT2_FAILED,"unproved physical hook rejects binding");proof_ok=true;
    f.bind();need(api.character_preview_int_bind(api.v1.context,selector,nullptr,&wrong)==MT2FW_MT2_BUSY,"no silent registration replacement");
    need(foreign.character_preview_int_unbind(foreign.v1.context,token)==MT2FW_MT2_INVALID,"foreign token owner rejected");
    need(api.character_preview_int_unbind(api.v1.context,token+1)==MT2FW_MT2_INVALID,"stale token rejected");
    MT2FW_MT2_Result worker_result=MT2FW_MT2_OK;std::thread worker([&]{worker_result=api.character_preview_int_unbind(api.v1.context,token);});worker.join();
    need(worker_result==MT2FW_MT2_UNAVAILABLE,"wrong-thread mutation rejected");
    need(api.character_preview_int_unbind(api.v1.context,token)==MT2FW_MT2_OK,"owner unregisters idle selector");}
void selections(){for(uintptr_t rva:{uintptr_t(0x9061c),uintptr_t(0x321ac0)}){Fixture f;f.bind();fake_caller=image+rva;
        int value=-777;need(invoke_pod(shared,97,&value)==0&&value==12,"original selected result preserved");
        need(callbacks==1&&originals==1&&trace_begins==1&&trace_finishes==1&&shared->words[0]==11&&local.words[0]==12,"private selection does not advance shared source");
        need(before_source==&local&&before_word==11&&after_word==12&&pin_observations&&fields_ok&&!f.plugin.native_pins,"actual final source and lease surround trace");}
    {Fixture f;f.bind();mode=Mode::Pass;int value=-777;need(invoke_pod(shared,97,&value)==0&&value==12&&shared->words[0]==12&&local.words[0]==11,"UNAVAILABLE alone is unchanged passthrough");need(callbacks==1&&originals==1&&pin_observations&&!f.plugin.native_pins,"passthrough still releases callback lease");}}
void guards(){
    {Fixture f;f.bind();fake_caller=image+0x9061d;expected_pin=false;int value=0;need(!invoke_pod(shared,97,&value)&&callbacks==0&&originals==1&&pin_observations,"other caller passes without selector");}
    {Fixture f;f.bind();expected_pin=false;*reinterpret_cast<uintptr_t*>(image+0x164b250)=0;int value=0;need(!invoke_pod(&unrelated_random,97,&value)&&callbacks==0&&originals==1,"other receiver passes without reading shared slot");}
    {Fixture f;f.bind();expected_pin=false;int value=0;DWORD exception=0;std::thread worker([&]{exception=invoke_pod(shared,97,&value);});worker.join();need(!exception&&callbacks==0&&originals==1&&pin_observations,"other thread passes unchanged");}
    {Fixture f;f.bind();existing_source=&unrelated_random;expected_pin=false;int value=0;need(!invoke_pod(shared,97,&value)&&callbacks==0&&before_source==&unrelated_random&&existing_count.completed==1,"existing Framework selection precedes preview override");}}
void failures(){
    inaccessible=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_NOACCESS);need(inaccessible!=nullptr,"allocate owned inaccessible source fixture");
    for(auto choice:{Mode::Fail,Mode::CppFault,Mode::SehFault,Mode::BadSize,Mode::BadBytes,Mode::BadPointer,Mode::Shared,Mode::Slot,Mode::NoAccess,Mode::Recursive}){
        Fixture f;f.bind();mode=choice;int value=-777;
        need(invoke_pod(shared,97,&value)==MT2FW_MT2_CHARACTER_PREVIEW_ABORT&&value==-777,"selector failure raises fixed abort without fabricated result");
        need(callbacks==1&&!originals&&!trace_begins&&!trace_finishes&&shared->words[0]==11&&local.words[0]==11&&!f.plugin.native_pins,"abort precedes native RNG and releases lease");
        need(!f.runtime.logs().empty()&&f.runtime.logs().back().level=="error","abort retains visible cause");
        const auto cause=f.runtime.logs().back().message;
        need(invoke_pod(shared,97,&value)==MT2FW_MT2_CHARACTER_PREVIEW_ABORT&&callbacks==1&&!originals&&!f.plugin.native_pins,"failed registration never retries selector or shared original");
        need(f.runtime.logs().back().message==cause,"first failure cause remains intact");
    }
    need(VirtualFree(inaccessible,0,MEM_RELEASE)!=0,"release owned inaccessible source fixture");inaccessible=nullptr;
    {Fixture f;f.bind();*reinterpret_cast<uintptr_t*>(image+0x164b250)=0;int value=-777;need(invoke_pod(shared,97,&value)==MT2FW_MT2_CHARACTER_PREVIEW_ABORT&&!callbacks&&!originals&&!f.plugin.native_pins,"changed Default slot aborts before callback or original");}
}
void unwinds(){
    {Fixture f;f.bind();original_seh=true;int value=-777;need(invoke_pod(shared,97,&value)==original_fault&&value==-777,"original SEH remains original");need(originals==1&&trace_begins==1&&!trace_finishes&&!f.plugin.native_pins,"original SEH releases owner pin");}
    {Fixture f;f.bind();original_cpp=true;bool caught=false;try{get_int(shared,97);}catch(const std::runtime_error& e){caught=std::string(e.what())=="original C++ failure";}need(caught&&originals==1&&!trace_finishes&&!f.plugin.native_pins,"original C++ exception and cleanup preserved");}
    {Fixture f;f.bind();trace_seh=true;int value=-777;need(invoke_pod(shared,97,&value)==trace_fault&&value==-777&&originals==1&&trace_finishes==1&&!f.plugin.native_pins,"trace SEH releases lease without replaying original");}
}
void lifetime(){
    {Fixture f;f.bind();mode=Mode::Unbind;int value=0;need(!invoke_pod(shared,97,&value)&&unbind_result==MT2FW_MT2_BUSY&&pin_observations&&!f.plugin.native_pins,"unbind disables new calls while retaining current leased source");
        need(api.character_preview_int_unbind(api.v1.context,token)==MT2FW_MT2_OK,"completed lease permits final unregister");mode=Mode::Select;f.bind();}
    {Fixture f;f.bind();mode=Mode::Retire;f.plugin.native_loaded=true;f.plugin.native_shutdown=shutdown_plugin;int value=0;
        need(!invoke_pod(shared,97,&value)&&value==12&&pin_observations&&shutdowns==1&&!f.plugin.native_pins&&!f.plugin.native_shutdown_pending,"retired plugin shuts down only after original and trace");
        MT2FW_MT2_ServiceV2 stale{};need(native_services->query(&f.plugin.host,2,&stale.v1,sizeof(stale))==MT2FW_MT2_UNAVAILABLE,"retired owner cannot register again");}
}
int main()try{basic_api();selections();guards();failures();unwinds();lifetime();std::cout<<checks<<" actual Character preview owner, dispatch, failure and SEH lifetime checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<"Character preview failure after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}
