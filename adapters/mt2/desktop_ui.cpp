#include "desktop_ui.h"
#include "native_extensions.h"
#include "crypto.h"
#include "native_input.h"
#include "native_app_state.h"
#include <windows.h>
#include <array>
#include <chrono>
#include <cstring>
#include <cmath>
#include <fstream>
#include <sstream>

namespace mt2fw::mt2::desktop {
namespace {
namespace native = mt2mp::game::native;
Runtime* runtime = nullptr;
constexpr int desktop_entry_resource = 202;
uintptr_t image = 0;
DWORD thread = 0;
fs::path profile;
Details details;
std::function<void()> language;
void* mode = nullptr;
void* panel = nullptr;
void* current_desktop = nullptr;
bool assets_ready = false, append_attempted = false, icon_attempted = false;
bool panel_open = false, toggle_pending = false, german = false, dirty = true;
int mode_index = -1;
size_t page = 0;
std::string selected, fault, notice, last_command, pending_reload;
uint64_t commands = 0, command_errors = 0;
std::chrono::steady_clock::time_point refreshed{};
std::map<std::string, std::string> labels;
struct App {
    const Plugin* owner = nullptr;
    std::string id, title, image_path, icon_material, icon_error;
    native_app::OpenRequest request;
    bool desktop = false, position_valid = false, presentation_dirty = true;
    bool icon_confirmed = false;
    std::optional<bool> icon_visible;
    void* icon = nullptr;
    void* attempted_desktop = nullptr;
    uint64_t launches = 0;
    std::array<float, 2> position{};
};
std::map<std::string, App> apps;
native_app::WizardLatch wizard;
void native_file(const fs::path& path, const std::string& bytes);
void native_mount();
std::string native_image(const std::string& owner, const std::string& relative);
std::string app_icon_image(const std::string& owner, const std::string& relative);
struct NativeWindow {
    const Plugin* owner = nullptr;
    void* mode = nullptr;
    void* panel = nullptr;
    std::string name, shape, path;
    Json widgets, values = Json::object(), clicked = Json::array();
    bool requested = false, suspended = false;
};
std::map<std::string, NativeWindow> native_windows;
std::map<std::string, std::string> native_images;
uint64_t native_instances = 0;
using MenuElements = void(*)(void*, const void*, float);
using MenuSelect = void(*)(void*, const void*);
MenuElements original_elements = nullptr;
MenuSelect original_select = nullptr;
bool start_entry_registered = false;
uint64_t start_entry_launches = 0;
struct Binding { void* desktop; void* record; std::string app_owner; const Plugin* plugin = nullptr; };
std::map<void*, Binding> icons;
using IconMethod = void(*)(void*);
IconMethod original_launch = nullptr, original_destroy = nullptr;
void* position_owner = nullptr;
std::array<float, 2> retained_position{};
bool position_valid = false;
Json observed = Json::object();
struct alignas(8) String { unsigned char bytes[32]; };
struct alignas(8) Literal { String value; void* args[3]{}; };
struct alignas(8) RecordStorage { unsigned char bytes[1024]; };

const char* tr(const char* en, const char* de) { return german ? de : en; }
void require(bool accepted, const std::string& cause) { if (!accepted) throw std::runtime_error(cause); }
bool readable(const void* address, size_t size) {
    MEMORY_BASIC_INFORMATION region{};
    if (!address || !VirtualQuery(address, &region, sizeof(region)) || region.State != MEM_COMMIT ||
        region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) return false;
    auto offset = reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(region.BaseAddress);
    return offset <= region.RegionSize && size <= region.RegionSize - offset;
}
template<class T> bool read(const void* object, size_t offset, T& value) {
    if (!object) return false;
    auto address = static_cast<const unsigned char*>(object) + offset;
    if (!readable(address, sizeof(T))) return false;
    std::memcpy(&value, address, sizeof(T)); return true;
}
bool text(const void* object, std::string& value) {
    const char* data = nullptr; size_t size = 0;
    if (!read(object, 0, data) || !read(object, 8, size) || size > 8192 || !readable(data, size + 1) || data[size]) return false;
    value.assign(data, size); return true;
}
void log_error(const std::string& cause) {
    notice = cause; dirty = true; ++command_errors;
    if (runtime) runtime->log("framework", "error", "Mods desktop: " + cause);
    else OutputDebugStringA(("error: Mods desktop: " + cause).c_str());
}
DWORD string_call(uintptr_t rva, void* receiver, const char* value, void** output = nullptr) {
    String input{}; bool constructed = false; DWORD error = 0;
    __try {
        reinterpret_cast<void(*)(void*, const char*)>(image + 0x49e090)(&input, value); constructed = true;
        if (output) *output = reinterpret_cast<void*(*)(void*, const void*)>(image + rva)(receiver, &input);
        else reinterpret_cast<void(*)(void*, const void*)>(image + rva)(receiver, &input);
    } __except (EXCEPTION_EXECUTE_HANDLER) { error = GetExceptionCode(); }
    if (constructed) { __try { reinterpret_cast<void(*)(void*)>(image + 0x11bd1e0)(&input); }
        __except (EXCEPTION_EXECUTE_HANDLER) { if (!error) error = GetExceptionCode(); } }
    return error;
}
void check(DWORD code, const std::string& operation) {
    require(!code, operation + " exception=" + std::to_string(code));
}
DWORD window_lookup(void** output, const char* name = "mt2fw_mods") {
    String input{}; bool constructed = false; DWORD error = 0;
    __try {
        reinterpret_cast<void(*)(void*, const char*)>(image + 0x49e090)(&input, name); constructed = true;
        *output = reinterpret_cast<void*(*)(const void*)>(image + 0x81aa90)(&input);
    } __except (EXCEPTION_EXECUTE_HANDLER) { error = GetExceptionCode(); }
    if (constructed) { __try { reinterpret_cast<void(*)(void*)>(image + 0x11bd1e0)(&input); }
        __except (EXCEPTION_EXECUTE_HANDLER) { if (!error) error = GetExceptionCode(); } }
    return error;
}
DWORD show(void* window, bool visible) {
    __try { reinterpret_cast<void(*)(void*, bool, bool)>(image + 0x81ec20)(window, visible, false); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); } return 0;
}
DWORD disable(void* pane, bool disabled) {
    __try { reinterpret_cast<void(*)(void*, bool)>(image + 0x670e00)(pane, disabled); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); } return 0;
}
DWORD text_label(void* pane, const char* value) {
    Literal input{}; bool constructed = false; DWORD error = 0;
    __try {
        reinterpret_cast<void(*)(void*, const char*)>(image + 0xa16f10)(&input, value); constructed = true;
        reinterpret_cast<void(*)(void*, const void*)>(image + 0x7f80a0)(pane, &input);
    } __except (EXCEPTION_EXECUTE_HANDLER) { error = GetExceptionCode(); }
    // Literal construction leaves the localization argument vector empty.
    if (constructed) { __try { reinterpret_cast<void(*)(void*)>(image + 0x11bd1e0)(&input); }
        __except (EXCEPTION_EXECUTE_HANDLER) { if (!error) error = GetExceptionCode(); } }
    return error;
}
DWORD toggle_row(void* pane, bool selected) {
    __try { reinterpret_cast<void(*)(void*, bool)>(image + 0x670d30)(pane, selected); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); } return 0;
}
void* pane(const std::string& id) {
    void* result = nullptr;
    check(string_call(0x81d860, panel, id.c_str(), &result), "Pane lookup " + id);
    require(result != nullptr, "Pane missing: " + id); return result;
}
void* native_pane(void* window, const std::string& id) {
    void* result = nullptr; check(string_call(0x81d860, window, id.c_str(), &result), "Native app pane " + id);
    require(result, "Native app pane missing: " + id); return result;
}
void native_shown(void* target, bool value) {
    uint8_t current = 0; require(read(target, 0x2b4, current) && current <= 1 && read(target, 0x69, current) && current <= 1,
        "Native app pane visibility differs");
    static_cast<unsigned char*>(target)[0x2b4] = uint8_t(value);
    static_cast<unsigned char*>(target)[0x69] = uint8_t(value);
}
void label(const std::string& id, const std::string& value) {
    if (auto found = labels.find(id); found != labels.end() && found->second == value) return;
    static const std::set<std::string> buttons{"mod0","mod1","mod2","mod3","mod4","mod5","previous","next","toggle","reload","settings","tools","language","open_app"};
    check(text_label(pane(buttons.contains(id) ? id + "_label" : id), value.c_str()), "Pane text " + id);
    labels[id] = value;
}
void disabled(const std::string& id, bool value) { check(disable(pane(id), value), "Pane disabled " + id); }
void shown(const std::string& id, bool value) {
    auto* target = static_cast<unsigned char*>(pane(id));
    uint8_t previous = 0;
    require(read(target, 0x2b4, previous) && previous <= 1, "Pane show property differs: " + id);
    MEMORY_BASIC_INFORMATION region{};
    require(VirtualQuery(target + 0x2b4, &region, sizeof(region)) && region.State == MEM_COMMIT &&
        (region.Protect & (PAGE_READWRITE | PAGE_WRITECOPY)) && !(region.Protect & (PAGE_GUARD | PAGE_NOACCESS)), "Pane show property is not writable: " + id);
    require(read(target, 0x69, previous) && previous <= 1 && readable(target + 0x69, 1), "Pane entity visibility differs: " + id);
    // The pane show property and inherited entity visibility control layout and drawing.
    target[0x2b4] = value ? 1 : 0; target[0x69] = value ? 1 : 0;
    require(read(target, 0x2b4, previous) && previous == uint8_t(value), "Pane show property was not acknowledged: " + id);
}
void set_open(bool value) {
    require(panel != nullptr, "Mods window is not registered in the current mode");
    check(show(panel, value), "Window visibility"); panel_open = value; dirty = true;
}
Plugin* selection() {
    for (const auto& plugin : runtime->plugins()) if (plugin->manifest.id == selected) return plugin.get();
    return nullptr;
}
void refresh() {
    auto count = runtime->plugins().size();
    size_t pages = (std::max)(size_t(1), (count + 5) / 6);
    page = (std::min)(page, pages - 1);
    if (!selection() && count) selected = runtime->plugins()[page * 6]->manifest.id;
    label("summary", std::string(tr("Installed mods", "Installierte Mods")) + " (" + std::to_string(count) + ")");
    label("library_title", tr("Installed mods", "Installierte Mods"));
    label("language", german ? "English" : "Deutsch");
    if (!labels.contains("#materials")) {
        for (size_t slot = 0; slot < 6; ++slot) {
            auto id = "mod" + std::to_string(slot);
            check(string_call(0x670ef0, pane(id), "mt2fw_ui_library"), "Mod row material " + id);
            check(string_call(0x671030, pane(id), "mt2fw_ui_selected"), "Selected row material " + id);
        }
        labels["#materials"] = "ready";
    }
    for (size_t slot = 0; slot < 6; ++slot) {
        auto index = page * 6 + slot; auto id = "mod" + std::to_string(slot);
        std::string value;
        if (index < count) {
            const auto& plugin = *runtime->plugins()[index];
            value = plugin.manifest.name;
            if (!plugin.active) value += std::string(" (") + tr("inactive", "inaktiv") + ")";
        }
        label(id, value); disabled(id, index >= count);
        shown(id, index < count); shown(id + "_label", index < count);
        check(toggle_row(pane(id), index < count && runtime->plugins()[index]->manifest.id == selected), "Selected mod row " + id);
    }
    label("page", std::to_string(page + 1) + " / " + std::to_string(pages));
    label("previous", tr("Previous", "Zurück")); disabled("previous", !page);
    label("next", tr("Next", "Weiter")); disabled("next", page + 1 >= pages);
    for (auto& id : {"previous", "previous_label", "next", "next_label", "page"}) shown(id, pages > 1);
    label("settings", tr("Settings", "Einstellungen")); label("tools", tr("Developer tools", "Entwicklerwerkzeuge"));
    auto* plugin = selection();
    if (!plugin) {
        label("name", tr("No mods installed", "Keine Mods installiert")); label("identity", ""); label("state", "");
        label("description", tr("Install a mod ZIP through Vortex or the MT2FW helper.", "Installiere ein Mod-ZIP mit Vortex oder dem MT2FW-Helfer."));
        label("dependencies", ""); label("error", notice);
    } else {
        label("name", plugin->manifest.name);
        label("identity", plugin->manifest.id + " | " + plugin->manifest.version + " | " + plugin->manifest.type);
        label("state", plugin->active ? tr("Active", "Aktiv") : tr("Inactive", "Inaktiv"));
        auto description = plugin->manifest.source.value("description", std::string());
        label("description", description.substr(0, 512));
        std::string dependencies;
        for (auto& [id, range] : plugin->manifest.dependencies) dependencies += id + " " + range + "\n";
        label("dependencies", dependencies.empty() ? tr("No dependencies", "Keine Abhängigkeiten") : std::string(tr("Dependencies:\n", "Abhängigkeiten:\n")) + dependencies);
        label("error", notice.empty() ? plugin->error : notice);
    }
    const bool can_reload = plugin && plugin->manifest.type == "lua" && !plugin->manifest.capabilities.contains("content");
    label("toggle", plugin && runtime->enabled(plugin->manifest.id) ? tr("Disable", "Deaktivieren") : tr("Enable", "Aktivieren"));
    label("reload", tr("Reload Lua", "Lua neu laden")); disabled("toggle", !plugin); disabled("reload", !can_reload || !pending_reload.empty()); disabled("settings", !plugin);
    auto app = apps.find(selected);
    bool can_open = plugin && plugin->active && app != apps.end() && app->second.owner == plugin;
    label("open_app", can_open ? std::string(tr("Open ", "Öffne ")) + app->second.title : tr("Open mod", "Mod öffnen"));
    shown("open_app", can_open); shown("open_app_label", can_open); disabled("open_app", !can_open);
    label("notice", tr("Lua changes can be reloaded.\nNative plugins and content need a restart.", "Lua kann neu geladen werden.\nNative Plugins und Inhalte benötigen einen Neustart."));
    dirty = false; refreshed = std::chrono::steady_clock::now();
}
void invoke(const std::string& value) {
    require(thread && thread == GetCurrentThreadId(), "Command is outside the bound game thread");
    require(mode && panel, "Command requires a current mode-owned Mods window");
    ++commands; last_command = value; notice.clear(); dirty = true;
    if (value == "MT2FWOpen") { set_open(true); return; }
    require(panel_open, "Command requires a visible Mods window");
    if (value.starts_with("MT2FWSelect") && value.size() == 12 && value.back() >= '0' && value.back() <= '5') {
        size_t index = page * 6 + size_t(value.back() - '0');
        require(index < runtime->plugins().size(), "Selected mod row is empty"); selected = runtime->plugins()[index]->manifest.id;
    } else if (value == "MT2FWPrevious") { if (page) --page; }
    else if (value == "MT2FWNext") { if ((page + 1) * 6 < runtime->plugins().size()) ++page; }
    else if (value == "MT2FWLanguage") language();
    else if (value == "MT2FWTools") details("", true);
    else {
        auto* plugin = selection(); require(plugin != nullptr, "No selected mod");
        if (value == "MT2FWToggle") {
            bool enabled = !runtime->enabled(selected);
            require(runtime->set_enabled(selected, enabled), "Mod activation setting was rejected");
            if (enabled && !plugin->active && plugin->manifest.type == "lua" && !plugin->manifest.capabilities.contains("content")) {
                require(pending_reload.empty(), "A Lua reload is already pending");
                pending_reload = selected; notice = tr("Reload requested", "Neuladen angefordert");
            }
        }
        else if (value == "MT2FWReload") {
            require(plugin->manifest.type == "lua" && !plugin->manifest.capabilities.contains("content"), "This mod requires a restart");
            require(pending_reload.empty(), "A Lua reload is already pending");
            pending_reload = selected; notice = tr("Reload requested", "Neuladen angefordert");
        } else if (value == "MT2FWApp") {
            auto app = apps.find(selected);
            require(plugin->active && app != apps.end() && app->second.owner == plugin, "Selected mod has no active app");
            app->second.request.active = true;
            app->second.request.launch(); set_open(false);
        } else if (value == "MT2FWSettings") details(selected, false);
        else throw std::runtime_error("Unknown Mods command: " + value);
    }
}
std::string resource(HMODULE module, int id) {
    auto item = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    require(item != nullptr, "Embedded UI resource missing: " + std::to_string(id));
    auto size = SizeofResource(module, item); auto loaded = LoadResource(module, item);
    auto data = loaded ? LockResource(loaded) : nullptr;
    require(data && size && size <= 512 * 1024, "Embedded UI resource invalid: " + std::to_string(id));
    return {static_cast<const char*>(data), size};
}
void mount_assets() {
    HMODULE module = nullptr;
    require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&mount_assets), &module) != 0, "Framework module identity unavailable");
    std::map<std::string, std::string> files{{"mods.png", resource(module, 201)}, {"entry.win", resource(module, desktop_entry_resource)},
        {"mods.win", resource(module, 203)}, {"materials/mt2fw_desktop_mods.mat", resource(module, 204)}};
    const char* materials[]{"base", "library", "details", "rule", "selected"};
    for (int index = 0; index < 5; ++index) files[std::string("materials/mt2fw_ui_") + materials[index] + ".mat"] = resource(module, 205 + index);
    std::string identity; for (const auto& [name, bytes] : files) identity += name + digest(bytes);
    auto root = profile / "cache/framework-ui" / digest(identity);
    for (const auto& [name, bytes] : files) {
        auto path = confined(root, name);
        if (fs::exists(path)) require(file_digest(path) == digest(bytes), "Cached UI resource differs: " + name);
        else {
            fs::create_directories(path.parent_path()); std::ofstream stream(path, std::ios::binary);
            stream.write(bytes.data(), bytes.size()); stream.close(); require(bool(stream), "Cached UI write failed: " + name);
        }
    }
    auto physfs = GetModuleHandleW(L"libphysfs.dll");
    auto mount = reinterpret_cast<int(*)(const char*, const char*, int)>(GetProcAddress(physfs, "PHYSFS_mount"));
    auto real = reinterpret_cast<const char*(*)(const char*)>(GetProcAddress(physfs, "PHYSFS_getRealDir"));
    require(mount && real, "Native resource mount functions unavailable");
    require(mount(path_text(root).c_str(), "mt2fw_ui", 1) && real("mt2fw_ui/mods.win") && real("mt2fw_ui/entry.win") && real("mt2fw_ui/mods.png"), "Native Mods UI mount acknowledgement failed");
    require(mount(path_text(root / "materials").c_str(), "materials", 1) && real("materials/mt2fw_desktop_mods.mat"), "Native Mods material mount acknowledgement failed");
    assets_ready = true;
}
DWORD delete_record(void* record) {
    __try { reinterpret_cast<void(*)(void*)>(image + 0xbfb4b0)(record); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); } return 0;
}
DWORD make_icon(void* desktop, void** output, void** record, bool* handed_off,
                const char* entry_path = "mt2fw_ui/entry.win", const float* initial_position = nullptr) {
    String filename{}; RecordStorage file{}, source{};
    bool named = false, opened = false, parsed = false; DWORD error = 0;
    __try {
        *record = reinterpret_cast<void*(*)(void*)>(image + 0xd8f2a0)(nullptr);
        if (!*record) return ERROR_NOT_ENOUGH_MEMORY;
        int shell = 6; std::memcpy(static_cast<unsigned char*>(*record) + 0x38, &shell, sizeof(shell));
        float width = 0; std::memcpy(&width, static_cast<unsigned char*>(desktop) + 0x138, sizeof(width));
        float position[2] = {width > 120 ? width - 120 : 20, 180};
        if (initial_position) std::memcpy(position, initial_position, sizeof(position));
        else if (position_valid && position_owner == desktop) std::memcpy(position, retained_position.data(), sizeof(position));
        std::memcpy(static_cast<unsigned char*>(*record) + 0x3c, position, sizeof(position));
        reinterpret_cast<void(*)(void*, const char*)>(image + 0x49e090)(&filename, entry_path); named = true;
        reinterpret_cast<void(*)(void*, const void*, int)>(image + 0x95ec00)(&file, &filename, 0); opened = true;
        reinterpret_cast<void(*)(void*)>(image + 0x9667c0)(&source); parsed = true;
        if (!reinterpret_cast<bool(*)(void*, void*)>(image + 0x95ddb0)(&file, &source)) error = ERROR_INVALID_DATA;
        else { *handed_off = true; *output = reinterpret_cast<void*(*)(void*, void*, void*)>(image + 0x5fdd60)(desktop, &source, *record); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { error = GetExceptionCode(); }
    if (parsed) { __try { reinterpret_cast<void(*)(void*)>(image + 0x9676a0)(&source); } __except (EXCEPTION_EXECUTE_HANDLER) { if (!error) error = GetExceptionCode(); } }
    if (opened) { __try { reinterpret_cast<void(*)(void*)>(image + 0x962170)(&file); } __except (EXCEPTION_EXECUTE_HANDLER) { if (!error) error = GetExceptionCode(); } }
    if (named) { __try { reinterpret_cast<void(*)(void*)>(image + 0x11bd1e0)(&filename); } __except (EXCEPTION_EXECUTE_HANDLER) { if (!error) error = GetExceptionCode(); } }
    return error;
}
DWORD icon_layout(void* desktop, void* icon) {
    __try { reinterpret_cast<void(*)(void*, void*)>(image + 0x5fe5f0)(desktop, icon); reinterpret_cast<void(*)(void*)>(image + 0x81a330)(desktop); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); } return 0;
}
DWORD icon_text_key(void* pane, const void* key) {
    __try { reinterpret_cast<void(*)(void*, const void*)>(image + 0x7f46e0)(pane, key); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); } return 0;
}
bool icon_identity(void* icon, const Binding& binding) {
    uintptr_t vtable = 0; void* record = nullptr; void* array = nullptr; int count = 0;
    if (!read(icon, 0, vtable) || vtable != image + 0x16fb650 || !read(icon, 0x318, record) || record != binding.record ||
        !read(binding.desktop, 0x4f0, array) || !read(binding.desktop, 0x4f8, count) || count < 1 || count > 4096 || !readable(array, size_t(count) * sizeof(void*))) return false;
    for (int index = 0; index < count; ++index) { void* candidate = nullptr; if (!read(array, size_t(index) * sizeof(void*), candidate)) return false; if (candidate == icon) return true; }
    return false;
}
void* entry(bool title) {
    void* icon = nullptr;
    check(string_call(0x816150, current_desktop, "mt2fw_mods_entry", &icon), "Desktop entry lookup");
    if (!icon) for (const auto& [candidate, binding] : icons) if (binding.desktop == current_desktop && binding.app_owner.empty()) {
        require(icon == nullptr, "Multiple Mods icons in the current Desktop"); icon = candidate;
    }
    if (!icon && title) {
        require(!icon_attempted, "Desktop icon creation already attempted; no replay"); icon_attempted = true;
        void* record = nullptr; bool handed_off = false;
        auto error = make_icon(current_desktop, &icon, &record, &handed_off);
        if (icon) icons.emplace(icon, Binding{current_desktop, record});
        if (error || !icon) {
            if (record && !handed_off) { auto cleanup = delete_record(record); if (cleanup) log_error("Unbound icon record cleanup failed: " + std::to_string(cleanup)); }
            if (record && handed_off && !icon) log_error("Icon creation failed after record handoff; ownership unknown, record retained; no replay");
            check(error, "Desktop icon creation"); throw std::runtime_error("Native Desktop returned no Mods icon; no replay");
        }
        require(icon_identity(icon, icons.at(icon)), "New Desktop icon identity differs");
        void* picture = nullptr; void* title_pane = nullptr; void* begin = nullptr; void* end = nullptr;
        require(read(icon, 0x3c0, picture) && picture && read(icon, 0x3c8, title_pane) && title_pane &&
            read(icon, 0x3a0, begin) && read(icon, 0x3a8, end) && begin == end, "Desktop icon presentation links differ");
        check(string_call(0x11bfce0, static_cast<unsigned char*>(icon) + 0x360, "Mods"), "Icon name");
        check(string_call(0x11bfce0, static_cast<unsigned char*>(icon) + 0x380, "Mods"), "Icon display name");
        check(icon_text_key(title_pane, static_cast<unsigned char*>(icon) + 0x380), "Icon text key");
        check(string_call(0x771f90, picture, "mt2fw_desktop_mods"), "Icon material");
        check(icon_layout(current_desktop, icon), "Native icon placement");
        runtime->log("framework", "info", "Mods app registered on the native game Desktop");
    }
    if (icon) require(icons.contains(icon) && icon_identity(icon, icons.at(icon)), "Desktop entry owner/class/record differs");
    return icon;
}
bool entry_visible(void* icon, bool title) {
    uint8_t shown = 0, dragging = 0; void* parent = nullptr; void* window = nullptr;
    require(read(current_desktop, 0x45f, shown), "Desktop visibility unreadable");
    if (!icon) return false;
    require(read(icon, 0x278, parent) && read(icon, 0x280, window) && read(icon, 0x358, dragging), "Desktop entry ancestry unreadable");
    return title && shown && !dragging && parent == current_desktop && window == current_desktop;
}
void launch(void* icon) {
    auto found = icons.find(icon);
    if (found == icons.end()) { original_launch(icon); return; }
    try {
        require(GetCurrentThreadId() == thread && icon_identity(icon, found->second) && found->second.desktop == current_desktop && entry_visible(icon, mode_index == 2), "Mods icon launch requires the current visible Desktop");
        if (found->second.app_owner.empty()) invoke("MT2FWOpen");
        else {
            auto app = apps.find(found->second.app_owner);
            require(app != apps.end() && app->second.owner == found->second.plugin &&
                app->second.icon == icon && app->second.desktop && app->second.icon_error.empty(),
                "Desktop app registration is no longer active");
            bool active = false;
            for (const auto& plugin : runtime->plugins()) if (plugin.get() == app->second.owner && plugin->active) active = true;
            require(active, "Desktop app mod is inactive");
            if (panel_open) set_open(false);
            app->second.request.active = true;
            app->second.request.launch();
            ++app->second.launches;
        }
    } catch (const std::exception& error) { log_error(error.what()); }
}
void destroy(void* icon) {
    void* record = nullptr;
    if (auto found = icons.find(icon); found != icons.end()) {
        record = found->second.record;
        if (found->second.app_owner.empty()) {
            if (read(record, 0x3c, retained_position)) { position_owner = found->second.desktop; position_valid = true; }
            else log_error("Desktop icon position could not be retained");
            if (found->second.desktop == current_desktop) icon_attempted = false;
        } else if (auto app = apps.find(found->second.app_owner); app != apps.end() && app->second.icon == icon) {
            app->second.position_valid = read(record, 0x3c, app->second.position);
            if (!app->second.position_valid) log_error("Desktop app position could not be retained: " + found->second.app_owner);
            app->second.icon = nullptr; app->second.attempted_desktop = nullptr;
            app->second.icon_confirmed = false; app->second.icon_visible = false;
        }
        icons.erase(found);
    }
    original_destroy(icon);
    if (record) { auto error = delete_record(record); if (error) log_error("Desktop icon record cleanup failed: " + std::to_string(error)); }
}
void sync_app_icons(bool title) {
    size_t slot = 0;
    for (auto& [owner, app] : apps) {
        app.icon_visible.reset();
        bool active = false;
        for (const auto& plugin : runtime->plugins()) if (plugin.get() == app.owner && plugin->active) active = true;
        if (!active) app.request.retire();
        else app.request.active = true;
        if (!app.icon_error.empty()) continue;
        try {
            uint8_t desktop_shown = 0;
            require(read(current_desktop, 0x45f, desktop_shown), "Desktop app visibility unreadable");
            const bool shown = active && app.desktop && title && desktop_shown;
            auto binding = icons.find(app.icon);
            bool current = binding != icons.end() && binding->second.desktop == current_desktop;
            if (!shown) {
                if (current) { require(icon_identity(app.icon, binding->second), "Desktop app retirement identity differs"); native_shown(app.icon, false); }
                app.icon_visible = false;
                continue;
            }
            if (!current) {
                require(app.attempted_desktop != current_desktop, "Desktop app creation already attempted; no replay");
                native_mount();
                auto module = GetModuleHandleW(L"MT2Framework.dll");
                auto bytes = native_app::icon_template(resource(module, desktop_entry_resource), digest(owner));
                auto filename = "app-" + digest(bytes) + ".win";
                native_file(profile / "cache/framework-native" / filename, bytes);
                const auto path = "mt2fw_native/" + filename;
                std::array<float, 2> position = app.position;
                if (!app.position_valid) {
                    float width = 0;
                    require(read(current_desktop, 0x138, width), "Desktop app placement width unreadable");
                    position = {(std::max)(20.0f, width - 240.0f - 120.0f * float(slot % 4)), 180.0f + 100.0f * float(slot / 4)};
                }
                app.attempted_desktop = current_desktop;
                void* icon = nullptr; void* record = nullptr; bool handed_off = false;
                const auto error = make_icon(current_desktop, &icon, &record, &handed_off, path.c_str(), position.data());
                if (icon) { app.icon = icon; icons.emplace(icon, Binding{current_desktop, record, owner, app.owner}); }
                if (error || !icon) {
                    if (record && !handed_off) { auto cleanup = delete_record(record); if (cleanup) log_error("Unbound app icon record cleanup failed: " + std::to_string(cleanup)); }
                    if (record && handed_off && !icon) log_error("App icon failed after record handoff; ownership unknown, record retained; no replay");
                    check(error, "Desktop app icon creation");
                    throw std::runtime_error("Native Desktop returned no app icon; no replay");
                }
                binding = icons.find(icon);
                app.presentation_dirty = true;
            }
            require(binding != icons.end() && icon_identity(app.icon, binding->second), "Desktop app icon identity differs");
            app.icon_confirmed = true;
            binding->second.plugin = app.owner;
            if (app.presentation_dirty) {
                void* picture = nullptr; void* label = nullptr;
                require(read(app.icon, 0x3c0, picture) && picture && read(app.icon, 0x3c8, label) && label,
                    "Desktop app presentation links differ");
                check(string_call(0x11bfce0, static_cast<unsigned char*>(app.icon) + 0x360, app.title.c_str()), "Desktop app name");
                check(string_call(0x11bfce0, static_cast<unsigned char*>(app.icon) + 0x380, app.title.c_str()), "Desktop app display name");
                check(icon_text_key(label, static_cast<unsigned char*>(app.icon) + 0x380), "Desktop app text key");
                auto material = app.image_path.empty() ? "mt2fw_desktop_mods" : app_icon_image(owner, app.image_path);
                check(string_call(0x771f90, picture, material.c_str()), "Desktop app icon material");
                check(icon_layout(current_desktop, app.icon), "Native app icon placement");
                app.icon_material = material; app.presentation_dirty = false;
            }
            native_shown(app.icon, true);
            app.icon_visible = true;
            ++slot;
        } catch (const std::exception& error) {
            app.icon_error = error.what();
            auto binding = icons.find(app.icon);
            if (binding != icons.end() && binding->second.desktop == current_desktop) {
                try {
                    require(icon_identity(app.icon, binding->second), "Failed app icon retirement identity differs");
                    native_shown(app.icon, false); app.icon_visible = false;
                } catch (const std::exception& hide_error) { app.icon_error += "; hide failed: " + std::string(hide_error.what()); }
            }
            runtime->log(owner, "error", "Desktop app: " + app.icon_error);
        }
    }
}
DWORD activate_native(void* target, bool icon) {
    __try {
        if (icon) reinterpret_cast<void(*)(void*, void*, const void*)>(image + 0x609490)(target, nullptr, nullptr);
        else reinterpret_cast<void(*)(void*)>(image + 0x674780)(target);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); } return 0;
}
#include "native_app_close.inc"
#include "native_app_probe.inc"
struct alignas(8) MenuElement { String key; Literal label; };
static_assert(sizeof(MenuElement) == 0x58);
struct MenuArray { void* vtable; void* data; int count, capacity; };
DWORD menu_element(MenuElement* item, bool construct) {
    DWORD error = 0;
    __try {
        if (construct) {
            reinterpret_cast<void(*)(void*, const char*)>(image + 0x49e090)(&item->key, "mt2fw_mods");
            reinterpret_cast<void(*)(void*, const char*)>(image + 0xa16f10)(&item->label, "Mods...");
        } else {
            reinterpret_cast<void(*)(void*)>(image + 0x11bd1e0)(&item->label);
            reinterpret_cast<void(*)(void*)>(image + 0x11bd1e0)(&item->key);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { error = GetExceptionCode(); }
    return error;
}
void menu_elements(void* menu, const void* source, float width) {
    void* owner = nullptr; void* start = nullptr;
    if (!runtime || !mode || GetCurrentThreadId() != thread || !read(menu, 0x280, owner) ||
        window_lookup(&start, "startmenu") || !start || owner != start) {
        original_elements(menu, source, width); return;
    }
    std::vector<MenuElement> elements;
    MenuArray array{}; size_t inserted = 0;
    try {
        require(read(source, 0, array) && array.count > 0 && array.count < 128 &&
            readable(array.data, size_t(array.count) * sizeof(MenuElement)), "Start menu element layout differs");
        inserted = size_t(array.count);
        for (int index = 0; index < array.count; ++index) {
            std::string key;
            require(text(static_cast<unsigned char*>(array.data) + index * sizeof(MenuElement), key), "Start menu key unreadable");
            if (key == "mt2fw_mods") { original_elements(menu, source, width); return; }
            if (key == "options") inserted = size_t(index);
        }
        elements.resize(size_t(array.count) + 1);
        std::memcpy(elements.data(), array.data, inserted * sizeof(MenuElement));
        std::memcpy(elements.data() + inserted + 1, static_cast<unsigned char*>(array.data) + inserted * sizeof(MenuElement),
            (size_t(array.count) - inserted) * sizeof(MenuElement));
        check(menu_element(&elements[inserted], true), "Start menu label construction");
    } catch (const std::exception& error) {
        log_error(error.what()); original_elements(menu, source, width); return;
    }
    // SetElements copies the borrowed elements into game-owned controls synchronously.
    array.data = elements.data(); ++array.count; array.capacity = array.count;
    original_elements(menu, &array, width);
    start_entry_registered = true;
    auto cleanup = menu_element(&elements[inserted], false);
    if (cleanup) log_error("Start menu label cleanup failed: " + std::to_string(cleanup));
}
void menu_select(void* receiver, const void* key) {
    std::string value; void* start = nullptr;
    if (!text(key, value) || value != "mt2fw_mods") { original_select(receiver, key); return; }
    try {
        require(GetCurrentThreadId() == thread && !window_lookup(&start, "startmenu") && start == receiver,
            "Mods menu item belongs to another window or thread");
        check(show(start, false), "Close start menu"); invoke("MT2FWOpen"); ++start_entry_launches;
    } catch (const std::exception& error) { log_error(error.what()); }
}
}

void configure(Runtime& owner, const fs::path& root, Details open_details, std::function<void()> change_language) {
    require(runtime == nullptr, "Desktop UI is already configured");
    runtime = &owner; profile = root; details = std::move(open_details); language = std::move(change_language);
    image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
}
std::vector<Hook> hooks() {
    constexpr unsigned char launch_bytes[]{0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xf0,0,0,0};
    constexpr unsigned char destroy_bytes[]{0x56,0x53,0x48,0x83,0xec,0x28};
    require(readable(reinterpret_cast<void*>(image + 0x609060), sizeof(launch_bytes)) && !std::memcmp(reinterpret_cast<void*>(image + 0x609060), launch_bytes, sizeof(launch_bytes)), "Desktop icon launch entry differs");
    require(readable(reinterpret_cast<void*>(image + 0x6087e0), sizeof(destroy_bytes)) && !std::memcmp(reinterpret_cast<void*>(image + 0x6087e0), destroy_bytes, sizeof(destroy_bytes)), "Desktop icon destructor entry differs");
    return {{reinterpret_cast<void*>(image + 0x609060), reinterpret_cast<void*>(&launch), reinterpret_cast<void**>(&original_launch)},
            {reinterpret_cast<void*>(image + 0x6087e0), reinterpret_cast<void*>(&destroy), reinterpret_cast<void**>(&original_destroy)},
            {reinterpret_cast<void*>(image + 0x73c200), reinterpret_cast<void*>(&menu_elements), reinterpret_cast<void**>(&original_elements)},
            {reinterpret_cast<void*>(image + 0x73d8a0), reinterpret_cast<void*>(&menu_select), reinterpret_cast<void**>(&original_select)}};
}
void frame(bool use_german) {
    if (!fault.empty()) return;
    try {
        german = use_german;
        if (!thread) {
            uintptr_t resolved = 0; uint32_t bound = 0; std::string hash, error;
            require(native::read_resolver_receipt(resolved, bound, hash, error), error);
            require(resolved == image && bound == GetCurrentThreadId() && hash == "bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6", "Native Desktop exact-build/thread receipt differs");
            thread = bound;
        }
        require(thread == GetCurrentThreadId(), "Desktop frame changed game thread");
        native::WorldStatus world; std::string error;
        require(native::read_world_status(world, error), error);
        bool title = world.active_mode_index == 2 && !world.title_transition_pending;
        bool active = title || world.active_mode_index == 3 && native::in_game();
        if (!active) { wizard.reset_epoch(); mode = nullptr; panel = nullptr; mode_index = -1; panel_open = false; labels.clear(); return; }
        if (!assets_ready) mount_assets();
        void* game = nullptr; void* next_mode = nullptr; void* active_mode = nullptr; void* desktop = nullptr; uintptr_t vtable = 0;
        require(read(reinterpret_cast<void*>(image), 0x13df8f0, game) && read(game, title ? 0x350 : 0x358, next_mode) &&
            read(game, 0x28, active_mode) && next_mode && next_mode == active_mode, "Mods active mode owner differs");
        if (mode != next_mode || mode_index != world.active_mode_index) {
            wizard.reset_epoch();
            mode = next_mode; mode_index = world.active_mode_index; panel = nullptr; append_attempted = false; panel_open = false; labels.clear(); dirty = true;
        }
        require(read(game, 0x1e8, desktop) && read(desktop, 0, vtable) && vtable == image + 0x16c02d0, "Native Desktop owner/class differs");
        if (current_desktop != desktop) { current_desktop = desktop; icon_attempted = false; }
        check(window_lookup(&panel), "Window registry lookup");
        if (!panel) {
            require(!append_attempted, "Mods window append already attempted; no replay"); append_attempted = true;
            check(string_call(0xbbe090, static_cast<unsigned char*>(mode) + (title ? 0x40 : 0x98), "mt2fw_ui/mods.win"), "Native Mods window append");
            check(window_lookup(&panel), "New window registry lookup"); require(panel != nullptr, "Native Mods window registration was not acknowledged");
            set_open(false); labels.clear(); dirty = true;
        }
        void* icon = entry(title);
        sync_app_icons(title);
        for (auto& [owner, app] : native_windows) {
            bool active = false;
            for (const auto& plugin : runtime->plugins()) if (plugin.get() == app.owner && plugin->active) active = true;
            if (!active && app.panel && app.mode == mode && app.requested) {
                check(show(app.panel, false), "Hide inactive mod app"); app.requested = false; app.clicked = Json::array();
            }
        }
        uint8_t shown = 0; require(read(panel, 0x45f, shown), "Mods window visibility unreadable"); panel_open = shown != 0;
        if (toggle_pending) { toggle_pending = false; set_open(!panel_open); }
        if (panel_open && (dirty || std::chrono::steady_clock::now() - refreshed > std::chrono::milliseconds(250))) refresh();
        observed = {{"entry_registered", icon != nullptr}, {"entry_visible", entry_visible(icon, title)},
            {"entry_owner", "native_game_desktop"}, {"panel_registered", true}, {"panel_open", panel_open},
            {"mode", mode_index}, {"selected", selected}, {"page", page}, {"commands", commands}, {"last_command", last_command}};
    } catch (const std::exception& error) { fault = error.what(); log_error(fault); throw; }
}
bool command(void* receiver, const void* input) {
    std::string value;
    if (!text(input, value) || !value.starts_with("MT2FW")) return false;
    if (value.starts_with("MT2FWNative ")) {
        try {
            require(receiver == mode && thread == GetCurrentThreadId(), "Native app command owner differs");
            std::istringstream words(value); std::string prefix, name, id; words >> prefix >> name >> id;
            bool handled = false;
            for (auto& [key, window] : native_windows) if (window.name == name && window.mode == mode && window.panel) {
                uint8_t visible = 0; require(read(window.panel, 0x45f, visible) && visible, "Native app command requires an open window");
                const Plugin* owner = nullptr;
                for (const auto& plugin : runtime->plugins()) if (plugin.get() == window.owner && plugin->active) owner = plugin.get();
                require(owner, "Native app mod is no longer active");
                for (auto& widget : window.widgets) if (widget.at("id") == id && widget.at("type") == "button") {
                    uint8_t disabled = 0; require(read(native_pane(window.panel, id), 0x46d, disabled) && !disabled, "Native app button is disabled");
                    require(window.clicked.size() < 16, "Native app event queue is full"); window.clicked.push_back(id); handled = true;
                }
            }
            require(handled, "Native app command is not registered");
        } catch (const std::exception& error) { log_error(error.what()); }
        return true;
    }
    try { require(receiver == mode, "Mods command receiver is not the current mode"); invoke(value); }
    catch (const std::exception& error) { log_error(error.what()); }
    return true;
}
void request_toggle() { toggle_pending = !toggle_pending; }
void end_frame() {
    if (pending_reload.empty()) return;
    require(thread == GetCurrentThreadId(), "Lua reload is outside the bound game thread");
    auto id = std::move(pending_reload); pending_reload.clear();
    if (runtime->reload(id)) notice = std::string(tr("Reloaded: ", "Neu geladen: ")) + id;
    else {
        notice = std::string(tr("Reload failed: ", "Neuladen fehlgeschlagen: ")) + id;
        const auto& logs = runtime->logs();
        if (!logs.empty() && logs.back().mod == id) notice = logs.back().message;
    }
    dirty = true;
}
bool visible() { return panel_open; }
Json status() { auto result = observed; result["error"] = fault; result["notice"] = notice; result["panel_open"] = panel_open; result["commands"] = commands; result["command_errors"] = command_errors; result["last_command"] = last_command; result["selected"] = selected; result["pending_reload"] = pending_reload; result["start_menu_entry"] = start_entry_registered; result["start_menu_launches"] = start_entry_launches; return result; }
Json activate(const Json& args) {
    require(fault.empty(), fault);
    require(thread == GetCurrentThreadId() && mode && panel, "Diagnostic requires current game-thread Mods window");
    const auto id = args.at("target").get<std::string>(); bool icon = id == "icon";
    if (id.starts_with("app-icon:")) return probe_app_icon(id.substr(9), args);
    if (id.starts_with("wizard:")) return probe_wizard(id.substr(7), args);
    if (id.starts_with("app-close:")) return close_native_app(id.substr(10), args);
    if (id.starts_with("app:")) {
        auto split = id.find(':',4); require(split != std::string::npos,"Native app diagnostic target requires its owner and widget");
        auto found = native_windows.find(id.substr(4,split-4)); require(found != native_windows.end(),"Native app is not registered");
        auto& app = found->second; require(app.panel && app.mode == mode,"Native app belongs to another mode");
        auto widget = id.substr(split+1); auto* target = native_pane(app.panel,widget);
        if(args.contains("text")) {
            bool input = false;size_t maximum=128; for(auto& item:app.widgets)if(item.at("id")==widget&&item.at("type")=="input"){input=true;maximum=native_input::limit(item);}
            require(input,"Native app diagnostic text target is not an input");
            auto value=args.at("text").get<std::string>();native_input::validate(value,maximum);
            check(string_call(0x7f1630,target,value.c_str()),"Native app diagnostic text");
            std::string actual;require(text(static_cast<unsigned char*>(target)+0x2d0,actual)&&actual==value,"Native app diagnostic text readback differs");
            return {{"native_event","mmoTextEditPane::SetText"},{"accepted",true},{"readback_matches",true},{"bytes",actual.size()}};
        }
        auto count=app.clicked.size(); check(activate_native(target,false),"Native app button activation");
        require(app.clicked.size()==count+1,"Native app button did not reach its handler");
        return {{"native_event","mmoButtonPane::Activate"},{"handler_reached",true},{"widget",widget}};
    }
    if (id == "start_menu" || id == "start_mods") {
        void* start = nullptr; check(window_lookup(&start, "startmenu"), "Start menu lookup"); require(start, "Start menu unavailable");
        if (id == "start_menu") check(show(start, true), "Show start menu");
        else { MenuElement item{}; check(menu_element(&item, true), "Menu diagnostic key"); menu_select(start, &item.key); check(menu_element(&item, false), "Menu diagnostic cleanup"); }
        return {{"menu_registered", start_entry_registered}, {"panel_open", panel_open}, {"launches", start_entry_launches}};
    }
    void* target = nullptr;
    if (icon) { target = entry(mode_index == 2); require(entry_visible(target, mode_index == 2), "Mods Desktop icon is not visible"); }
    else {
        require(panel_open, "Mods window is not open");
        static const std::set<std::string> buttons{"mod0","mod1","mod2","mod3","mod4","mod5","previous","next","toggle","reload","settings","tools","language","open_app"};
        require(buttons.contains(id), "Unknown native Mods button: " + id); target = pane(id);
        uint8_t is_disabled = 0; require(read(target, 0x46d, is_disabled) && !is_disabled, "Native Mods button is disabled");
    }
    auto before = commands, errors_before = command_errors; check(activate_native(target, icon), "Native Mods activation");
    require(commands == before + 1, "Native Mods activation did not reach its handler");
    require(command_errors == errors_before, "Native Mods command failed: " + notice);
    if (dirty && panel_open) refresh();
    return {{"native_event", icon ? "mmoDesktopIcon::DoubleClick" : "mmoButtonPane::Activate"},
        {"handler_reached", true}, {"command", last_command}, {"panel_open", panel_open}, {"selected", selected}, {"notice", notice}};
}
Json app(const Json& args) {
    auto owner = args.at("mod_id").get<std::string>();
    auto id = args.at("id").get<std::string>(), title = args.at("title").get<std::string>();
    require(!id.empty() && id.size() <= 64 && id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.") == std::string::npos,
        "App ID must contain 1-64 letters, digits, dots, hyphens or underscores");
    require(!title.empty() && title.size() <= 80, "App title must contain 1-80 bytes");
    const Plugin* plugin = nullptr;
    for (const auto& candidate : runtime->plugins()) if (candidate->manifest.id == owner && candidate->active) plugin = candidate.get();
    require(plugin, "App registration requires its active mod");
    const bool desktop = args.value("desktop", false);
    auto icon = args.value("icon", std::string());
    require(icon.empty() || desktop, "Desktop icon requires desktop exposure");
    native_app::icon_kind(icon);
    auto& value = apps[owner];
    if (desktop && !value.desktop) {
        size_t count = 0; for (const auto& [key, item] : apps) if (item.desktop) ++count;
        require(count < 64, "Native Desktop supports at most 64 registered mod apps");
    }
    if (value.owner != plugin || value.id != id) value.request.retire();
    if (value.owner != plugin || value.id != id || value.title != title || value.image_path != icon || value.desktop != desktop) {
        value.owner = plugin; value.id = id; value.title = title; value.image_path = icon; value.desktop = desktop;
        value.presentation_dirty = true; dirty = true;
    }
    value.request.active = true;
    auto binding = icons.find(value.icon);
    const bool registered = value.icon_confirmed && binding != icons.end() && binding->second.desktop == current_desktop;
    const bool may_show = registered && desktop && mode_index == 2;
    const Json visible = !may_show ? Json(false) : value.icon_visible ? Json(*value.icon_visible) : Json(nullptr);
    return {{"open_requested", value.request.consume()}, {"desktop", {{"requested", desktop},
        {"registered", registered}, {"visible", visible}, {"visibility_known", !may_show || value.icon_visible.has_value()},
        {"error", value.icon_error}}}};
}
Json new_world(const Json& args) {
    bool attempted = false;
    try {
        const auto owner = args.at("mod_id").get<std::string>();
        auto app = apps.find(owner);
        require(app != apps.end(), "Register the app before opening New Game");
        bool active = false;
        for (const auto& plugin : runtime->plugins()) if (plugin.get() == app->second.owner && plugin->active) active = true;
        require(active && thread && thread == GetCurrentThreadId(), "New Game requires an active app on the bound game thread");
        auto observe = [&]() {
            native::WorldStatus status; std::string error;
            require(native::read_world_status(status, error), error);
            require(status.active_mode_index == 2 && status.requested_mode_index == 2 && !status.title_transition_pending,
                "New Game requires idle Title with no pending mode transition");
            void* game = nullptr; void* title = nullptr; void* active_mode = nullptr; void* window = nullptr; void* registered = nullptr;
            uintptr_t title_class = 0, window_class = 0; uint8_t visible = 0, pending = 1;
            require(read(reinterpret_cast<void*>(image), 0x13df8f0, game) && read(game, 0x350, title) &&
                read(game, 0x28, active_mode) && title == mode && title == active_mode && read(title, 0, title_class) &&
                title_class == image + 0x16dfbb0 && read(title, 0xd0, pending) && !pending &&
                read(title, 0x20, window) && window && read(window, 0, window_class) && window_class == image + 0x1707fb0,
                "New Game Title/window ownership differs");
            check(window_lookup(&registered, "NewGame"), "New Game window registry");
            require(registered == window && read(window, 0x45f, visible) && visible <= 1,
                "New Game registry or visibility differs");
            return native_app::WizardSnapshot{reinterpret_cast<uintptr_t>(title), reinterpret_cast<uintptr_t>(window), visible != 0};
        };
        if (args.value("observe", false)) {
            const auto state = wizard.inspect(observe);
            return {{"status", "observed"}, {"confirmed", true}, {"attempted", false}, {"visible", state.visible},
                {"mode", 2}, {"window", "NewGame"}, {"route", "mmoNewGameWindow::visibility"}};
        }
        const auto result = wizard.open(observe, [&](bool& called) {
            const auto state = observe(); std::string error;
            const bool ok = native::show_new_game_window(reinterpret_cast<void*>(state.title), reinterpret_cast<void*>(state.window), called, error);
            attempted = called; require(ok, error);
        });
        return {{"status", result == native_app::WizardResult::already_open ? "already_open" : "opened"},
            {"confirmed", true}, {"attempted", attempted}, {"visible", true}, {"mode", 2},
            {"window", "NewGame"}, {"route", "mmoModeTitle::ShowCreateWindow"}};
    } catch (const native_app::WizardFailure& error) {
        runtime->log(args.value("mod_id", std::string("framework")), error.unknown ? "error" : "warn", error.what());
        return {{"status", error.unknown ? "unknown" : "rejected"}, {"confirmed", false}, {"attempted", error.attempted}, {"cause", error.what()}};
    } catch (const std::exception& error) {
        runtime->log(args.value("mod_id", std::string("framework")), "warn", error.what());
        return {{"status", "rejected"}, {"confirmed", false}, {"attempted", false}, {"cause", error.what()}};
    }
}
namespace {
bool native_mounted = false;
void native_file(const fs::path& path, const std::string& bytes) {
    if (fs::exists(path)) { require(file_digest(path) == digest(bytes), "Cached native app asset differs"); return; }
    fs::create_directories(path.parent_path()); std::ofstream out(path, std::ios::binary);
    out.write(bytes.data(), bytes.size()); out.close(); require(bool(out), "Native app asset write failed");
}
void native_mount() {
    if (native_mounted) return;
    auto root = profile / "cache/framework-native"; fs::create_directories(root / "materials");
    auto module = GetModuleHandleW(L"libphysfs.dll");
    auto mount = reinterpret_cast<int(*)(const char*, const char*, int)>(GetProcAddress(module, "PHYSFS_mount"));
    require(mount && mount(path_text(root).c_str(), "mt2fw_native", 1) && mount(path_text(root / "materials").c_str(), "materials", 1),
        "Native app resource mount failed"); native_mounted = true;
}
std::string cached_png(const std::string& owner, const std::string& relative, const fs::path& path, const std::string& expected_hash) {
    auto key = owner + ":" + relative;
    if (auto found = native_images.find(key); found != native_images.end()) return found->second;
    require(native_images.size() < 128, "Native preview session limit reached");
    auto length = fs::file_size(path); require(length > 32 && length <= 1024 * 1024, "Native preview size exceeds 1 MiB");
    std::ifstream stream(path, std::ios::binary); std::string bytes(size_t(length), '\0');
    stream.read(bytes.data(), std::streamsize(bytes.size())); require(bool(stream), "Native preview read failed");
    constexpr unsigned char magic[]{137,80,78,71,13,10,26,10};
    require(!std::memcmp(bytes.data(), magic, 8), "Native preview is not PNG");
    auto number = [&](int offset) { auto* p = reinterpret_cast<const unsigned char*>(bytes.data()) + offset; return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; };
    require(number(16) && number(16) <= 2048 && number(20) && number(20) <= 2048, "Native preview dimensions exceed 2048 pixels");
    auto sha = digest(bytes); require(expected_hash.empty() || expected_hash == sha, "Native preview hash differs");
    auto name = "mt2fw_preview_" + sha;
    auto root = profile / "cache/framework-native";
    native_file(root / (sha + ".png"), bytes);
    native_file(root / "materials" / (name + ".mat"), "Material\n{\nmode normal\ncolor 1 1 1 1\nculling none\nzread false\nzwrite false\nglow false\ntexture \"mt2fw_native/" + sha + ".png\" clampUV\nshader \"default_v.glsl\" \"icon_f.glsl\"\n}\n");
    native_images.emplace(key, name); return name;
}
std::string native_image(const std::string& owner, const std::string& relative) {
    require(relative.starts_with("cache/") && relative.size() == 74 && relative.ends_with(".png") &&
        relative.substr(6, 64).find_first_not_of("0123456789abcdef") == std::string::npos,
        "Native preview requires an immutable cache/<sha256>.png file");
    return cached_png(owner, relative, confined(confined(profile, fs::path("data") / owner), fs::u8path(relative)), relative.substr(6,64));
}
std::string app_icon_image(const std::string& owner, const std::string& relative) {
    if (native_app::icon_kind(relative) == native_app::IconKind::cache) return native_image(owner, relative);
    const Plugin* plugin = nullptr;
    for (const auto& candidate : runtime->plugins()) if (candidate->manifest.id == owner && candidate->active) plugin = candidate.get();
    require(plugin, "Desktop icon requires its active mod package");
    return cached_png(owner, relative, confined(plugin->directory, fs::u8path(relative)), "");
}
}
Json native_window(const Json& args) {
    if (!mode || !thread) return {{"ready", false}, {"open", args.value("open", false)}, {"visible",nullptr},{"visibility_known",false},{"suspended",nullptr},{"clicked", Json::array()}, {"values", Json::object()}};
    require(thread == GetCurrentThreadId(), "Native app window is outside the game thread");
    const auto owner = args.at("mod_id").get<std::string>(), id = args.at("id").get<std::string>();
    auto registration = apps.find(owner); require(registration != apps.end() && registration->second.id == id, "Register the native app before its window");
    const auto& items = args.at("widgets"); require(items.is_array() && !items.empty() && items.size() <= 80, "Native app requires 1-80 widgets");
    Json structure = Json::array(); std::set<std::string> identifiers;
    int width = args.at("width"), height = args.at("height");
    require(width >= 400 && width <= 1100 && height >= 300 && height <= 640, "Native app dimensions are outside the supported range");
    for (auto& item : items) {
        auto name = item.at("id").get<std::string>(), type = item.at("type").get<std::string>();
        require(!name.empty() && name.size() <= 48 && name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") == std::string::npos &&
            !name.ends_with("_label") && identifiers.insert(name).second, "Native app widget ID is invalid or duplicated");
        require(type == "text" || type == "button" || type == "input" || type == "image" || type == "panel", "Unsupported native app widget");
        int x = item.at("x"), y = item.at("y"), w = item.at("width"), h = item.at("height"), size = item.value("size", 16);
        require(x >= 0 && y >= 0 && w > 0 && h > 0 && x + w <= width && y + h <= height && size >= 10 && size <= 30, "Native app widget is outside its window");
        for (auto field : {"text", "value"}) if (item.contains(field)) require(item[field].is_string() && item[field].get_ref<const std::string&>().size() <= 1024, "Native app text exceeds 1024 bytes");
        for (auto field : {"visible", "disabled", "selected"}) if (item.contains(field)) require(item[field].is_boolean(), "Invalid native app widget state");
        auto style = item.value("style",std::string("button")), tone = item.value("tone",std::string());
        size_t maximum=type=="input"?native_input::limit(item):0;
        if(type=="input")native_input::validate(item.value("value",std::string()),maximum);
        require(style == "button" || style == "tab" || style == "row" || style == "quiet", "Unsupported native app widget style");
        require(tone.empty() || tone == "muted" || tone == "error" || tone == "subtle" || tone == "preview", "Unsupported native app widget tone");
        structure.push_back({{"id",name},{"type",type},{"x",x},{"y",y},{"width",w},{"height",h},{"size",size},{"tone",tone},{"style",style},{"max_bytes",maximum}});
    }
    auto shape = digest(Json{{"format",5},{"widgets",structure},{"width",width},{"height",height},{"title",registration->second.title}}.dump());
    auto& window = native_windows[owner];
    if (window.owner != registration->second.owner) {
        if (window.panel && window.mode == mode) check(show(window.panel, false), "Hide previous mod instance window");
        window = {}; window.owner = registration->second.owner;
    }
    require(window.shape.empty() || window.shape == shape, "Native app layout changed; restart required");
    if (window.shape.empty()) {
        native_mount(); window.shape = shape; window.name = "mt2fw_app_" + digest(owner + id + std::to_string(++native_instances)).substr(0,20); window.widgets = structure;
        std::ostringstream source;
        source << "mmoWindow { id \"" << window.name << "\"; dims " << width << ' ' << height
            << "; anchorType Window; anchorPos 80 35; title " << Json(registration->second.title).dump()
            << "; material \"PanelBacking\"; corners \"BottomRight\"; hideOnEscape true; transitionType rez; rezSpeed fast; content {\n";
        for (auto& item : structure) {
            auto type = item.at("type").get<std::string>(), name = item.at("id").get<std::string>();
            int x=item.at("x"), y=item.at("y"), w=item.at("width"), h=item.at("height"), size=item.at("size");
            const char* kind = type == "button" ? "mmoButtonPane" : type == "input" ? "mmoTextEditPane" : type == "text" ? "mmoTextPane" : "mmoPane";
            source << kind << " { id \"" << name << "\"; anchorType TopLeft; anchorPos " << x << ' ' << y << "; dims " << w << ' ' << h << "; border false; ";
            if(type == "button") source << "text \"\"; clickable true; border true; edges All; corners \"TopLeft BottomRight\"; cornerSize 5; command \"MessageTo Mode MT2FWNative " << window.name << ' ' << name << "\";";
            else if(type == "input") source << "text \"\"; size " << size << "; clickable true; editable true; maxLength " << item.at("max_bytes").get<size_t>() << "; margin 8; material \"DarkPanelBacking\"; border true; edges All; justify left;";
            else if(type == "text") source << "text \"\"; size " << size << "; clickable false; justify topleft; textColor " << (item.at("tone") == "muted" ? "0.7 0.8 0.88 1" : item.at("tone") == "error" ? "1 0.65 0.4 1" : "0.95 0.97 1 1") << ';';
            else source << "clickable false; material \"" << (type == "panel" ? (item.at("tone") == "subtle" ? "DarkPanelBacking" : "PanelBacking") : "") << "\";";
            source << " }\n";
            if(type == "button") source << "mmoTextPane { id \"" << name << "_label\"; anchorType TopLeft; anchorPos " << x+10 << ' ' << y << "; dims " << w-20 << ' ' << h << "; text \"\"; size " << size << "; justify left; clickable false; border false; }\n";
        }
        source << "} }\n";
        std::string formatted; bool quoted = false, escaped = false;
        for(char c : source.str()) {
            if(c == '"' && !escaped) quoted = !quoted;
            if(!quoted && c == '{') formatted += '\n';
            formatted += c;
            if(!quoted && (c == ';' || c == '{' || c == '}')) formatted += '\n';
            escaped = c == '\\' && !escaped;
        }
        window.path = digest(formatted) + ".win";
        native_file(profile / "cache/framework-native" / window.path, formatted);
    }
    if (window.mode != mode) { window.mode = mode; window.panel = nullptr; window.values = Json::object(); window.clicked = Json::array(); window.requested = false; window.suspended = false; }
    if (window.panel) {
        void* registered = nullptr; check(window_lookup(&registered,window.name.c_str()), "Native app owner lookup");
        if (registered != window.panel) { window.panel = registered; window.values = Json::object(); window.requested = false; window.clicked = Json::array(); }
    }
    bool desired = args.value("open", false);
    if (!window.panel && desired) {
        check(string_call(0xbbe090, static_cast<unsigned char*>(mode) + (mode_index == 2 ? 0x40 : 0x98), ("mt2fw_native/" + window.path).c_str()), "Append native app window");
        check(window_lookup(&window.panel, window.name.c_str()), "Native app registry lookup"); require(window.panel, "Native app window registration not acknowledged");
        check(show(window.panel, false), "Initialize native app visibility");
        for (auto& item : window.widgets) if (item.at("type") == "button" && item.at("style") != "button") {
            auto name = item.at("id").get<std::string>(); auto* target = native_pane(window.panel,name);
            const char* normal = item.at("style") == "tab" ? "TabNormalBackground" : "PanelButtonBackground";
            check(string_call(0x670ef0,target,normal), "Native app button material");
            check(string_call(0x671030,target,"ButtonBackgroundActive"), "Native app selected material");
        }
    }
    Json events{{"ready", true},{"open",desired},{"visible",nullptr},{"visibility_known",false},{"suspended",window.suspended},{"clicked",Json::array()},{"values",Json::object()}};
    if (!window.panel) return events;
    if (!desired || panel_open) {
        if (window.requested || panel_open) check(show(window.panel, false), "Hide native app");
        window.requested = desired; window.suspended = desired && panel_open; events["open"] = desired;
        uint8_t actual=0;require(read(window.panel,0x45f,actual),"Native app hidden visibility unreadable");events["visible"]=actual!=0;events["visibility_known"]=true;events["suspended"]=window.suspended;return events;
    }
    if (!window.requested || window.suspended) check(show(window.panel, true), "Open native app");
    window.requested = true; window.suspended = false;
    uint8_t visible = 0; require(read(window.panel, 0x45f, visible), "Native app visibility unreadable"); events["open"] = visible != 0;
    events["visible"]=visible!=0;events["visibility_known"]=true;events["suspended"]=false;
    if (!visible) {
        window.requested = false;
        window.clicked = Json::array();
        return events;
    }
    for (auto& item : items) {
        auto name = item.at("id").get<std::string>(), type = item.at("type").get<std::string>(); auto* target = native_pane(window.panel, name);
        bool shown = item.value("visible", true); native_shown(target, shown);
        if(type == "button") native_shown(native_pane(window.panel,name+"_label"), shown);
        if(!shown) continue;
        if(type == "button") { check(disable(target,item.value("disabled",false)),"Native app button state"); check(toggle_row(target,item.value("selected",false)),"Native app selection"); }
        if(type == "text" || type == "button") {
            auto value = item.value("text",std::string());
            if(!window.values.contains(name) || window.values[name] != value) { check(text_label(type == "button" ? native_pane(window.panel,name+"_label") : target,value.c_str()),"Native app text"); window.values[name] = value; }
        } else if(type == "input") {
            auto value = item.value("value",std::string());
            if(!window.values.contains(name) || window.values[name] != value) { check(string_call(0x7f1630,target,value.c_str()),"Native app input value"); window.values[name] = value; }
            std::string actual; require(text(static_cast<unsigned char*>(target)+0x2d0,actual) && actual.size() <= 4096,"Native app input unreadable");
            auto maximum=native_input::limit(item);auto bounded=native_input::prefix(actual,maximum);
            if(bounded!=actual){check(string_call(0x7f1630,target,bounded.c_str()),"Native app input byte limit");std::string readback;require(text(static_cast<unsigned char*>(target)+0x2d0,readback)&&readback==bounded,"Native app bounded input readback differs");actual=std::move(bounded);}
            if(actual != value) { events["values"][name] = actual; window.values[name] = actual; }
        } else if(type == "image") {
            auto value = item.value("path",std::string());
            if(!value.empty() && (!window.values.contains(name) || window.values[name] != value)) { check(string_call(0x771f90,target,native_image(owner,value).c_str()),"Native app image material"); window.values[name] = value; }
        }
    }
    events["clicked"] = std::move(window.clicked); window.clicked = Json::array(); return events;
}
}
