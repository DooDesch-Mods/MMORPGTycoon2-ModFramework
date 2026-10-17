#include "native_services.h"
#include <iostream>
#include <thread>

using namespace mt2fw;
using namespace mt2fw::mt2;
namespace {
int checks=0,shutdowns=0;
void check(bool condition,const char* reason){++checks;if(!condition)throw std::runtime_error(reason);}
void MT2FW_CALL shutdown_plugin(){++shutdowns;}
struct Calls {std::vector<int>* order;int id;int begin=0,end=0;uint32_t returned=0;};
int MT2FW_CALL begin(void* user,const MT2FW_MT2_Frame* frame){auto& value=*static_cast<Calls*>(user);check(frame->size==sizeof(*frame),"frame size");++value.begin;value.order->push_back(value.id);return 1;}
void MT2FW_CALL end(void* user,const MT2FW_MT2_Frame* frame){auto& value=*static_cast<Calls*>(user);++value.end;value.returned=frame->original_returned;value.order->push_back(-value.id);}
int MT2FW_CALL skip(void*,const MT2FW_MT2_Frame*){return 0;}
struct Nested {std::vector<uint64_t> begins,ends;std::vector<uint32_t> returned;};
int MT2FW_CALL nested_begin(void* user,const MT2FW_MT2_Frame* frame){static_cast<Nested*>(user)->begins.push_back(frame->id);return 1;}
void MT2FW_CALL nested_end(void* user,const MT2FW_MT2_Frame* frame){auto& value=*static_cast<Nested*>(user);value.ends.push_back(frame->id);value.returned.push_back(frame->original_returned);}
}
int main(){try{
    auto root=fs::current_path()/"artifacts/native-services-tests";fs::create_directories(root/"logs");
    Runtime runtime(root);Plugin a(&runtime),b(&runtime);
    a.manifest.id="test.multiplayer";b.manifest.id="test.profiler";
    for(auto value:{&a,&b}){value->manifest.type="native";value->staging=false;value->active=true;value->directory=root/value->manifest.id;}
    NativeServices service(root/"game",root,[&](const MT2FW_Host* host){if(host==&a.host)return &a;if(host==&b.host)return &b;return static_cast<Plugin*>(nullptr);});
    runtime.native_retire=[&](Plugin& value){return service.retire(value);};
    MT2FW_MT2_ServiceV1 api{},profiler{};
    check(service.query(&a.host,1,&api,sizeof(api))==MT2FW_MT2_OK,"query actual host");
    check(service.query(&b.host,1,&profiler,sizeof(profiler))==MT2FW_MT2_OK,"query second host");
    auto copied=a.host;check(service.query(&copied,1,&api,sizeof(api))==MT2FW_MT2_UNAVAILABLE,"reject copied host identity");
    check(service.query(&a.host,2,&api,sizeof(api))==MT2FW_MT2_INVALID,"reject service version");
    check(service.query(&a.host,1,&api,sizeof(api)-1)==MT2FW_MT2_INVALID,"reject undersized service");
    a.staging=true;check(service.query(&a.host,1,&api,sizeof(api))==MT2FW_MT2_UNAVAILABLE,"reject staged plugin");a.staging=false;
    check(std::string(api.game_data_directory)==path_text(root/"game-data"),"isolated game data path");
    check(std::string(api.mod_data_directory)==path_text(root/"data/test.multiplayer"),"owned mod data path");
    std::vector<int> order;Calls first{&order,1},second{&order,2};
    auto one=api.frame_subscribe(api.context,begin,end,&first),two=profiler.frame_subscribe(profiler.context,begin,end,&second);
    check(one&&two&&one!=two,"owned pair tokens");
    check(api.frame_unsubscribe(api.context,two)==MT2FW_MT2_INVALID,"reject foreign pair token");
    service.begin_frame(nullptr,0.016f);order.push_back(9);service.end_frame(true);
    check(order==std::vector<int>({1,2,9,-2,-1}),"ordered pairs surround original once");
    check(first.returned==1&&second.returned==1,"original returned marker");
    check(api.frame_unsubscribe(api.context,one)==MT2FW_MT2_OK,"unsubscribe owner");
    check(profiler.frame_unsubscribe(profiler.context,two)==MT2FW_MT2_OK,"unsubscribe second owner");
    Nested nested;auto nested_token=api.frame_subscribe(api.context,nested_begin,nested_end,&nested);
    service.begin_frame(nullptr,1);service.begin_frame(nullptr,2);service.end_frame(false);
    check(service.in_frame()&&a.native_pins==1,"nested end keeps outer frame and callback lease alive");
    service.end_frame(true);check(!service.in_frame()&&!a.native_pins,"outer end releases final native lease");
    check(nested.begins.size()==2&&nested.ends==std::vector<uint64_t>({nested.begins[1],nested.begins[0]})&&nested.returned==std::vector<uint32_t>({0,1}),"nested frame identity and original outcomes remain independent");
    check(api.frame_unsubscribe(api.context,nested_token)==MT2FW_MT2_OK,"nested pair unsubscribe");
    auto skipped=api.frame_subscribe(api.context,skip,end,&first);service.begin_frame(nullptr,0);service.end_frame(false);
    check(first.end==1,"skipped begin has no end");api.frame_unsubscribe(api.context,skipped);
    auto section=api.section_register(api.context,"snapshot.apply");
    check(section&&section==api.section_register(api.context,"snapshot.apply"),"immutable section id");
    check(api.scope_begin(api.context,section).token==0,"inactive capture producer is dormant");
    check(profiler.scope_begin(profiler.context,section).token==0,"reject foreign section");
    MT2FW_MT2_Section definition{sizeof(definition)};
    check(profiler.section_read(profiler.context,section,&definition)==MT2FW_MT2_OK&&std::string(definition.name)=="snapshot.apply"&&std::string(definition.owner)==a.manifest.id,"read immutable names");
    uint64_t epoch=0;
    check(profiler.capture_start(profiler.context,64,&epoch)==MT2FW_MT2_OK&&epoch,"start opt-in capture");
    uint64_t foreign=0;check(api.capture_start(api.context,64,&foreign)==MT2FW_MT2_BUSY,"capture has one owner");
    check(api.capture_stop(api.context)==MT2FW_MT2_UNAVAILABLE,"foreign owner cannot stop capture");
    service.begin_frame(nullptr,0.1f);auto scope=api.scope_begin(api.context,section);api.scope_end(api.context,section,scope);service.end_frame(true);
    MT2FW_MT2_TraceEvent events[64]{};MT2FW_MT2_TraceStatus status{sizeof(status)};
    check(api.capture_read(api.context,events,64,&status)==MT2FW_MT2_UNAVAILABLE,"foreign owner cannot drain capture");
    check(profiler.capture_read(profiler.context,events,64,&status)==MT2FW_MT2_OK&&status.written==4&&!status.dropped,"frame and scope markers");
    check(events[0].kind==MT2FW_MT2_FRAME_BEGIN&&events[1].kind==MT2FW_MT2_SCOPE_BEGIN&&events[2].kind==MT2FW_MT2_SCOPE_END&&events[3].kind==MT2FW_MT2_FRAME_END,"marker order");
    check(events[1].token==events[2].token&&events[1].epoch==epoch&&events[1].thread==events[2].thread&&events[1].frame==events[0].frame,"scope attribution");
    check(events[1].nanoseconds<=events[2].nanoseconds&&!status.thread_cpu_available,"monotonic wall timing does not claim CPU timing");
    for(int i=0;i<80;++i){auto token=api.scope_begin(api.context,section);api.scope_end(api.context,section,token);}
    check(profiler.capture_read(profiler.context,events,64,&status)==MT2FW_MT2_OK&&status.written==64&&status.dropped==96,"overflow remains visible");
    auto old=api.scope_begin(api.context,section);check(profiler.capture_stop(profiler.context)==MT2FW_MT2_OK,"stop capture");
    uint64_t next=0;check(profiler.capture_start(profiler.context,64,&next)==MT2FW_MT2_OK&&next>epoch,"capture epoch advances");
    api.scope_end(api.context,section,old);
    check(profiler.capture_read(profiler.context,events,64,&status)==MT2FW_MT2_OK&&!status.written,"old scope cannot contaminate new epoch");
    std::thread worker([&]{for(int i=0;i<128;++i){auto token=api.scope_begin(api.context,section);api.scope_end(api.context,section,token);}});
    for(int i=0;i<128;++i){auto token=api.scope_begin(api.context,section);api.scope_end(api.context,section,token);}worker.join();
    check(profiler.capture_read(profiler.context,events,64,&status)==MT2FW_MT2_OK&&status.written+status.dropped==512,"bounded concurrent producers account for every event");
    check(profiler.capture_stop(profiler.context)==MT2FW_MT2_OK,"stop concurrent capture");
    int installed=0,removed=0,target=0;bool fail_remove=false;
    service.reserve("game.update");service.target("test.unique",&target);
    service.backend([&](void* address,void*,void** original,std::string&){++installed;*original=address;return true;},[&](void*,std::string& cause){if(fail_remove){cause="synthetic remove failure";return false;}++removed;return true;});
    void* original=nullptr;uint64_t hook=0;
    check(api.hook_install(api.context,"game.update",&target,&original,&hook)==MT2FW_MT2_UNAVAILABLE,"shared target rejected");
    check(api.hook_install(api.context,"missing",&target,&original,&hook)==MT2FW_MT2_UNAVAILABLE,"unknown target rejected");
    check(api.hook_install(api.context,"test.unique",&target,&original,&hook)==MT2FW_MT2_OK&&original==&target&&installed==1,"named hook installed");
    uint64_t other=0;check(profiler.hook_install(profiler.context,"test.unique",&target,&original,&other)==MT2FW_MT2_BUSY,"target uniqueness");
    check(profiler.hook_remove(profiler.context,hook)==MT2FW_MT2_INVALID,"foreign hook ownership rejected");
    fail_remove=true;check(api.hook_remove(api.context,hook)==MT2FW_MT2_FAILED&&service.has_hooks(),"failed removal retains ownership");fail_remove=false;
    check(api.hook_remove(api.context,hook)==MT2FW_MT2_OK&&removed==1&&!service.has_hooks(),"owner removes own hook");
    service.backend([](void*,void*,void**,std::string& cause){cause="synthetic enable failure";return false;},[&](void*,std::string& cause){if(fail_remove){cause="synthetic rollback failure";return false;}++removed;return true;});
    fail_remove=true;
    check(api.hook_install(api.context,"test.unique",&target,&original,&hook)==MT2FW_MT2_FAILED&&service.has_hooks(),"failed enable and rollback retain ownership");
    check(profiler.hook_install(profiler.context,"test.unique",&target,&original,&other)==MT2FW_MT2_BUSY,"rollback failure cannot expose a target to another owner");
    fail_remove=false;check(service.retire(a)&&!service.has_hooks(),"later retirement retries retained hook cleanup");
    Plugin c(&runtime);c.manifest.id="test.frame";c.manifest.type="native";c.active=true;c.staging=false;c.directory=root/"frame";
    NativeServices frames(root/"game",root,[&](const MT2FW_Host* host){return host==&c.host?&c:nullptr;});
    runtime.native_retire=[&](Plugin& value){return &value==&c?frames.retire(value):service.retire(value);};
    check(frames.query(&c.host,1,&api,sizeof(api))==MT2FW_MT2_OK,"fresh native frame owner");
    auto final_pair=api.frame_subscribe(api.context,begin,end,&first);check(final_pair!=0,"final pair registered");
    c.native_loaded=true;c.native_shutdown=shutdown_plugin;
    frames.begin_frame(nullptr,0);c.native_shutdown_pending=true;runtime.disable(c,"synthetic mid-frame retirement");
    check(c.native_pins==1&&!shutdowns,"entered pair pins shutdown");
    frames.end_frame(false);check(first.end==2&&!first.returned&&shutdowns==1&&!c.native_pins,"retired pair ends before deferred shutdown");
    frames.begin_frame(nullptr,0);frames.end_frame(true);check(first.begin==2&&first.end==2,"retired owner receives no later pair");
    check(api.frame_subscribe(api.context,begin,end,&first)==0,"retired interface rejects registrations");
    check(profiler.capture_start(profiler.context,64,&next)==MT2FW_MT2_OK,"other mod remains usable");
    check(service.retire(b),"capture owner retires");
    std::cout<<checks<<" native service checks passed\n";return 0;
}catch(const std::exception& problem){std::cerr<<"Native service failure after "<<checks<<" checks: "<<problem.what()<<"\n";return 1;}}
