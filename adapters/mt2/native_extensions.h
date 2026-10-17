#pragma once
#include "mt2mp/game/native_engine.h"
#include <nlohmann/json.hpp>
namespace mt2mp::game::native {
bool read_initial_save_world(void* receiver,std::string& folder,std::string& error);
struct ObservationHook {void* target;void* replacement;void** original;};
struct ObjectChange {std::string operation;PropState prop;bool removed=false;};
std::vector<ObservationHook> observation_hooks();
std::vector<ObjectChange> take_object_changes();
bool selected_prop(PropState& result,bool& selected,std::string& error);
bool diagnostic_select_prop(uint64_t uid,std::string& error);
bool saved_location(uint64_t uid,nlohmann::json& location,std::string& error);
bool resolve_location(const nlohmann::json& location,PropState& prop,std::string& error);
bool move_authored_prop(uint64_t uid,float x,float y,float z,bool& attempted,std::string& error);
bool remove_authored_prop(uint64_t uid,bool& attempted,std::string& error);
bool load_authored_model(const std::string& name,std::string& error);
bool place_authored_building(const std::string& type,const std::string& variant,float x,float z,BuildingState& result,bool& attempted,std::string& error);
}
