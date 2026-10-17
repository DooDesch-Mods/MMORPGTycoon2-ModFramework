#include <nlohmann/json.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>

using Json = nlohmann::json;
using DWORD = unsigned long;
struct Plugin { bool active = true; };
struct Runtime {
    std::vector<std::unique_ptr<Plugin>> owned;
    const auto& plugins() const { return owned; }
} storage;
struct App { const Plugin* owner; };
struct NativeWindow {
    const Plugin* owner; void* mode; void* panel; std::string name;
    bool requested = true, suspended = false;
};
Runtime* runtime = &storage;
std::map<std::string, App> apps;
std::map<std::string, NativeWindow> native_windows;
std::array<unsigned char, 1536> window_bytes{}, button_bytes{}, titlebar_bytes{};
void* mode = nullptr;
void* panel = nullptr;
void* fixture_registered = nullptr;
DWORD thread = 7;
bool panel_open = false, readable = true;
size_t fixture_unreadable_offset = 0;
std::string fault, button_command, last_error;
int calls = 0, effect = 0;
DWORD GetCurrentThreadId() { return 7; }
void require(bool accepted, const std::string& cause) { if (!accepted) throw std::runtime_error(cause); }
void check(DWORD error, const std::string& cause) { require(!error, cause); }
template<class T> bool read(const void* pointer, size_t offset, T& value) {
    if (!readable || (pointer == button_bytes.data() && offset == fixture_unreadable_offset)) return false;
    std::memcpy(&value, static_cast<const unsigned char*>(pointer) + offset, sizeof(T)); return true;
}
template<class T> void write(void* pointer, size_t offset, T value) {
    std::memcpy(static_cast<unsigned char*>(pointer) + offset, &value, sizeof(T));
}
bool text(const void*, std::string& result) { result = button_command; return true; }
void log_error(const std::string& cause) { last_error = cause; }
DWORD window_lookup(void** result, const char*) { *result = fixture_registered; return 0; }
int window_lookups = 0, titlebar_lookups = 0;
bool close_missing = false;
DWORD titlebar_lookup_error = 0;
DWORD string_call(uintptr_t rva, void* receiver, const char* value, void** output) {
    require(std::string(value) == "close", "plain original close ID");
    *output = nullptr;
    if (rva == 0x81d860) {
        require(receiver == window_bytes.data(), "exact Window::FindPane receiver");
        ++window_lookups;
        return 0;
    }
    require(rva == 0x816150 && receiver == titlebar_bytes.data(), "exact titlebar mmoView::FindPane receiver");
    ++titlebar_lookups;
    if (!close_missing && !titlebar_lookup_error) *output = button_bytes.data();
    return titlebar_lookup_error;
}
#include "native_pane.inc"
DWORD activate_native(void* target, bool icon) {
    require(target == button_bytes.data() && !icon, "actual button activation"); ++calls;
    if (effect == 1) return 0;
    if (effect == 2) return 0xc0000005;
    if (effect == 3) { readable = false; return 0; }
    if (effect == 4) { fixture_registered = nullptr; return 0; }
    if (effect == 5) { mode = nullptr; return 0; }
    write<uint8_t>(window_bytes.data(), 0x45f, 0); return 0;
}
#include "previous_app_close.inc"
#include "previous_titlebar_close.inc"
#include "native_app_close.inc"

