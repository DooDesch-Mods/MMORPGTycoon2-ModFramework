#include "mt2fw/core.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#define CHECK(expression) do { if(!(expression))throw std::runtime_error("Check failed: " #expression); } while(false)
using namespace mt2fw;
static int checks=0;
template<class F> void rejects(F fn){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}CHECK(rejected);++checks;}
static void write(const fs::path& file,const std::string& text){fs::create_directories(file.parent_path());std::ofstream stream(file);stream<<text;CHECK(bool(stream));}
static Json manifest(const std::string& id){return {{"id",id},{"name",id},{"version","1.0.0"},{"api_version","1.0.0"},{"type","lua"},{"entry","main.lua"},{"capabilities",Json::array({"ui","world.read"})},{"multiplayer","local"}};}
static void mod(const fs::path& root,const std::string& id,const std::string& script,Json m=Json()) {atomic_json(root/"mods"/id/"mod.json",m.is_null()?manifest(id):m);write(root/"mods"/id/"main.lua",script);}
int main(){
    auto root=fs::current_path()/"evidence"/("mt2fw-core-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        CHECK(version_matches("1.2.0","^1.0.0"));CHECK(!version_matches("2.0.0","^1.0.0"));CHECK(!version_matches("0.2.0","^0.1.0"));checks+=3;
        CHECK(version_matches("1.0.0-beta.11+test",">=1.0.0-beta.2"));CHECK(version_matches("1.0.0+build","1.0.0+other"));
        CHECK(!version_matches("1.0.0-rc.1",">=1.0.0"));checks+=3;
        rejects([&]{version_matches("1.0.0-01","1.0.0");});
        rejects([&]{Manifest::parse(manifest("invalid"));});
        auto unsafe=manifest("test.unsafe");unsafe["capabilities"].push_back("objects.write");rejects([&]{Manifest::parse(unsafe);});
        unsafe=manifest("test.unsafe");unsafe["entry"]="../escape.lua";rejects([&]{Manifest::parse(unsafe);});
        rejects([&]{confined(root,"../outside");});
        mod(root,"test.first","local count=0; mt2.events.on('update',function() count=count+1; mt2.log.info('first:'..count) end)");
        auto second=manifest("test.second");second["dependencies"]={{"test.first","^1.0.0"}};
        mod(root,"test.second","mt2.events.on('update',function() mt2.log.info('second') end)",second);
        Runtime runtime(root);
        std::vector<std::string> notices;
        runtime.mod_notice=[&](const Manifest& value,const char* state){notices.push_back(std::string(state)+" "+value.id+" "+value.version);};
        runtime.load();
        CHECK((notices==std::vector<std::string>{"Loading test.first 1.0.0","Active test.first 1.0.0","Loading test.second 1.0.0","Active test.second 1.0.0"}));++checks;
        runtime.emit("update");CHECK(runtime.plugins().size()==2);CHECK(runtime.plugins()[0]->active&&runtime.plugins()[1]->active);checks+=2;
        write(root/"mods/test.first/main.lua","this is invalid Lua!");CHECK(!runtime.reload("test.first"));runtime.emit("update");CHECK(runtime.plugins()[0]->active);checks+=2;
        write(root/"mods/test.first/main.lua","mt2.events.on('update',function() mt2.log.info('replacement') end)");CHECK(runtime.reload("test.first"));runtime.emit("update");CHECK(runtime.plugins()[1]->active);checks+=2;
        runtime.world_ready("reload-world");
        write(root/"mods/test.first/main.lua","mt2.events.on('world_ready',function() error('replacement world initialization failed') end)");
        CHECK(!runtime.reload("test.first"));CHECK(runtime.plugins()[0]->active&&runtime.plugins()[1]->active);checks+=2;
        write(root/"mods/test.first/main.lua","mt2.events.on('update',function() error('retained original cause') end)");CHECK(runtime.reload("test.first"));runtime.emit("update");CHECK(!runtime.plugins()[0]->active);CHECK(!runtime.plugins()[1]->active);CHECK(runtime.plugins()[0]->error.find("retained original cause")!=std::string::npos);checks+=4;
        auto isolated=root/"isolated";
        mod(isolated,"test.loop","mt2.events.on('update',function() while true do end end)");
        mod(isolated,"test.healthy","mt2.events.on('update',function() mt2.log.info('healthy') end)");
        Runtime isolated_runtime(isolated);isolated_runtime.load();isolated_runtime.emit("update");
        CHECK(isolated_runtime.plugins()[0]->active);CHECK(!isolated_runtime.plugins()[1]->active);CHECK(isolated_runtime.plugins()[1]->error.find("instruction budget")!=std::string::npos);checks+=3;
        auto restricted=root/"restricted";
        mod(restricted,"test.sandbox","assert(io==nil and os==nil and package==nil and debug==nil and load==nil); mt2.events.on('start',function() mt2.config.define('enabled',{type='boolean',default=true}); mt2.storage.set('counter',42) end)");
        Runtime sandbox(restricted);sandbox.load();CHECK(sandbox.plugins()[0]->active);CHECK(read_json(restricted/"data/test.sandbox/profile.json")["counter"]==42);checks+=2;
        auto cycle=root/"cycle";
        auto a=manifest("test.a"),b=manifest("test.b");a["dependencies"]={{"test.b","1.0.0"}};b["dependencies"]={{"test.a","1.0.0"}};
        mod(cycle,"test.a","",a);mod(cycle,"test.b","",b);Runtime cyclic(cycle);cyclic.load();CHECK(!cyclic.plugins()[0]->active&&!cyclic.plugins()[1]->active);++checks;
        sandbox.world_ready("one");CHECK(sandbox.epoch==1);
        sandbox.object_bindings={{"reference",{{"owner","test.sandbox"},{"epoch",1},{"uid","123"},{"location",{{"region",2},{"child",0}}}}}};
        sandbox.request(*sandbox.plugins()[0],"storage.set",{{"key","value"},{"value",1},{"scope","world"}});
        sandbox.saved("save-a");
        sandbox.request(*sandbox.plugins()[0],"storage.set",{{"key","value"},{"value",2},{"scope","world"}});
        sandbox.saved("save-b");sandbox.world_leave();CHECK(sandbox.epoch==2);checks+=2;
        sandbox.check_save("save-a");sandbox.world_ready("one");
        CHECK(sandbox.plugins()[0]->world_data["value"]==1);++checks;
        CHECK(sandbox.object_bindings["reference"]["epoch"]==0);++checks;
        sandbox.world_leave();sandbox.check_save("save-b");sandbox.world_ready("one");
        CHECK(sandbox.plugins()[0]->world_data["value"]==2);++checks;
        CHECK(sandbox.multiplayer_conflicts(Json::array()).empty());++checks;
        auto exhausted=root/"memory";mod(exhausted,"test.memory","mt2.events.on('update',function() local rows={} for i=1,100 do rows[i]=string.rep('x',8*1024*1024)..i end end)");
        mod(exhausted,"test.survivor","mt2.events.on('update',function() mt2.log.info('survived allocation failure') end)");
        Runtime memory(exhausted);memory.load();memory.emit("update");CHECK(!memory.plugins()[0]->active);CHECK(memory.plugins()[1]->active);checks+=2;
        auto protected_loop=root/"protected-loop";mod(protected_loop,"test.loop","mt2.events.on('update',function() while true do pcall(function() while true do end end) end end)");
        Runtime protected_runtime(protected_loop);protected_runtime.load();protected_runtime.emit("update");CHECK(!protected_runtime.plugins()[0]->active);++checks;
        int executions=0;Runtime abi(root/"abi",[&](const std::string&,const Json&){++executions;return Json(true);});Plugin client(&abi);
        client.manifest=Manifest::parse(manifest("test.abi"));client.active=true;client.staging=false;
        auto capacity=client.host.request(client.host.context,"world.status","{}",nullptr,0);CHECK(capacity>0&&executions==0);
        std::vector<char> buffer(capacity);client.host.request(client.host.context,"world.status","{}",buffer.data(),capacity-1);CHECK(executions==0);
        client.host.request(client.host.context,"world.status","{}",buffer.data(),capacity);CHECK(executions==1&&Json::parse(buffer.data())["ok"]==true);checks+=3;
        auto content_root=root/"scripted-content";auto content_manifest=manifest("test.content");content_manifest["multiplayer"]="singleplayer";content_manifest["capabilities"]=Json::array({"content"});
        mod(content_root,"test.content","mt2.content.register({{id='test.content:pyramid',variant='test',model='assets/model.vmb',material='assets/model.mat'}})",content_manifest);
        write(content_root/"mods/test.content/assets/model.vmb","fixture");write(content_root/"mods/test.content/assets/model.mat","fixture");
        int registrations=0;Runtime scripted_content(content_root,[&](const std::string& op,const Json&){CHECK(op=="content.register");++registrations;return Json(true);});
        scripted_content.load();CHECK(registrations==1&&scripted_content.plugins()[0]->active&&scripted_content.plugins()[0]->content_registered);CHECK(!scripted_content.reload("test.content"));checks+=2;
        auto inputs=root/"input-owners";mod(inputs,"test.a","mt2.events.on('input',function() mt2.log.info('a input') end)");mod(inputs,"test.b","mt2.events.on('input',function() mt2.log.info('b input') end)");
        Runtime input_runtime(inputs);input_runtime.load();input_runtime.emit("input",{{"owner","test.a"},{"name","example"}});
        CHECK(input_runtime.plugins()[0]->calls==1&&input_runtime.plugins()[1]->calls==0);++checks;
        auto unicode_root=root/fs::u8path("Grüße");mod(unicode_root,"test.unicode","mt2.events.on('update',function() mt2.log.info('Grüße') end)");
        Runtime unicode(unicode_root);unicode.load();write(unicode_root/"mods/test.unicode/main.lua","this is invalid Lua!");
        CHECK(!unicode.reload("test.unicode"));CHECK(unicode.plugins()[0]->active);++checks;
        std::string forwarded_owner;
        Runtime marker_runtime(root/"markers",[&](const std::string& operation,const Json& args){CHECK(operation=="ui.world_marker"||operation=="ui.world_project");forwarded_owner=args.at("mod_id");return Json(true);});
        Plugin marker_owner(&marker_runtime);auto marker_manifest=manifest("test.markers");marker_manifest["type"]="native";marker_manifest["entry"]="plugin.dll";
        marker_owner.manifest=Manifest::parse(marker_manifest);marker_owner.active=true;marker_owner.staging=false;marker_owner.native_loaded=true;
        marker_runtime.request(marker_owner,"ui.world_marker",{{"mod_id","spoof.other"}});CHECK(forwarded_owner=="test.markers");++checks;
        marker_runtime.request(marker_owner,"ui.world_project",Json::object());++checks;
        rejects([&]{marker_runtime.request(marker_owner,"ui.text",{{"text","not a render callback"}});});
        marker_owner.manifest.capabilities.erase("world.read");rejects([&]{marker_runtime.request(marker_owner,"ui.world_marker",Json::object());});
        marker_owner.manifest.capabilities.insert("world.read");marker_owner.manifest.capabilities.erase("ui");rejects([&]{marker_runtime.request(marker_owner,"ui.world_marker",Json::object());});
        marker_owner.manifest.capabilities.insert("ui");marker_owner.native_loaded=false;rejects([&]{marker_runtime.request(marker_owner,"ui.world_marker",Json::object());});
        marker_owner.native_loaded=true;marker_owner.active=false;rejects([&]{marker_runtime.request(marker_owner,"ui.world_marker",Json::object());});
        std::cout<<"info: "<<checks<<" core assertions passed; evidence="<<root.string()<<"\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"error: "<<error.what()<<" evidence="<<root.string()<<"\n";return 1;}
}
