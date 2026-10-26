// Runs real Lua registrations against a controlled host; it does not certify native game effects.
#include "mt2fw/core.h"
#include <iostream>
#include <chrono>
using namespace mt2fw;
int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::runtime_error("Expected mod directory and result JSON path");
        fs::path source=fs::u8path(argv[1]);auto manifest=Manifest::parse(read_json(source/"mod.json"));
        auto root=fs::current_path()/"evidence"/("docs-probe-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        auto folder=root/"mods"/manifest.id;fs::create_directories(folder);fs::copy(source,folder,fs::copy_options::recursive);
        Json calls=Json::array();int action=0;
        Json object={{"handle",{{"epoch",1},{"uid","1"}}},{"x",1},{"y",2},{"z",3}};
        Runtime runtime(root,[&](const std::string& op,const Json& args)->Json {
            calls.push_back({{"operation",op},{"arguments",args}});
            if(op=="world.status")return {{"loaded",true},{"name","Documentation fixture"},{"epoch",1}};
            if(op=="gameplay.operations")return Json::parse(
#include "../adapters/mt2/gameplay/operations.inc"
            );
            if(op=="gameplay.query"){
                const auto operation=args.at("operation").get<std::string>();
                if(operation=="regions.at")return {{"epoch",1},{"id",2}};
                if(operation=="terrain.heights")return {{"width",3},{"height",2},{"origin",{0,0,0}},{"cell_size",4}};
                if(operation=="terrain.samples"){Json result=Json::array();for(const auto& point:args.at("arguments").at("points")){
                    auto row=point;row["index"]=point.at("row").get<int32_t>()*3+point.at("column").get<int32_t>();row["height"]=2.5;result.push_back(row);}return result;}
            }
            if(op=="objects.list")return Json::array({object});
            if(op=="objects.get"||op=="objects.selected"||op=="objects.resolve")return object;
            if(op=="objects.persist")return "fixture-reference";
            if(op=="objects.building") { auto result=object;result.update({{"type","Scenery"},{"variant","Oak"},{"rotation",{0,0,0,1}},{"scale",{1,1,1}},{"in_dungeon",false},{"in_dungeon_instance",false}});return result; }
            if(op=="world.region")return {{"epoch",1},{"x",args.at("x")},{"z",args.at("z")},{"type",0},{"owned",true},{"purchase_price",1000},{"cash",5000}};
            if(op=="world.simulation")return {{"epoch",1},{"cash",5000},{"logged_in_subscribers",42},{"clock",{{"paused",true},{"speed",1},{"day",1},{"seconds",0}}},{"ledger",{{"day",1},{"cash",5000},{"income",200},{"expense",50},{"lines",Json::array()}}}};
            if(op=="world.software")return {{"epoch",1},{"rating",75.5},{"fun",80.5}};
            if(op=="camera.get")return {{"x",1},{"y",2},{"z",3},{"focus_x",1},{"focus_y",2},{"focus_z",3},{"distance",30},{"min",10},{"max",1000},{"editable",true},{"script_weight",1}};
            if(op=="camera.focus")return {{"accepted",true}};
            if(op=="time.get"||op=="time.set")return {{"paused",true},{"speed",1},{"day",1},{"seconds",0}};
            if(op=="content.register")return {{"registered",args.at("definitions").size()}};
            if(op=="actions.status")return {{"id",args.at("id")},{"status","confirmed"},{"cause",""},{"object",object}};
            if(op.starts_with("actions."))return {{"id","fixture-"+std::to_string(++action)},{"status","accepted"}};
            if(op=="ui.begin"||op=="ui.end"||op=="ui.text")return true;
            if(op=="ui.button")return false;
            if(op=="ui.number"||op=="ui.checkbox")return args.at("value");
            if(op=="ui.app")return {{"open_requested",true},{"desktop",{{"requested",false},{"registered",false},{"visible",nullptr},{"visibility_known",false},{"error",""}}}};
            if(op=="ui.native")return {{"ready",true},{"open",args.at("open")},{"visible",true},{"visibility_known",true},{"suspended",false},{"clicked",Json::array()},{"values",Json::object()}};
            if(op=="ui.document")return {{"open",true},{"clicked",Json::array()},{"values",Json::object()}};
            throw std::runtime_error("Fixture has no operation: "+op);
        });
        runtime.set_patch_authority([](const std::string&, std::string&) { return true; });
        runtime.load();runtime.world_ready("Documentation fixture");runtime.emit("update",{{"dt",0.016},{"epoch",1}});
        runtime.rendering=true;runtime.emit("render");runtime.rendering=false;
        // App examples open through their launcher before taking a bounded update snapshot.
        if(manifest.source.at("api_version")=="1.1.0")runtime.emit("update",{{"dt",1.0},{"epoch",1}});
        runtime.emit("input",{{"owner",manifest.id},{"name","show_message"},{"scancode",66}});
        Json exports=Json::array();
        for(auto& log:runtime.logs())if(log.message.starts_with("@export "))exports.push_back(log.message.substr(8));
        Json result={{"mods",runtime.status().at("mods")},{"exports",exports},{"host_calls",calls},{"patch_targets",runtime.patches.targets()},{"fixture_only",true}};
        atomic_json(fs::u8path(argv[2]),result);
        if(runtime.plugins().size()!=1||!runtime.plugins()[0]->active)throw std::runtime_error("Example did not remain active: "+result.dump());
        std::cout<<"info: real Lua example executed with controlled host; result="<<argv[2]<<"\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"error: "<<error.what()<<"\n";return 1;}
}
