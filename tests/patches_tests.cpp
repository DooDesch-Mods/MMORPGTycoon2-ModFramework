#include "mt2fw/core.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

using namespace mt2fw;
namespace {
int checks = 0;
void check(bool condition,const char* cause){++checks;if(!condition)throw std::runtime_error(cause);}
template<class Function>std::string rejects(Function function){try{function();}catch(const std::exception& error){++checks;return error.what();}throw std::runtime_error("Expected rejection");}
PatchSpec spec(std::string id,std::string target="game.update",int priority=0){PatchSpec value;value.id=std::move(id);value.target=std::move(target);value.priority=priority;return value;}
PatchCallbacks observe(std::vector<std::string>& calls,const std::string& id){return {
    [&calls,id](const PatchCall& call)->Json{check(call.data.at("phase")=="before","before phase");calls.push_back(id);return nullptr;},
    [&calls,id](const PatchCall& call){check(call.data.at("phase")=="after","after phase");calls.push_back("-"+id);}
};}
void write(const fs::path& path,const std::string& value){fs::create_directories(path.parent_path());std::ofstream stream(path);stream<<value;if(!stream)throw std::runtime_error("Could not write fixture");}
Json manifest(const std::string& id){return {{"id",id},{"name",id},{"version","1.0.0"},{"api_version","1.0.0"},{"type","lua"},{"entry","main.lua"},{"capabilities",Json::array({"world.read","time.write","objects.write"})},{"multiplayer","shared"}};}
void mod(const fs::path& root,const std::string& id,const std::string& lua){atomic_json(root/"mods"/id/"mod.json",manifest(id));write(root/"mods"/id/"main.lua",lua);}
}
int main(){try{
    std::vector<std::string> errors,calls;int a=0,b=0,c=0;
    PatchRegistry registry([&](const std::string& owner,const std::string& cause){errors.push_back(owner+" "+cause);});
    {
        PatchRegistry presence;
        check(!presence.has_subscribers("game.update"),"empty target has no published subscribers");
        presence.add(&a,"alpha",spec("present"),observe(calls,"present"));
        check(presence.has_subscribers("game.update")&&!presence.has_subscribers("objects.create"),"subscriber presence is target specific");
        auto rejected=spec("rejected","unknown.target");
        rejects([&]{presence.replace(&a,"alpha",{{rejected,observe(calls,"rejected")}});});
        check(presence.has_subscribers("game.update")&&!presence.has_subscribers("unknown.target"),"failed replacement preserves published subscriber presence");
        check(presence.remove(&a,"present")&&!presence.has_subscribers("game.update"),"removing final subscriber closes empty target fast path");
    }
    rejects([]{PatchSpec::parse({{"id","overflow"},{"target","game.update"},{"priority",uint64_t(2147483648)}});});
    rejects([]{PatchSpec::parse({{"id","underflow"},{"target","game.update"},{"priority",int64_t(-2147483649)}});});
    check(PatchSpec::parse({{"id","valid"},{"target","game.update"},{"priority",2147483647}}).priority==2147483647,"maximum supported priority remains exact");
    auto first=spec("first","game.update",-5);first.before={"beta:second"};
    registry.add(&a,"alpha",first,observe(calls,"a"));
    check(registry.status().at("patches")[0].at("unresolved")==Json::array({"beta:second"}),"missing reference stays explicit");
    registry.add(&b,"beta",spec("second","game.update",100),observe(calls,"b"));
    auto run=registry.begin("game.update",{{"dt",0.1}});calls.push_back("original");registry.end(run,{{"original_returned",true}});
    check(calls==std::vector<std::string>({"a","b","original","-b","-a"}),"constraint wins over priority and after reverses nesting");
    check(registry.status().at("patches")[0].at("unresolved").empty(),"missing reference resolves");
    auto generation=registry.status().at("generation");auto cycle=spec("third");cycle.after={"beta:second"};cycle.before={"alpha:first"};
    auto cause=rejects([&]{registry.add(&c,"gamma",cycle,observe(calls,"c"));});
    check(cause.find("alpha:first -> beta:second -> gamma:third -> alpha:first")!=std::string::npos,"closed cycle path is actionable");
    check(registry.status().at("generation")==generation&&registry.status().at("patches").size()==2,"failed cycle keeps generation");
    rejects([&]{registry.add(&a,"alpha",first,observe(calls,"duplicate"));});
    rejects([&]{registry.remove(&c,"first");});
    auto cross=spec("cross","objects.create");cross.before={"alpha:first"};rejects([&]{registry.add(&c,"gamma",cross,observe(calls,"cross"));});
    std::vector<int> permutation={0,1,2};
    do{
        PatchRegistry ordered;std::vector<std::string> output;
        void* owners[]={&a,&b,&c};std::string names[]={"a.a","a","z"};
        for(int index:permutation)ordered.add(owners[index],names[index],spec("x"),observe(output,names[index]));
        auto sample=ordered.begin("game.update",Json::object());ordered.end(sample);
        check(output==std::vector<std::string>({"a","a.a","z","-z","-a.a","-a"}),"owner ordinal order independent of insertion");
    }while(std::next_permutation(permutation.begin(),permutation.end()));
    PatchRegistry writer;auto policy=spec("policy","api.time.set");policy.mode="modify";
    writer.add(&a,"alpha",policy,{[](const PatchCall&)->Json{return {{"speed",0},{"paused",true}};},{}});
    auto second_policy=spec("policy","api.time.set");second_policy.mode="modify";
    check(rejects([&]{writer.add(&b,"beta",second_policy,{[](const PatchCall&)->Json{return nullptr;},{}});}).find("alpha:policy")!=std::string::npos,"writer conflict identifies owner");
    auto denied=writer.begin("api.time.set",{{"paused",false},{"speed",1}});check(denied.failed,"unknown authority fails writer");writer.end(denied);
    auto permitted=writer.begin("api.time.set",{{"paused",false},{"speed",1}},nullptr,true);check(!permitted.failed&&permitted.data.at("speed")==0&&permitted.data.at("paused")==true,"typed writer preserves supported zero speed");writer.end(permitted);
    rejects([&]{auto invalid=spec("mutable");invalid.mode="modify";writer.add(&b,"beta",invalid,{[](const PatchCall&)->Json{return nullptr;},{}});});
    calls.clear();registry.retire(&a);registry.retire(&b);
    registry.add(&a,"alpha",spec("leased"),{
        [&](const PatchCall&)->Json{check(!registry.retire(&a),"retirement reports active lease");calls.push_back("before");return nullptr;},
        [&](const PatchCall&){calls.push_back("after");}
    });
    auto leased=registry.begin("game.update",Json::object());check(registry.busy(&a),"lease exists until paired after");registry.end(leased);
    check(!registry.busy(&a)&&calls==std::vector<std::string>({"before","after"}),"retired pair closes before state release");
    auto empty=registry.begin("game.update",Json::object());registry.end(empty);check(calls.size()==2,"retired owner receives no new callbacks");
    registry.add(&a,"alpha",spec("throws"),{[](const PatchCall&)->Json{throw std::runtime_error("retained cause");},[&](const PatchCall&){calls.push_back("bad after");}});
    registry.add(&b,"beta",spec("healthy"),observe(calls,"healthy"));
    auto failing=registry.begin("game.update",Json::object());int originals=0;++originals;registry.end(failing,{{"original_returned",true}});
    check(originals==1&&calls.back()=="-healthy","observer failure does not suppress original or healthy after");
    check(errors.back().find("retained cause")!=std::string::npos,"observer original cause retained");
    registry.retire(&a);registry.retire(&b);bool nested=false;std::vector<std::string> identities;
    registry.add(&a,"alpha",spec("nested"),{
        [&](const PatchCall& call)->Json{identities.push_back(call.data.at("invocation"));if(!nested){nested=true;auto inner=registry.begin("game.update",Json::object());registry.end(inner);}return nullptr;},{}
    });
    auto outer=registry.begin("game.update",Json::object());registry.end(outer);check(identities.size()==2&&identities[0]!=identities[1],"nested identities independent");
    registry.retire(&a);
    std::atomic<bool> entered=false,release=false;
    registry.add(&a,"alpha",spec("worker","objects.create"),{[&](const PatchCall&)->Json{entered=true;while(!release.load())std::this_thread::yield();return nullptr;},{}});
    std::thread worker([&]{auto work=registry.begin("objects.create",Json::object());registry.end(work);});
    while(!entered.load())std::this_thread::yield();check(!registry.retire(&a),"concurrent callback protects owner state");check(!registry.has_subscribers("objects.create"),"retired in-flight subscriber is absent from published target");release=true;worker.join();check(!registry.busy(&a),"concurrent retirement confirms quiescence");

    auto root=fs::current_path()/"evidence"/("patches-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    mod(root,"test.alpha",R"lua(
mt2.patches.add({id='observe',target='game.update',before={'test.beta:observe'}},{
 before=function(call) assert(type(call.invocation)=='string'); mt2.storage.set('seen',41); call.dt=999 end,
 after=function(call) assert(call.dt==0.25); mt2.storage.set('after',42) end
})
)lua");
    mod(root,"test.beta",R"lua(
local removed,remove_error=pcall(mt2.patches.remove,'missing')
assert(not removed and remove_error:find('PATCH_NOT_FOUND'))
local added,add_error=pcall(mt2.patches.add,{id='invalid',target='game.update'},{before=42})
assert(not added and add_error:find('PATCH_CALLBACK'))
mt2.patches.add({id='observe',target='game.update'},{before=function(call) assert(call.dt==0.25) end})
)lua");
    int adapter_calls=0;Runtime runtime(root,[&](const std::string& op,const Json& args)->Json{++adapter_calls;return args;});runtime.load();
    check(runtime.plugins().size()==2&&runtime.plugins()[0]->active&&runtime.plugins()[1]->active,"Lua patch mods load");
    auto lua=runtime.patches.begin("game.update",{{"dt",0.25},{"lua_allowed",true}});runtime.patches.end(lua,{{"original_returned",true}});
    check(runtime.plugins()[0]->profile_data.at("seen")==41&&runtime.plugins()[0]->profile_data.at("after")==42,"Lua callbacks execute with isolated copied payloads");
    auto old_generation=runtime.patches.status().at("generation");
    write(root/"mods/test.alpha/main.lua",R"lua(mt2.patches.add({id='observe',target='game.update',after={'test.beta:observe'},before={'test.beta:observe'}},{before=function() end}))lua");
    check(!runtime.reload("test.alpha")&&runtime.plugins()[0]->active,"cycle reload preserves active instance");
    check(runtime.patches.status().at("generation")==old_generation,"cycle reload preserves graph generation");
    write(root/"mods/test.alpha/main.lua",R"lua(mt2.patches.add({id='replacement',target='game.update'},{before=function() mt2.storage.set('replacement',true) end}); mt2.events.on('world_ready',function() error('candidate world callback failed') end))lua");
    runtime.world_ready("world");check(!runtime.reload("test.alpha"),"candidate lifecycle callback rejection retains old graph");
    check(runtime.patches.status().at("generation")==old_generation,"activation callback failure does not publish candidate");
    auto& plugin=*runtime.plugins()[0];plugin.staging=false;
    check(!runtime.patch_status().at("authority").at("api.time.set").at("allowed"),"authority defaults unknown");
    rejects([&]{runtime.request(plugin,"time.set",{{"paused",false},{"speed",1}});});check(adapter_calls==0,"unknown authority never invokes native adapter");
    runtime.set_patch_authority([](const std::string&,std::string&){return true;});
    int policy_owner=0;runtime.patches.add(&policy_owner,"test.policy",policy,{[](const PatchCall&)->Json{return {{"speed",0.5}};},{}});
    auto clock=runtime.request(plugin,"time.set",{{"paused",false},{"speed",1}});check(adapter_calls==1&&clock.at("speed")==0.5,"acknowledged authority executes one typed modified request");
    runtime.patches.retire(&policy_owner);int fence_owner=0;
    runtime.patches.add(&fence_owner,"test.fence",spec("write"),{[&](const PatchCall&)->Json{runtime.request(plugin,"time.set",{{"paused",false},{"speed",1}});return nullptr;},{}});
    auto forbidden=runtime.patches.begin("game.update",{{"lua_allowed",true},{"dt",0.25}});runtime.patches.end(forbidden);
    check(adapter_calls==1,"observer cannot bypass mode through Runtime write API");
    runtime.patches.retire(&fence_owner);
    runtime.patches.add(&fence_owner,"test.fence",policy,{[&](const PatchCall&)->Json{runtime.request(plugin,"time.set",{{"paused",false},{"speed",1}});return nullptr;},{}});
    check(rejects([&]{runtime.request(plugin,"time.set",{{"paused",false},{"speed",1}});}).find("writer failed")!=std::string::npos&&adapter_calls==1,"modifier cannot recursively execute a shared-world write");
    runtime.patches.retire(&fence_owner);
    int guard_reads=0;runtime.set_patch_authority([&](const std::string&,std::string& reason){if(++guard_reads==1)return true;reason="role changed";return false;});
    check(rejects([&]{runtime.request(plugin,"time.set",{{"paused",false},{"speed",1}});})=="role changed"&&adapter_calls==1,"authority rechecked immediately before execution");
    write(root/"mods/test.alpha/main.lua",R"lua(
mt2.patches.add({id='throw',target='game.update',priority=10},{before=function() error('Lua patch original cause') end})
mt2.patches.add({id='later',target='game.update'},{before=function() mt2.storage.set('unexpected_before',true) end,after=function() mt2.storage.set('unexpected_after',true) end})
)lua");
    check(runtime.reload("test.alpha"),"valid Lua replacement publishes atomically");
    auto broken=runtime.patches.begin("game.update",{{"lua_allowed",true},{"dt",0.25}});int native_calls=0;++native_calls;runtime.patches.end(broken,{{"original_returned",true}});
    check(native_calls==1&&!runtime.plugins()[0]->active&&runtime.plugins()[1]->active,"Lua patch error disables only its owner and preserves original");
    check(runtime.plugins()[0]->error.find("Lua patch original cause")!=std::string::npos,"Lua error attribution retains original cause");
    check(!runtime.plugins()[0]->profile_data.contains("unexpected_before")&&!runtime.plugins()[0]->profile_data.contains("unexpected_after"),"disabled Lua owner stops remaining snapshot callbacks");
    runtime.stop_plugins();
    std::cout<<"Patch checks passed: "<<checks<<"\n";return 0;
}catch(const std::exception& error){std::cerr<<"Patch check failed: "<<error.what()<<"\n";return 1;}}
