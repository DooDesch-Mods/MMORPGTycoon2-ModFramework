#pragma once
#include "adapter.h"

namespace mt2fw::mt2::world_markers {
void configure(Runtime& runtime);
std::vector<Hook> hooks();
Json request(const std::string& operation, const Json& arguments);
void frame();
bool retire(Plugin& owner);
bool shutdown();
}