void reset() {
    storage.owned.clear(); storage.owned.push_back(std::make_unique<Plugin>());
    runtime = &storage; fault.clear(); last_error.clear(); calls = effect = 0;
    readable = true; panel_open = false; thread = 7; fixture_unreadable_offset = 0;
    mode = &storage; panel = &apps; fixture_registered = window_bytes.data();
    window_bytes.fill(0); button_bytes.fill(0); titlebar_bytes.fill(0);
    window_lookups = titlebar_lookups = 0; close_missing = false; titlebar_lookup_error = 0;
    apps = {{"mp", {storage.owned[0].get()}}};
    native_windows = {{"mp", {storage.owned[0].get(), mode, fixture_registered, "exact-native-window"}}};
    write<uint8_t>(fixture_registered, 0x45f, 1);
    write<void*>(fixture_registered, 0x3b8, titlebar_bytes.data());
    write<void*>(titlebar_bytes.data(), 0x280, fixture_registered);
    write<void*>(button_bytes.data(), 0x280, fixture_registered);
    write<uint8_t>(button_bytes.data(), 0x2b4, 0);
    write<uint8_t>(button_bytes.data(), 0x69, 1);
    button_command = "MessageTo Window Close";
}
int checks = 0;
void expect(bool accepted, const char* cause) { ++checks; require(accepted, cause); }
template<class Change> void reject(Change change) {
    reset(); change(); bool failed = false;
    try { close_native_app("mp", Json::object()); }
    catch (const std::runtime_error&) { failed = true; }
    expect(failed && calls == 0, "precondition rejects without native mutation");
}
int main() {
    try {
        reset(); bool previous_rejected = false;
        try { previous_close_native_app("mp", Json::object()); }
        catch (const std::runtime_error& error) {
            previous_rejected = std::string(error.what()) == "Native app pane missing: close";
        }
        expect(previous_rejected && calls == 0 && window_lookups == 1 && titlebar_lookups == 0,
            "V4 regression: Window lookup excludes titlebar X and never activates");
        reset(); bool titlebar_rejected = false;
        try { previous_titlebar_close("mp", Json::object()); }
        catch (const std::runtime_error& error) {
            titlebar_rejected = std::string(error.what()) == "Native app close button is disabled or hidden";
        }
        expect(titlebar_rejected && calls == 0 && window_lookups == 0 && titlebar_lookups == 1,
            "previous titlebar helper rejects measured disabled=0 shown=0 entity_shown=1 before Activate");
        reset(); auto result = close_native_app("mp", Json::object());
        expect(window_lookups == 0 && titlebar_lookups == 1, "corrected helper queries the owned titlebar directly");
        expect(calls == 1 && result.at("close_confirmed") && result.at("result") == "confirmed", "X closes once");
        expect(result.at("visibility_known") && result.at("visible") == false, "actual native visibility readback");
        expect(native_windows.at("mp").requested, "normal ui.native polling consumes close without forced reopen");
        expect(result.at("native_attempted") && result.at("native_returned") &&
            window_bytes[0x45f] == 0 && button_bytes[0x2b4] == 0 && button_bytes[0x69] == 1,
            "measured titlebar state closes through one original call without synthesizing notification state");
        reset(); write<uint8_t>(button_bytes.data(), 0x2b4, 1);
        result = close_native_app("mp", Json::object());
        expect(calls == 1 && result.at("close_confirmed") && result.at("visibility_known") &&
            result.at("visible") == false, "notified titlebar state remains accepted");
        reject([] { write<void*>(fixture_registered, 0x3b8, nullptr); });
        reject([] { write<void*>(titlebar_bytes.data(), 0x280, nullptr); });
        reject([] { close_missing = true; });
        reject([] { titlebar_lookup_error = 5; });
        reject([] { fault = "prior adapter fault"; });
        reject([] { thread = 8; });
        reject([] { runtime = nullptr; });
        reject([] { apps.clear(); });
        reject([] { native_windows.clear(); });
        reject([] { apps.at("mp").owner = nullptr; });
        reject([] { storage.owned[0]->active = false; });
        reject([] { native_windows.at("mp").mode = nullptr; });
        reject([] { fixture_registered = nullptr; });
        reject([] { panel_open = true; });
        reject([] { native_windows.at("mp").suspended = true; });
        reject([] { native_windows.at("mp").requested = false; });
        reject([] { write<uint8_t>(fixture_registered, 0x45f, 0); });
        reject([] { write<void*>(button_bytes.data(), 0x280, nullptr); });
        reject([] { write<uint8_t>(button_bytes.data(), 0x46d, 1); });
        reject([] { write<uint8_t>(button_bytes.data(), 0x69, 0); });
        reject([] { write<uint8_t>(button_bytes.data(), 0x2b4, 2); });
        reject([] { fixture_unreadable_offset = 0x46d; });
        reject([] { fixture_unreadable_offset = 0x2b4; });
        reject([] { fixture_unreadable_offset = 0x69; });
        reject([] { button_command = "MessageTo Mode MT2FWNative custom close"; });
        for (int value = 1; value <= 5; ++value) {
            reset(); effect = value; result = close_native_app("mp", Json::object());
            expect(calls == 1 && !result.at("close_confirmed"), "failed or unknown close is not replayed");
            expect(result.at("result") == (value == 1 ? "rejected" : "unknown"), "outcomes stay distinct");
            expect(!last_error.empty(), "original failure remains visible");
        }
        std::cout << checks << " native app close conditions passed\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
