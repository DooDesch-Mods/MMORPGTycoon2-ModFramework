#include "world_markers.h"
#include "world_marker_state.h"
#include "native_extensions.h"
#include "crypto.h"
#include <windows.h>
#include <mutex>
#include <cstring>
#include <optional>

namespace mt2fw::mt2::world_markers {
namespace {
namespace native = mt2mp::game::native;
Runtime* host = nullptr;
uintptr_t image = 0;
DWORD game_thread = 0;
std::recursive_mutex mutex;
markers::Store store;
bool poisoned = false, drawing = false;
using Draw = void(*)(void*, void*, void*, int);
Draw original_draw = nullptr;
struct Resource { void* arrow = nullptr; void* label = nullptr; std::string name; bool arrow_ready = false, label_ready = false, uncertain = false; };
std::map<markers::Key, Resource> resources;
struct Vec2 { float x, y; };
struct Projection { Vec2 point{}; float width = 0, height = 0; bool visible = false; };
struct alignas(8) Literal { unsigned char string[32]{}; void* arguments[3]{}; };
struct Pin { uintptr_t rva; size_t size; const char* sha; };
constexpr Pin pins[] = {
#include "world_marker_bindings.inc"
};
bool readable(const void* address, size_t size) {
    MEMORY_BASIC_INFORMATION region{};
    if (!address || !VirtualQuery(address, &region, sizeof(region)) || region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto offset = uintptr_t(address) - uintptr_t(region.BaseAddress);
    return offset <= region.RegionSize && size <= region.RegionSize - offset;
}
template<class T> T read(const void* address, size_t offset = 0) {
    auto field = static_cast<const unsigned char*>(address) + offset;
    markers::require(readable(field, sizeof(T)), "World marker native field is unreadable");
    T result; std::memcpy(&result, field, sizeof(result)); return result;
}
void write(void* address, size_t offset, const void* bytes, size_t count) {
    auto field = static_cast<unsigned char*>(address) + offset;
    markers::require(readable(field, count), "World marker owned field is unreadable");
    std::memcpy(field, bytes, count);
}
void fail(const std::string& cause) noexcept {
    if (!poisoned) {
        poisoned = true;
        try {
            if (host) host->log("framework", "error", "Native world markers stopped: " + cause);
            else { OutputDebugStringA("error: Native world markers stopped: "); OutputDebugStringA(cause.c_str()); }
        } catch (const std::exception& logging) {
            OutputDebugStringA("error: Native world markers stopped; log write failed: "); OutputDebugStringA(logging.what()); OutputDebugStringA(cause.c_str());
        } catch (...) { OutputDebugStringA("error: Native world markers stopped; log write raised an unknown exception: "); OutputDebugStringA(cause.c_str()); }
    }
}
void check(DWORD exception, const char* operation) {
    if (exception) throw std::runtime_error(std::string(operation) + " exception=" + std::to_string(exception) + "; native outcome unknown, restart required");
}
DWORD create_arrow(Resource* resource) {
    __try {
        resource->arrow = reinterpret_cast<void*(*)(size_t)>(image + 0xa02670)(0x1d8);
        if (!resource->arrow) return ERROR_NOT_ENOUGH_MEMORY;
        reinterpret_cast<void(*)(void*, int)>(image + 0x1fd330)(resource->arrow, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
DWORD create_label(Resource* resource, const char* name) {
    Literal literal{}; bool constructed = false; DWORD exception = 0;
    __try {
        reinterpret_cast<void(*)(void*, const char*)>(image + 0xa16f10)(&literal, name); constructed = true;
        resource->label = reinterpret_cast<void*(*)(size_t)>(image + 0xa02670)(0x1b8);
        if (!resource->label) exception = ERROR_NOT_ENOUGH_MEMORY;
        else reinterpret_cast<void(*)(void*, const void*)>(image + 0x632980)(resource->label, &literal);
    } __except (EXCEPTION_EXECUTE_HANDLER) { exception = GetExceptionCode(); }
    if (constructed) {
        __try {
            reinterpret_cast<void(*)(void*)>(image + 0x11b2b00)(literal.arguments);
            reinterpret_cast<void(*)(void*)>(image + 0x11bd1e0)(&literal);
        } __except (EXCEPTION_EXECUTE_HANDLER) { if (!exception) exception = GetExceptionCode(); }
    }
    return exception;
}
DWORD erase_object(void* object, bool label) {
    __try { reinterpret_cast<void(*)(void*)>(image + (label ? 0x632510 : 0x1fb930))(object); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
DWORD extract_object(void* object) {
    __try { reinterpret_cast<void(*)(void*)>(image + 0x97b270)(object); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
DWORD register_object(void* object) {
    __try { reinterpret_cast<void(*)(void*, int)>(image + 0x97a9d0)(object, 12); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
DWORD update_positions(Resource* resource, const Vec2* point) {
    Vec2 label{point->x + 22.f, point->y + 28.f};
    __try {
        reinterpret_cast<void(*)(void*, const void*)>(image + 0xceec60)(resource->arrow, point);
        reinterpret_cast<void(*)(void*, const void*)>(image + 0x631fe0)(resource->label, &label);
        reinterpret_cast<void(*)(void*, float)>(image + 0x632650)(resource->label, 0.f);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
DWORD native_projection(const std::array<float, 3>* world, Vec2* output, bool* accepted) {
    __try { *accepted = reinterpret_cast<bool(*)(void*, const void*)>(image + 0x864d60)(output, world->data()); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
void* ui_scene() {
    auto slot = read<void*>(reinterpret_cast<void*>(image + 0x164b420));
    auto screen = read<void*>(slot);
    markers::require(read<int>(screen, 0x30) > 12, "Native UI scene 12 is unavailable");
    auto scenes = read<void*>(screen, 0x10);
    auto result = read<void*>(scenes, 12 * sizeof(void*));
    markers::require(result != nullptr, "Native UI scene 12 is null");
    return result;
}
Projection project(const std::array<float, 3>& world) {
    auto scene = ui_scene(); auto camera = read<void*>(scene, 0x38);
    Projection result; result.height = read<float>(camera, 0x68);
    result.width = result.height * read<float>(camera, 0x6c);
    markers::require(std::isfinite(result.height) && std::isfinite(result.width) && result.height > 0 && result.width > 0,
                     "Native UI projection dimensions are invalid");
    bool accepted = false; auto exception = native_projection(&world, &result.point, &accepted);
    if (exception) fail("Native world projection exception=" + std::to_string(exception) + "; restart required");
    check(exception, "Native world projection");
    markers::require(!accepted || (std::isfinite(result.point.x) && std::isfinite(result.point.y)), "Native UI projection returned a nonfinite position");
    result.visible = accepted && result.point.x >= 0 && result.point.y >= 0 && result.point.x <= result.width && result.point.y <= result.height;
    return result;
}
void context() {
    uintptr_t module = 0; uint32_t thread = 0; std::string digest, cause;
    if (!native::read_resolver_receipt(module, thread, digest, cause)) throw std::runtime_error("World marker game-thread check failed: " + cause);
    markers::require(module == image && thread == GetCurrentThreadId(), "World marker resolver identity differs");
    markers::require(!game_thread || game_thread == thread, "World marker game-thread identity changed");
    game_thread = thread;
    const auto state = mt2::request("world.status", Json::object());
    store.context(state.at("epoch").get<uint64_t>(), state.at("loaded").get<bool>() && native::in_game());
}
void delete_resource(Resource& resource) {
    markers::require(!resource.uncertain, "Native marker lifetime is unknown; restart required");
    if (resource.label) { resource.uncertain = true; check(erase_object(resource.label, true), "Native marker label destruction"); resource.label = nullptr; resource.label_ready = false; resource.uncertain = false; }
    if (resource.arrow) { resource.uncertain = true; check(erase_object(resource.arrow, false), "Native marker arrow destruction"); resource.arrow = nullptr; resource.arrow_ready = false; resource.uncertain = false; }
}
void synchronize() {
    for (auto i = resources.begin(); i != resources.end();) {
        if (!store.entries.contains(i->first)) { delete_resource(i->second); i = resources.erase(i); } else ++i;
    }
}
void passive(void* object, uintptr_t vtable) {
    markers::require(read<uintptr_t>(object) == image + vtable, "Native marker constructor vtable differs");
    const unsigned char no = 0; write(object, 0x52, &no, 1);
    markers::require(read<void*>(object, 0x48) == nullptr, "Native marker constructor attached an unexpected scene");
}
void appearance(void* object, const std::array<float, 4>& color) {
    write(object, 0x90, color.data(), sizeof(color));
    const unsigned char yes = 1; write(object, 0xb0, &yes, 1);
}
bool detach_all() noexcept {
    bool clean = true;
    for (auto& [_, resource] : resources) {
        if (resource.uncertain) { clean = false; continue; }
        for (void* object : {resource.arrow_ready ? resource.arrow : nullptr, resource.label_ready ? resource.label : nullptr}) if (object) {
        try {
            if (read<void*>(object, 0x48)) {
                check(extract_object(object), "Native marker extraction");
                markers::require(read<void*>(object, 0x48) == nullptr, "Native marker extraction did not acknowledge detachment");
            }
        } catch (const std::exception& error) { clean = false; fail(error.what()); }
        }
    }
    return clean;
}
void prepare_draw(void* scene) {
    context(); synchronize();
    if (!store.ready || scene != ui_scene() || !read<unsigned char>(scene, 0xb1)) return;
    for (auto& [key, entry] : store.entries) {
        store.presented(key, false);
        if (!entry.enabled) continue;
        auto projected = project(entry.world); if (!projected.visible) continue;
        auto& resource = resources[key];
        if (!resource.arrow) { resource.uncertain = true; check(create_arrow(&resource), "Native marker arrow creation"); resource.arrow_ready = true; resource.uncertain = false; passive(resource.arrow, 0x16c1fb0); }
        if (resource.label && resource.name != entry.name) {
            resource.uncertain = true; check(erase_object(resource.label, true), "Native marker label replacement"); resource.label = nullptr; resource.label_ready = false; resource.uncertain = false;
        }
        if (!resource.label) { resource.uncertain = true; check(create_label(&resource, entry.name.c_str()), "Native marker label creation"); resource.label_ready = true; resource.uncertain = false; passive(resource.label, 0x16db040); resource.name = entry.name; }
        appearance(resource.arrow, entry.color);
        check(update_positions(&resource, &projected.point), "Native marker positioning");
        check(register_object(resource.arrow), "Native marker arrow registration");
        check(register_object(resource.label), "Native marker label registration");
        markers::require(read<void*>(resource.arrow, 0x48) == scene && read<void*>(resource.label, 0x48) == scene,
                         "Native marker scene registration did not acknowledge the UI scene");
    }
}
void draw(void* scene, void* list, void* target, int flags) {
    std::lock_guard lock(mutex);
    // Inactive overlays leave the original scene path untouched.
    if (poisoned || drawing || store.entries.empty() || !game_thread || game_thread != GetCurrentThreadId()) {
        original_draw(scene, list, target, flags); return;
    }
    drawing = true;
    auto prepare = [&] {
        try { prepare_draw(scene); } catch (const std::exception& error) { fail(error.what()); detach_all(); }
        catch (...) { fail("Unknown native marker preparation exception; restart required"); detach_all(); }
    };
    auto cleanup = [&]() noexcept { detach_all(); drawing = false; };
    markers::transient_draw(prepare, [&] {
        original_draw(scene, list, target, flags);
        try {
            if (!poisoned) for (const auto& [key, resource] : resources)
                if (resource.arrow && read<void*>(resource.arrow, 0x48) == scene) store.presented(key, true);
        } catch (const std::exception& error) { fail(error.what()); }
    }, cleanup);
}
}
void configure(Runtime& runtime) {
    std::lock_guard lock(mutex);
    markers::require(!host, "Native world marker service is already configured");
    image = uintptr_t(GetModuleHandleW(nullptr));
    for (const auto& pin : pins) {
        const auto code = reinterpret_cast<const char*>(image + pin.rva);
        markers::require(readable(code, pin.size), "Native world marker binding range is unavailable");
        if (digest(std::string(code, pin.size)) != pin.sha) throw std::runtime_error("Native world marker binding body differs at RVA=" + std::to_string(pin.rva));
    }
    host = &runtime;
}
std::vector<Hook> hooks() { return {{reinterpret_cast<void*>(image + 0x9c9070), reinterpret_cast<void*>(draw), reinterpret_cast<void**>(&original_draw)}}; }
Json request(const std::string& operation, const Json& arguments) {
    std::lock_guard lock(mutex);
    markers::require(host != nullptr && !poisoned, "Native world markers unavailable; check Framework log and restart");
    context();
    if (operation == "ui.world_project") {
        store.validate_epoch(arguments);
        const auto result = project(markers::vector<3>(arguments.at("world")));
        return {{"visible", result.visible}, {"x", result.point.x}, {"y", result.point.y},
                {"width", result.width}, {"height", result.height}, {"space", "mt2_ui"}, {"epoch", std::to_string(store.epoch)}};
    }
    auto result = store.request(arguments.at("mod_id").get<std::string>(), arguments);
    try { synchronize(); } catch (const std::exception& error) { fail(error.what()); throw; }
    return result;
}
void frame() {
    std::lock_guard lock(mutex);
    if (!host || poisoned) return;
    try { context(); synchronize(); } catch (const std::exception& error) { fail(error.what()); }
}
bool retire(Plugin& owner) {
    std::lock_guard lock(mutex);
    store.retire(owner.manifest.id);
    bool has_resource = false; for (const auto& [key, _] : resources) has_resource |= key.first == owner.manifest.id;
    if (!has_resource) return true;
    if (poisoned || drawing || game_thread != GetCurrentThreadId()) {
        host->log(owner.manifest.id, "error", "Native marker retirement requires its bound game thread and known native state; runtime retained"); return false;
    }
    try { synchronize(); return true; } catch (const std::exception& error) { fail(error.what()); return false; }
}
bool shutdown() {
    std::lock_guard lock(mutex);
    store.entries.clear();
    if (!resources.empty() && (poisoned || drawing || game_thread != GetCurrentThreadId())) return false;
    try { synchronize(); host = nullptr; return true; } catch (const std::exception& error) { fail(error.what()); return false; }
}
}
