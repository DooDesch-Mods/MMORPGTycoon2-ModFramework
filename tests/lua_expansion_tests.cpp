#include "mt2fw/core.h"
#include "public_reads.h"
#include <fstream>
#include <iostream>

using namespace mt2fw;
namespace reads = mt2fw::mt2::public_reads;
namespace native = mt2mp::game::native;
int checks = 0;
void expect(bool result, const char* cause) { ++checks; if (!result) throw std::runtime_error(cause); }
template<class Call> void rejects(Call call, const char* cause) {
    try { call(); } catch (const std::exception& error) {
        expect(std::string(error.what()).find(cause) != std::string::npos, "Failure lost its cause"); return;
    }
    throw std::runtime_error("Expected rejection did not occur");
}
void write(const fs::path& file, const std::string& text) {
    std::ofstream stream(file); stream << text;
    if (!stream) throw std::runtime_error("Cannot write Lua fixture");
}
int main() {
    try {
        expect(reads::coordinate({{"x",-3.5}},"x")==-3.5f,"World coordinates changed");
        rejects([] { reads::coordinate(Json::object(),"x"); }, "finite x");
        rejects([] { reads::coordinate({{"x","1"}},"x"); }, "finite x");
        rejects([] { reads::coordinate({{"x",1e300}},"x"); }, "finite float x");
        rejects([] { reads::coordinate({{"x",std::numeric_limits<double>::quiet_NaN()}},"x"); }, "finite float x");
        native::BuildingState building; building.prop={9007199254740993ull,1,2,3};
        building.type="Scenery";building.variant="Oak";building.rotation={0,0.5f,0,0.5f};building.scale={2,3,4};
        auto value=reads::building(building,7);
        expect(value["handle"]["uid"]=="9007199254740993"&&value["handle"]["epoch"]==7,"Handle lost exact UID or epoch");
        expect(!value.contains("exterior")&&value["rotation"][1]==0.5f&&value["scale"][2]==4,"Building transform changed");
        building.exterior_uid=9007199254740995ull;building.dungeon_membership=true;
        value=reads::building(building,8);
        expect(value["exterior"]["uid"]=="9007199254740995"&&value["exterior"]["epoch"]==8&&value["in_dungeon"],"Dungeon identity changed");
        native::SimulationState simulation;simulation.cash=-250;simulation.logged_in_subscribers=42;
        value=reads::simulation(simulation,9);
        expect(value["ledger"].is_null()&&value["cash"]==-250&&value["epoch"]==9,"Absent ledger became a success-shaped record");
        simulation.record_present=true;simulation.record_income=120;simulation.record_expense=30;
        simulation.line_counts[19]=4;simulation.line_income[19]=-17;
        value=reads::simulation(simulation,9);
        expect(value["ledger"]["lines"].size()==20&&value["ledger"]["lines"][19]["category"]==19&&value["ledger"]["lines"][19]["income"]==-17,"Ledger category, size or signed income changed");
        native::RegionState region;region.owned=true;region.purchase_price=1500;
        value=reads::region(region,-10,20,10);
        expect(value["owned"]&&value["purchase_price"]==1500&&value["x"]==-10&&value["epoch"]==10,"Region snapshot changed");
        native::SoftwareState software;software.rating=73.25f;software.fun=8.5f;
        value=reads::software(software,10);
        expect(value.size()==3&&value["rating"]==73.25f&&value["fun"]==8.5f,"Software exposes internal replica state or normalizes values");

        auto root=fs::current_path()/"evidence"/("lua-expansion-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        auto folder=root/"mods/test.expansion";fs::create_directories(folder);
        Json manifest{{"id","test.expansion"},{"name","Expansion"},{"version","1.0.0"},{"api_version","1.1.0"},{"type","lua"},{"entry","main.lua"},{"capabilities",{"ui","world.read","objects.read"}},{"multiplayer","local"}};
        atomic_json(folder/"mod.json",manifest);
        auto supported=manifest;supported["api_version"]="1.3.0";
        expect(Manifest::parse(supported).source.at("api_version")=="1.3.0","Current API minor version was rejected");
        auto future=manifest;future["api_version"]="1.4.0";
        rejects([&] { Manifest::parse(future); }, "API");
        write(folder/"main.lua",R"lua(
mt2.events.on('update',function()
    assert(mt2.world.region(1,2).owned)
    assert(mt2.world.simulation().cash == -250)
    assert(mt2.world.software().rating == 73.25)
    local b=mt2.objects.building({epoch=1,uid='9007199254740993'})
    assert(b.handle.uid == '9007199254740993' and b.scale[3] == 4)
    local ok,cause=pcall(function() mt2.ui.app({id='main',title='Test'}) end)
    assert(not ok and cause:find('render'))
end)
mt2.events.on('render',function()
    local app=mt2.ui.app({id='main',title='Test',mod_id='forged.owner'})
    assert(app.open_requested)
    local ui=mt2.ui.native({id='main',open=true,width=500,height=300,
        widgets={{id='label',type='text',x=0,y=0,width=100,height=30,text='Test'}}})
    assert(ui.ready and ui.clicked[1]=='clicked' and ui.values.search=='Hello')
    local doc=mt2.ui.document({id='doc',title='Test',content={type='text',text='Test'}})
    assert(not doc.open)
end)
)lua");
        std::map<std::string,int> calls;
        Runtime runtime(root,[&](const std::string& op,const Json& args)->Json {
            ++calls[op];expect(args.at("mod_id")=="test.expansion","Lua forged another mod's owner");
            if(op=="objects.building")return reads::building(building,1);
            if(op=="world.region")return reads::region(region,args.at("x"),args.at("z"),1);
            if(op=="world.simulation")return reads::simulation(simulation,1);
            if(op=="world.software")return reads::software(software,1);
            if(op=="ui.app")return {{"open_requested",true}};
            if(op=="ui.native")return {{"ready",true},{"clicked",{"clicked"}},{"values",{{"search","Hello"}}}};
            if(op=="ui.document")return {{"open",false}};
            throw std::runtime_error("Unexpected fixture operation: "+op);
        });
        runtime.load();runtime.world_ready("Fixture");runtime.emit("update");
        runtime.rendering=true;runtime.emit("render");runtime.rendering=false;
        expect(runtime.plugins().at(0)->active,"Lua conversion or UI lifecycle failed");
        expect(calls.size()==7&&calls["ui.app"]==1,"A new Lua operation was missed or a non-render UI call escaped");
        write(folder/"main.lua","this is invalid Lua!");
        expect(!runtime.reload("test.expansion"),"Invalid Lua replacement succeeded");
        runtime.rendering=true;runtime.emit("render");runtime.rendering=false;
        expect(runtime.plugins().at(0)->active&&calls["ui.app"]==2,"Failed replacement removed the old app callback");
        write(folder/"main.lua","mt2.events.on('render',function() mt2.ui.app({id='new',title='Replacement'}) end)");
        expect(runtime.reload("test.expansion"),"Valid Lua replacement failed");
        runtime.rendering=true;runtime.emit("render");runtime.rendering=false;
        expect(calls["ui.app"]==3&&calls["ui.native"]==2,"Replacement kept old UI callbacks");
        auto& owner=*runtime.plugins().at(0);owner.manifest.capabilities.clear();
        rejects([&] { runtime.request(owner,"world.simulation",Json::object()); }, "world.read");
        rejects([&] { runtime.request(owner,"objects.building",{{"handle",{{"epoch",1},{"uid","1"}}}}); }, "objects.read");
        runtime.rendering=true;
        rejects([&] { runtime.request(owner,"ui.app",{{"id","main"},{"title","Test"}}); }, "ui");
        runtime.rendering=false;
        expect(calls["world.simulation"]==1&&calls["objects.building"]==1&&calls["ui.app"]==3,"Denied request reached adapter");
        std::cout<<checks<<" Lua expansion conditions passed\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<"error: "<<error.what()<<'\n';return 1; }
}
