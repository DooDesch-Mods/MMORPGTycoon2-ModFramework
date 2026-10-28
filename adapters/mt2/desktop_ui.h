#pragma once
#include "adapter.h"
union SDL_Event;

namespace mt2fw::mt2::desktop {
using Details = std::function<void(const std::string&, bool)>;
using VerifyBody = std::function<bool(uint64_t,size_t,const std::string&,std::string&)>;
void configure(Runtime& runtime, const fs::path& profile, Details details, std::function<void()> language, VerifyBody verify_body);
std::vector<Hook> hooks();
void frame(bool german);
void end_frame();
bool command(void* mode, const void* text);
bool input(const SDL_Event& event, uint32_t game_window, bool keyboard_captured);
void request_toggle();
bool visible();
Json status();
Json activate(const Json& args);
Json app(const Json& args);
Json new_world(const Json& args);
Json native_window(const Json& args);
Json terminal_activate(const Json& args);
Json terminal_status();
bool terminal_draw(uint64_t lease,uintptr_t window,int width,int height,const std::function<void()>& repaint,std::string& error);
}
