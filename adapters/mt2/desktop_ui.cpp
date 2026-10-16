#include "desktop_ui.h"
#include "native_extensions.h"
#include "crypto.h"
#include <windows.h>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>

namespace mt2fw::mt2::desktop {
namespace {
namespace native = mt2mp::game::native;
Runtime* runtime = nullptr;
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
struct Binding { void* desktop; void* record; };
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
DWORD window_lookup(void** output) {
    String input{}; bool constructed = false; DWORD error = 0;
    __try {
        reinterpret_cast<void(*)(void*, const char*)>(image + 0x49e090)(&input, "mt2fw_mods"); constructed = true;
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
void label(const std::string& id, const std::string& value) {
    if (auto found = labels.find(id); found != labels.end() && found->second == value) return;
    static const std::set<std::string> buttons{"mod0","mod1","mod2","mod3","mod4","mod5","previous","next","toggle","reload","settings","tools","language"};
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
    std::map<std::string, std::string> files{{"mods.png", resource(module, 201)}, {"entry.win", resource(module, 202)},
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
DWORD make_icon(void* desktop, void** output, void** record, bool* handed_off) {
    String filename{}; RecordStorage file{}, source{};
    bool named = false, opened = false, parsed = false; DWORD error = 0;
    __try {
        *record = reinterpret_cast<void*(*)(void*)>(image + 0xd8f2a0)(nullptr);
        if (!*record) return ERROR_NOT_ENOUGH_MEMORY;
        int shell = 6; std::memcpy(static_cast<unsigned char*>(*record) + 0x38, &shell, sizeof(shell));
        float width = 0; std::memcpy(&width, static_cast<unsigned char*>(desktop) + 0x138, sizeof(width));
        float position[2] = {width > 120 ? width - 120 : 20, 180};
        if (position_valid && position_owner == desktop) std::memcpy(position, retained_position.data(), sizeof(position));
        std::memcpy(static_cast<unsigned char*>(*record) + 0x3c, position, sizeof(position));
        reinterpret_cast<void(*)(void*, const char*)>(image + 0x49e090)(&filename, "mt2fw_ui/entry.win"); named = true;
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
    if (!icon) for (const auto& [candidate, binding] : icons) if (binding.desktop == current_desktop) {
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
        invoke("MT2FWOpen");
    } catch (const std::exception& error) { log_error(error.what()); }
}
void destroy(void* icon) {
    void* record = nullptr;
    if (auto found = icons.find(icon); found != icons.end()) {
        record = found->second.record;
        if (read(record, 0x3c, retained_position)) { position_owner = found->second.desktop; position_valid = true; }
        else log_error("Desktop icon position could not be retained");
        if (found->second.desktop == current_desktop) icon_attempted = false;
        icons.erase(found);
    }
    original_destroy(icon);
    if (record) { auto error = delete_record(record); if (error) log_error("Desktop icon record cleanup failed: " + std::to_string(error)); }
}
DWORD activate_native(void* target, bool icon) {
    __try {
        if (icon) reinterpret_cast<void(*)(void*, void*, const void*)>(image + 0x609490)(target, nullptr, nullptr);
        else reinterpret_cast<void(*)(void*)>(image + 0x674780)(target);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); } return 0;
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
            {reinterpret_cast<void*>(image + 0x6087e0), reinterpret_cast<void*>(&destroy), reinterpret_cast<void**>(&original_destroy)}};
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
        if (!active) { mode = nullptr; panel = nullptr; mode_index = -1; panel_open = false; labels.clear(); return; }
        if (!assets_ready) mount_assets();
        void* game = nullptr; void* next_mode = nullptr; void* active_mode = nullptr; void* desktop = nullptr; uintptr_t vtable = 0;
        require(read(reinterpret_cast<void*>(image), 0x13df8f0, game) && read(game, title ? 0x350 : 0x358, next_mode) &&
            read(game, 0x28, active_mode) && next_mode && next_mode == active_mode, "Mods active mode owner differs");
        if (mode != next_mode || mode_index != world.active_mode_index) {
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
Json status() { auto result = observed; result["error"] = fault; result["notice"] = notice; result["panel_open"] = panel_open; result["commands"] = commands; result["command_errors"] = command_errors; result["last_command"] = last_command; result["selected"] = selected; result["pending_reload"] = pending_reload; return result; }
Json activate(const Json& args) {
    require(fault.empty(), fault);
    require(thread == GetCurrentThreadId() && mode && panel, "Diagnostic requires current game-thread Mods window");
    const auto id = args.at("target").get<std::string>(); bool icon = id == "icon";
    void* target = nullptr;
    if (icon) { target = entry(mode_index == 2); require(entry_visible(target, mode_index == 2), "Mods Desktop icon is not visible"); }
    else {
        require(panel_open, "Mods window is not open");
        static const std::set<std::string> buttons{"mod0","mod1","mod2","mod3","mod4","mod5","previous","next","toggle","reload","settings","tools","language"};
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
}
