#pragma once
#include "game/native_engine.h"
namespace mt2fw::game::native {
// Uses the active map root independently of the camera shard.
bool read_building_global(uint64_t uid,BuildingState& state,std::string& error);
}
