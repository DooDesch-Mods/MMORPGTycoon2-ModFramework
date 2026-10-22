#include "mt2fw/core.h"
#include <fstream>
#include <iostream>
#include <chrono>

using namespace mt2fw;
int main(){try{
    size_t checks=0;auto require=[&](bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);};
    auto root=fs::current_path()/"evidence"/("gameplay-lua-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    auto directory=root/"mods/test.gameplay";fs::create_directories(directory);
    atomic_json(directory/"mod.json",{{"id","test.gameplay"},{"name","Gameplay fixture"},{"version","1.0.0"},{"api_version","1.2.0"},
        {"type","lua"},{"entry","main.lua"},{"capabilities",{"world.read","objects.write"}},{"multiplayer","singleplayer"}});
    {std::ofstream file(directory/"main.lua");file<<R"lua(
local ran = false
mt2.events.on('update',function()
    if ran then return end
    ran = true
    assert(#mt2.experimental.operations() == 1)
    assert(mt2.characters.list(false)[1].handle.uid == '9007199254740993')
    assert(mt2.management.fees().subscription == 12)
    local h = {epoch=1,uid='9007199254740993'}
    assert(mt2.characters.rename(h, 'My class').status == 'accepted')
    assert(mt2.dungeons.place_gizmo(h, 'Door', 'door', {0,0,0}).status == 'accepted')
    assert(mt2.network.place_pipe(h, {epoch=1,uid='9007199254740995'}).status == 'accepted')
    local shape = {{{x=9007199254740993,z=0},{x=9007199254740995,z=0},{x=9007199254740993,z=2}}}
    assert(mt2.terrain.paint({epoch=1,id=2}, 1, {{position={1,2,3},radius=2,shape=shape}}).status == 'accepted')
end)
mt2.events.on('render',function()
    local ok,cause=pcall(function() mt2.characters.list() end)
    assert(not ok and cause:find('game%-thread callbacks'))
end)
)lua";if(!file)throw std::runtime_error("Cannot write gameplay Lua fixture");}
    int queries=0,actions=0;uint64_t observed_instance=0;
    Runtime runtime(root,[&](const std::string& op,const Json& args)->Json{
        require(args.at("mod_id")=="test.gameplay","Lua owner is forged");
        auto instance=args.at("mod_instance").get<uint64_t>();require(instance>0,"Mod instance missing");
        if(observed_instance)require(instance==observed_instance,"Mod instance changed inside one lifetime");observed_instance=instance;
        if(op=="gameplay.operations")return Json::array({{{"name","characters.list"}}});
        if(op=="gameplay.query"){
            ++queries;auto operation=args.at("operation").get<std::string>();
            if(operation=="characters.list"){
                require(args.at("arguments").at("include_deleted")==false,"Explicit false became a default");
                return Json::array({{{"handle",{{"epoch",1},{"uid","9007199254740993"}}}}});
            }
            if(operation=="management.fees")return {{"subscription",12}};
        }
        if(op=="gameplay.action"){
            ++actions;const auto& value=args.at("arguments");
            if(args.at("operation")=="characters.rename")require(value.at("name")=="My class"&&value.at("handle").at("uid")=="9007199254740993","Character action lost exact data");
            else if(args.at("operation")=="dungeons.place_gizmo")require(value.at("rotation")==Json::array({0,0,0,1})&&value.at("scale")==Json::array({1,1,1}),"Lua default vectors are not real tables");
            else if(args.at("operation")=="network.place_pipe")require(value.at("end").at("uid")=="9007199254740995"&&!value.contains("position"),"Lua keyword endpoint or omitted optional argument was corrupted");
            else if(args.at("operation")=="terrain.paint")require(value.at("stamps")[0].at("shape")[0][0].at("x").is_number_integer()&&value.at("stamps")[0].at("shape")[0][0].at("x").get<int64_t>()==9007199254740993ll&&value.at("label")=="Paint terrain","Lua terrain geometry lost integer precision or its label default");
            else throw std::runtime_error("Unexpected gameplay action");
            return {{"id","fixture-"+std::to_string(actions)},{"status","accepted"}};
        }
        throw std::runtime_error("Unexpected gameplay fixture request: "+op);
    });
    runtime.set_patch_authority([](const std::string& op,std::string& cause){if(op=="api.gameplay.action")return true;cause="Fixture operation denied";return false;});
    runtime.load();runtime.world_ready("Fixture");runtime.emit("update");runtime.rendering=true;runtime.emit("render");runtime.rendering=false;
    require(runtime.plugins().size()==1&&runtime.plugins()[0]->active,"Real Lua gameplay callbacks failed");
    require(queries==2&&actions==4,"Gameplay bridge executed missing or duplicate operations");
    auto& plugin=*runtime.plugins()[0];runtime.multiplayer=true;bool denied=false;
    try{runtime.request(plugin,"gameplay.action",{{"operation","characters.rename"},{"arguments",Json::object()}});}catch(const std::exception& error){denied=std::string(error.what()).find("Singleplayer mod")!=std::string::npos;}
    require(denied&&actions==4,"Lua mutation bypassed multiplayer authority");
    std::cout<<checks<<" real Lua gameplay checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
