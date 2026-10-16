#pragma once
#include "mt2fw/core.h"
namespace mt2fw::mt2 {
void initialize(const fs::path& game,const fs::path& profile);
void frame(Runtime& runtime,float dt);
Json request(const std::string& operation,const Json& args);
bool in_world();
void* game_update();
void refresh_saved_bindings();
void measurement_frame();
struct Hook {void* target;void* replacement;void** original;};
std::vector<Hook> observation_hooks();
Json diagnostic(const std::string& operation,const Json& args);
}
