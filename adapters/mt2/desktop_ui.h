#pragma once
#include "adapter.h"

namespace mt2fw::mt2::desktop {
using Details = std::function<void(const std::string&, bool)>;
void configure(Runtime& runtime, const fs::path& profile, Details details, std::function<void()> language);
std::vector<Hook> hooks();
void frame(bool german);
void end_frame();
bool command(void* mode, const void* text);
void request_toggle();
bool visible();
Json status();
Json activate(const Json& args);
Json app(const Json& args);
Json new_world(const Json& args);
Json native_window(const Json& args);
}
