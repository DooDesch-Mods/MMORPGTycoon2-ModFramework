#include "adapter.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/logger.h"
#include "crypto.h"
#include "native_extensions.h"
#include "public_reads.h"
#include "gameplay/dispatch.h"
#include <windows.h>
#include <cmath>
#include <deque>

namespace mt2fw::mt2 {
namespace native=mt2mp::game::native;
namespace {
fs::path game_root,profile_root;
Runtime* current_runtime=nullptr;
uint64_t current_epoch=0;
std::string current_world;
bool world=false;
struct Action {std::string id,owner,operation;Json arguments,result;uint64_t epoch;uint64_t instance=0;};
std::deque<Action> pending;
std::map<std::string,Action> completed;
std::deque<std::string> completed_order;
std::map<std::string,Json> content;
std::set<std::string> mounted;
std::string uuid() {
    unsigned char bytes[16]{};auto result=BCryptGenRandom(nullptr,bytes,sizeof(bytes),BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(result<0)throw std::runtime_error("Action ID generation failed NTSTATUS="+std::to_string(result));
    std::string text;for(auto byte:bytes){text+="0123456789abcdef"[byte>>4];text+="0123456789abcdef"[byte&15];}return text;
}
void mount(const fs::path& directory) {
    if(mounted.contains(path_text(directory)))return;
    using Mount=int(*)(const char*,const char*,int);
    auto physfs=GetModuleHandleW(L"libphysfs.dll");auto fn=physfs?reinterpret_cast<Mount>(GetProcAddress(physfs,"PHYSFS_mount")):nullptr;
    auto exists=physfs?reinterpret_cast<int(*)(const char*)>(GetProcAddress(physfs,"PHYSFS_exists")):nullptr;
    if(!exists)throw std::runtime_error("PhysFS resource collision check unavailable");
    std::set<std::string> resources;
    for(auto& entry:fs::recursive_directory_iterator(directory))if(entry.is_regular_file()) {
        auto name=path_text(fs::relative(entry.path(),directory));confined(directory,name);
        auto folded=name;std::transform(folded.begin(),folded.end(),folded.begin(),[](unsigned char c){return char(std::tolower(c));});
        if(!resources.insert(folded).second||exists(name.c_str()))throw std::runtime_error("Content resource conflicts with an already registered resource: "+name);
    }
    auto encoded=directory.generic_u8string();std::string path(reinterpret_cast<const char*>(encoded.data()),encoded.size());
    if(!fn||!fn(path.c_str(),nullptr,1))throw std::runtime_error("Authored content PhysFS mount failed: "+path_text(directory));
    mounted.insert(path_text(directory));
}
std::string require(bool result,const std::string& error){if(!result)throw std::runtime_error(error);return error;}
Json handle(uint64_t uid){return {{"epoch",current_runtime->epoch},{"uid",std::to_string(uid)}};}
uint64_t checked_handle(const Json& value) {
    if(!current_runtime||!world||value.at("epoch").get<uint64_t>()!=current_runtime->epoch)throw std::runtime_error("Object handle belongs to another world epoch");
    auto uid=value.at("uid").get<std::string>();size_t used=0;auto result=std::stoull(uid,&used);if(used!=uid.size()||!result)throw std::runtime_error("Invalid object ID");return result;
}
Json prop(const native::PropState& row) {return {{"handle",handle(row.uid)},{"x",row.x},{"y",row.y},{"z",row.z}};}
void perform_actions(Runtime& runtime) {
    auto ready=std::move(pending);pending.clear();
    for(auto& action:ready) {
        bool attempted=false;std::string error;bool confirmed=false;Json object;
        try {
            if(!world||action.epoch!=runtime.epoch)throw std::runtime_error("Action belongs to a world that is no longer active");
            if(action.instance&&!std::any_of(runtime.plugins().begin(),runtime.plugins().end(),[&](const auto& plugin){return plugin->active&&plugin->manifest.id==action.owner&&plugin->instance==action.instance;}))
                throw std::runtime_error("Action owner was disabled or replaced before execution");
            auto& args=action.arguments;
            if(action.operation=="gameplay.action") {
                std::string reason;if(!runtime.patch_authorized("api.gameplay.action",reason))throw std::runtime_error(reason);
                auto context=gameplay::context(runtime.epoch);
                confirmed=gameplay::action(context,args.at("operation"),args.at("arguments"),attempted,object,error);
                if(confirmed&&!error.empty())throw std::runtime_error("Gameplay handler returned confirmation with an error: "+error);
                if(!confirmed&&error.empty())error="Native gameplay action returned without confirmation or cause";
            }else if(action.operation=="actions.place") {
                std::string type,variant;auto definition=args.at("definition").get<std::string>();
                if(definition.starts_with("mt2:")){auto parts=definition.substr(4);auto slash=parts.find('/');if(slash==std::string::npos)throw std::runtime_error("Built-in definition format: mt2:Type/Variant");type=parts.substr(0,slash);variant=parts.substr(slash+1);}
                else {auto found=content.find(definition);if(found==content.end())throw std::runtime_error("Unknown content definition: "+definition);type="Scenery";variant=found->second.at("variant");}
                native::BuildingState result;confirmed=native::place_authored_building(type,variant,args.at("x"),args.at("z"),result,attempted,error);if(confirmed)object=prop(result.prop);
            }else if(action.operation=="actions.move") {auto uid=checked_handle(args.at("handle"));confirmed=native::move_authored_prop(uid,args.at("x"),args.at("y"),args.at("z"),attempted,error);if(confirmed){native::PropState row;require(native::read_prop(uid,row,error),error);object=prop(row);}}
            else if(action.operation=="actions.remove"){auto uid=checked_handle(args.at("handle"));confirmed=native::remove_authored_prop(uid,attempted,error);}
            else throw std::runtime_error("Unsupported queued action");
        }catch(const std::exception& problem){error=problem.what();}
        action.result={{"id",action.id},{"status",confirmed?"confirmed":attempted?"unknown":"rejected"},{"native_attempted",attempted},{"cause",error}};
        if(!object.is_null())action.result[action.operation=="gameplay.action"?"result":"object"]=object;
        if(action.operation=="gameplay.action")action.result["operation"]=action.arguments.at("operation");
        if(!confirmed)runtime.log(action.owner,attempted?"error":"warn",action.operation+": "+error);
        runtime.emit("action_completed",action.result);
        completed[action.id]=action;completed_order.push_back(action.id);
        if(completed_order.size()>256){completed.erase(completed_order.front());completed_order.pop_front();}
    }
}
}
void initialize(const fs::path& game,const fs::path& profile) {
    game_root=game;profile_root=profile;mt2mp::Logger::instance().init(profile.string());
    std::string error;require(native::initialize(error),error);
}
void* game_update(){return native::entrypoint(native::EntryPoint::GameUpdate);}
std::vector<Hook> observation_hooks(){std::vector<Hook> result;for(auto& item:native::observation_hooks())result.push_back({item.target,item.replacement,item.original});return result;}
bool in_world(){return world;}
void measurement_frame(){if(!native::bind_game_thread())throw std::runtime_error("Measurement game-thread binding rejected");}
void refresh_saved_bindings() {
    if(!current_runtime||!world)return;
    for(auto& [id,row]:current_runtime->object_bindings.items()) {
        if(row.value("deleted",false))continue;
        std::string error;native::PropState prop;
        if(row.value("epoch",uint64_t(-1))==current_runtime->epoch){if(!native::read_prop(std::stoull(row.at("uid").get<std::string>()),prop,error)){row["deleted"]=true;continue;}}
        else require(native::resolve_location(row.at("location"),prop,error),error);
        Json location;require(native::saved_location(prop.uid,location,error),error);row["location"]=location;row["uid"]=std::to_string(prop.uid);row["epoch"]=current_runtime->epoch;
    }
}
void observe_world(Runtime& runtime) {
    current_runtime=&runtime;
    if(!native::bind_game_thread())throw std::runtime_error("Native game-thread identity rejected");
    bool active=native::in_game();
    if(active) {
        std::string folder,error;require(native::read_active_world_folder(folder,error),error);
        if(!world||folder!=current_world){current_world=folder;world=true;runtime.world_ready(folder);}
    }else if(world){runtime.world_leave();world=false;current_world.clear();}
}
void frame(Runtime& runtime,float dt) {
    observe_world(runtime);
    perform_actions(runtime);
    auto changes=native::take_object_changes();
    if(world)for(auto& change:changes){auto object=prop(change.prop);object["deleted"]=change.removed;runtime.emit("object_changed",{{"operation",change.operation},{"source","native"},{"object",object}});}
    runtime.emit("update",{{"dt",dt},{"epoch",runtime.epoch}});
}
Json request(const std::string& op,const Json& args) {
    std::string error;
    if(op=="world.status")return {{"loaded",world},{"name",current_world},{"epoch",current_runtime?current_runtime->epoch:0}};
    if(op=="gameplay.operations")return gameplay::operations();
    if(op=="gameplay.query") {
        if(!current_runtime||!world)throw std::runtime_error("Gameplay query requires a loaded world");
        auto context=gameplay::context(current_runtime->epoch);
        return gameplay::query(context,args.at("operation"),args.at("arguments"));
    }
    if(op=="gameplay.action") {
        if(!current_runtime||!world)throw std::runtime_error("Gameplay action requires a loaded world");
        if(pending.size()>=64)throw std::runtime_error("Pending action budget exceeded (64)");
        gameplay::validate_operation(args.at("operation"),"action",args.at("arguments"));
        Action action{uuid(),args.at("mod_id").get<std::string>(),op,args,{},current_runtime->epoch,args.at("mod_instance").get<uint64_t>()};
        action.result={{"id",action.id},{"status","accepted"},{"operation",args.at("operation")}};
        pending.push_back(action);return action.result;
    }
    if(op=="world.region"||op=="world.simulation"||op=="world.software") {
        if(!current_runtime||!world)throw std::runtime_error("World query requires a loaded world");
        if(op=="world.region") {
            auto x=public_reads::coordinate(args,"x"),z=public_reads::coordinate(args,"z");
            native::RegionState state;require(native::read_region_state(x,z,state,error),error);
            return public_reads::region(state,x,z,current_runtime->epoch);
        }
        if(op=="world.simulation") {
            native::SimulationState state;require(native::read_simulation_state(state,error),error);
            return public_reads::simulation(state,current_runtime->epoch);
        }
        native::SoftwareState state;require(native::read_software_state(state,error),error);
        return public_reads::software(state,current_runtime->epoch);
    }
    if(op=="time.get") {native::ClockState state;require(native::read_clock(state,error),error);return {{"paused",state.paused},{"speed",state.time_scale},{"day",state.flat_day},{"seconds",state.seconds}};}
    if(op=="time.set") {require(native::set_clock(args.at("paused").get<bool>(),args.at("speed").get<float>(),error),error);return request("time.get",Json::object());}
    if(op=="objects.list") {
        int count=args.value("limit",100);if(count<1||count>128)throw std::runtime_error("Object query limit must be 1..128");
        std::vector<native::PropState> rows;require(native::list_props(count,rows,error),error);Json result=Json::array();for(auto& row:rows)result.push_back(prop(row));return result;
    }
    if(op=="objects.get") {auto uid=checked_handle(args.at("handle"));native::PropState state;require(native::read_prop(uid,state,error),error);return prop(state);}
    if(op=="objects.building") {auto uid=checked_handle(args.at("handle"));native::BuildingState state;require(native::read_building(uid,state,error),error);return public_reads::building(state,current_runtime->epoch);}
    if(op=="objects.persist") {
        auto uid=checked_handle(args.at("handle"));Json location;require(native::saved_location(uid,location,error),error);
        if(current_runtime->object_bindings.size()>=1024)throw std::runtime_error("Persistent object budget exceeded (1024)");
        auto id=uuid();current_runtime->object_bindings[id]={{"owner",args.at("mod_id")},{"location",location},{"uid",std::to_string(uid)},{"epoch",current_runtime->epoch}};return id;
    }
    if(op=="objects.resolve") {
        auto id=args.at("id").get<std::string>();auto& rows=current_runtime->object_bindings;
        if(!world||!rows.contains(id)||rows[id].at("owner")!=args.at("mod_id")||rows[id].value("deleted",false))throw std::runtime_error("Persistent object reference is missing or belongs to another mod");
        auto& row=rows[id];native::PropState state;
        if(row.value("epoch",uint64_t(-1))==current_runtime->epoch)require(native::read_prop(std::stoull(row.at("uid").get<std::string>()),state,error),error);
        else require(native::resolve_location(row.at("location"),state,error),error);
        row["uid"]=std::to_string(state.uid);row["epoch"]=current_runtime->epoch;return prop(state);
    }
    if(op=="objects.selected"){native::PropState row;bool selected=false;require(native::selected_prop(row,selected,error),error);return selected?prop(row):Json();}
    if(op=="content.register") {
        auto owner=args.at("mod_id").get<std::string>();auto directory=fs::u8path(args.at("directory").get<std::string>());auto definitions=args.at("definitions");
        if(!definitions.is_array()||definitions.empty())throw std::runtime_error("Content definitions must be a nonempty array");
        std::map<std::string,Json> staged;
        for(auto definition:definitions) {
            auto id=definition.at("id").get<std::string>();
            if(!id.starts_with(owner+":")||content.contains(id)||staged.contains(id))throw std::runtime_error("Content ID is duplicated or outside its mod namespace: "+id);
            for(const char* field:{"model","material"})if(!fs::is_regular_file(confined(directory,definition.at(field).get<std::string>())))throw std::runtime_error("Content resource missing: "+std::string(field));
            staged[id]=definition;
        }
        mount(confined(directory,"assets"));for(auto& [id,definition]:staged)content[id]=definition;return {{"registered",staged.size()}};
    }
    if(op=="actions.status") {
        auto id=args.at("id").get<std::string>();auto owner=args.at("mod_id").get<std::string>();
        for(auto& action:pending)if(action.id==id&&action.owner==owner)return action.result;
        auto found=completed.find(id);if(found==completed.end()||found->second.owner!=owner)throw std::runtime_error("Action ID is unknown, expired or belongs to another mod");return found->second.result;
    }
    if(op=="actions.place"||op=="actions.move"||op=="actions.remove") {
        if(!current_runtime||!world)throw std::runtime_error("World action requires a loaded world");
        if(pending.size()>=64)throw std::runtime_error("Pending action budget exceeded (64)");
        for(const char* field:{"x","y","z"})if(op!="actions.remove"&&(!args.contains(field)||!args[field].is_number()||!std::isfinite(args[field].get<double>())))throw std::runtime_error("Action requires finite x, y and z");
        if(op!="actions.place")checked_handle(args.at("handle"));
        Action action{uuid(),args.at("mod_id").get<std::string>(),op,args,{},current_runtime->epoch,args.at("mod_instance").get<uint64_t>()};action.result={{"id",action.id},{"status","accepted"}};
        pending.push_back(action);return action.result;
    }
    if(op=="camera.get") {native::CameraState s;require(native::read_camera_state(s,error),error);return {{"x",s.x},{"y",s.y},{"z",s.z},{"focus_x",s.focus_x},{"focus_y",s.focus_y},{"focus_z",s.focus_z},{"distance",s.distance},{"min",s.minimum_distance},{"max",s.maximum_distance},{"editable",s.edit_weight>=0.999f},{"script_weight",s.edit_weight}};}
    if(op=="camera.focus") {native::CameraState s;require(native::diagnostic_focus_camera(args.at("x"),args.at("y"),args.at("z"),args.at("distance"),s,error),error);return {{"accepted",true}};}
    throw std::runtime_error("Adapter operation is not available: "+op);
}
Json diagnostic(const std::string& op,const Json& args) {
    std::string error;
    if(op=="engine_status") {native::WorldStatus s;require(native::read_world_status(s,error),error);return {{"active_mode",s.active_mode_index},{"requested_mode",s.requested_mode_index},{"world_present",s.world_present},{"world_ready",native::in_game()},{"crash_report_active",s.crash_report_active},{"title_transition_pending",s.title_transition_pending},{"saves_in_progress",s.saves_in_progress}};}
    if(op=="select_info"){require(native::diagnostic_select_prop(checked_handle(args.at("handle")),error),error);return {{"confirmed",true}};}
    if(op=="placement_probe"){native::PropState row;require(native::probe_building_placement(args.value("type","Scenery"),args.at("variant"),args.at("x"),args.at("z"),row,error),error);return {{"x",row.x},{"y",row.y},{"z",row.z},{"clear",true}};}
    if(op=="native_move_once"||op=="native_remove_once"){
        auto uid=checked_handle(args.at("handle"));bool attempted=false;
        bool confirmed=op=="native_move_once"?native::move_authored_prop(uid,args.at("x"),args.at("y"),args.at("z"),attempted,error):native::remove_authored_prop(uid,attempted,error);
        return {{"status",confirmed?"confirmed":attempted?"unknown":"rejected"},{"attempted",attempted},{"cause",error}};
    }
    if(op=="dismiss_crash_report"){require(native::request_dismiss_crash_report(error),error);return {{"accepted",true}};}
    if(op=="prepare_world"){require(native::prepare_test_world(args.value("name","MPTestFramework"),error),error);return {{"accepted",true}};}
    if(op=="world_ready"){bool complete=false;require(native::test_world_ready(complete,error),error);return {{"ready",complete}};}
    if(op=="create_world"){require(native::create_test_world(args.value("name","MPTestFramework"),error),error);return {{"accepted",true}};}
    if(op=="save"){require(native::capture_snapshot(args.at("path"),error),error);return {{"accepted",true}};}
    if(op=="load"){require(native::request_load(args.at("path"),error),error);return {{"accepted",true}};}
    if(op=="title"){require(native::request_return_to_title(error),error);return {{"accepted",true}};}
    if(op=="exit"){require(native::request_exit_game(error),error);return {{"accepted",true}};}
    if(op=="model_probe"){mount(confined(profile_root,"model-probe"));require(native::load_authored_model("mt2fw-probe/pyramid",error),error);return {{"loaded",true},{"format","vmd"}};}
    return request(op,args);
}
}
