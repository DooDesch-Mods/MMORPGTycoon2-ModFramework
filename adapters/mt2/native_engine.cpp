// Copyright (c) 2026 DooDesch. Extracted adapter licensed under the root LICENSE.
#include "mt2mp/game/native_engine.h"
#include "mt2mp/net/snapshot_world_path.h"
#include "mt2mp/core/logger.h"
#include "native_extensions.h"
#include "shared_operations.h"
#include <intrin.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "live_host_state/memory_query_timing.h"
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace mt2mp::game::native {
namespace {
constexpr char expected_hash[] = "bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
std::string resolved_executable_sha256;
constexpr uintptr_t clock_slot = 0x13df9a0;
constexpr uintptr_t mode_slot = 0x13df320;
constexpr uintptr_t game_slot = 0x13df8f0;
constexpr uintptr_t string_construct = 0x49e090;
constexpr uintptr_t string_dispose = 0x11bd1e0;
constexpr uintptr_t save_absolute = 0x4a0570;
constexpr uintptr_t time_scale = 0x259d70;
constexpr uintptr_t title_command = 0x4b9be0;
constexpr uintptr_t pane_command = 0x770300;
constexpr uintptr_t software_slot = 0x13df500;
constexpr uintptr_t subscriber_manager_slot = 0x13df690;
struct AtomicDayTickCounters {
    std::atomic<uint64_t> entered{0}, forwarded{0}, accepted{0};
    std::atomic<uint64_t> receiver_guard_rejected{0}, wrong_thread{0}, deferred_logs{0};
    std::atomic<DayTickGuard> first_rejection{DayTickGuard::Accepted}, last_guard{DayTickGuard::Accepted};
    std::atomic<int32_t> last_valid_day{-1}, last_valid_subscriber_id{-1};
    std::array<std::atomic<uint64_t>, static_cast<size_t>(DayTickGuard::Count)> guards{};
    std::atomic<uint64_t> logged_reasons{0}, next_transition_log_ms{0};
};
std::array<AtomicDayTickCounters, static_cast<size_t>(DayTickLeaf::Count)> day_tick_counters{};
std::atomic<uint64_t> invalid_day_observe_calls{0}, invalid_day_forward_calls{0};
struct AtomicSoftwareTickCounters {
    std::atomic<uint64_t> entered{0}, forwarded{0};
    std::array<std::atomic<uint64_t>, static_cast<size_t>(SoftwareTickGuard::Count)> guards{};
};
std::array<AtomicSoftwareTickCounters, 2> software_tick_counters{};
struct AtomicSoftwareCalculationCounters {
    std::atomic<uint64_t> entered{0}, forwarded{0}, suppressed{0}, guard_rejected{0};
};
std::array<AtomicSoftwareCalculationCounters, 2> software_calculation_counters{};
// Policy and object identities are accessed only on the bound native game thread.
struct SoftwareReplica {
    bool armed = false, bound = false;
    uint64_t epoch = 0, revision = 0;
    void* world = nullptr;
    void* software = nullptr;
    SoftwareCache initial{}, applied{};
    std::string world_folder;
    std::string error;
};
SoftwareReplica software_replica;
struct AtomicNeedsWrites {
    std::atomic<uint64_t> entered{0}, forwarded{0}, selected{0}, suppressed{0}, permitted{0};
    std::atomic<uint64_t> guard_rejected{0}, wrong_thread{0};
};
struct AtomicNeedsTicks { std::atomic<uint64_t> entered{0}, forwarded{0}, guard_rejected{0}; };
std::array<AtomicNeedsWrites, 4> needs_write_counters{};
std::array<AtomicNeedsTicks, 2> needs_tick_counters{};
std::atomic<uint32_t> needs_hook_mask{0};
std::atomic<void*> needs_target{nullptr}, needs_brain_target{nullptr};
std::atomic<bool> needs_wrong_thread{false};
struct NeedsReplica {
    bool armed = false, permit = false;
    uint64_t epoch = 0, revision = 0;
    int32_t id = -1, permit_index = -1;
    float permit_value = 0;
    uint32_t permit_hits = 0;
    void* world = nullptr;
    void* manager = nullptr;
    void* subscriber = nullptr;
    void* brain = nullptr;
    void* needs = nullptr;
    SubscriberNeeds initial{}, applied{}, applied_before{};
    std::string error;
};
NeedsReplica needs_replica;
std::atomic<uintptr_t> image{0};
std::atomic<DWORD> game_thread{0};
bool object_hook_entries_verified=false;
std::atomic<bool> world_available{false};
std::atomic<bool> title_available{false};
std::atomic<bool> island_lock_unknown{false};

bool fail(std::string& error, const std::string& message) {
    error = message;
    LOG_ERROR("[Native] {}", message);
    return false;
}

bool readable(const void* pointer, size_t bytes) {
    if (!pointer || !bytes) return false;
    MEMORY_BASIC_INFORMATION memory{};
    if (!mt2mp::game::live_host_state::apply_timing::virtual_query(pointer, &memory, sizeof(memory)) || memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto address = reinterpret_cast<uintptr_t>(pointer);
    const auto start = reinterpret_cast<uintptr_t>(memory.BaseAddress);
    return address >= start && bytes <= memory.RegionSize && address-start <= memory.RegionSize-bytes;
}

template<class T> bool read_at(const void* object, size_t offset, T& value) {
    if (!object) return false;
    const auto* address = static_cast<const unsigned char*>(object) + offset;
    if (!readable(address, sizeof(T))) return false;
    std::memcpy(&value, address, sizeof(T));
    return true;
}

bool has_vtable(const void* object, uintptr_t table) {
    uintptr_t actual = 0;
    return read_at(object, 0, actual) && actual == image.load() + table + 0x10;
}

void* singleton(uintptr_t rva) {
    void* result = nullptr;
    const auto base = image.load();
    if (base) read_at(reinterpret_cast<void*>(base), rva, result);
    return result;
}

bool check_thread(std::string& error) {
    if (!image.load()) return fail(error, "engine operation rejected: exact-build resolver is not ready");
    if (!game_thread.load() || game_thread.load() != GetCurrentThreadId())
        return fail(error, "engine operation rejected: caller is not the bound game thread");
    error.clear();
    return true;
}

bool hash_executable(std::string& result, std::string& error) {
    std::array<wchar_t, 32768> path{};
    const DWORD chars = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!chars || chars >= path.size()) return fail(error, "GetModuleFileNameW failed: " + std::to_string(GetLastError()));
    HANDLE file = CreateFileW(path.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return fail(error, "open executable for build hash failed: " + std::to_string(GetLastError()));
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    DWORD object_size = 0, received = 0;
    if (status >= 0) status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &received, 0);
    std::vector<unsigned char> object(object_size);
    if (status >= 0) status = BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0);
    std::array<unsigned char, 65536> block{};
    DWORD bytes = 0, io_error = 0;
    while (status >= 0) {
        if (!ReadFile(file, block.data(), static_cast<DWORD>(block.size()), &bytes, nullptr)) {
            io_error = GetLastError();
            break;
        }
        if (!bytes) break;
        status = BCryptHashData(hash, block.data(), bytes, 0);
    }
    std::array<unsigned char, 32> digest{};
    if (status >= 0 && !io_error) status = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    CloseHandle(file);
    if (status < 0 || io_error) return fail(error, "executable hash failed: BCrypt=" + std::to_string(status) + " Win32=" + std::to_string(io_error));
    constexpr char hex[] = "0123456789abcdef";
    result.clear();
    for (auto byte : digest) { result += hex[byte >> 4]; result += hex[byte & 15]; }
    return true;
}

// This storage is populated and released by the game's own libstdc++ functions.
struct alignas(8) GameString { unsigned char bytes[32]; };
using ConstructString = void(*)(void*, const char*);
using DisposeString = void(*)(void*);
using StringMethod = void(*)(void*, const void*);
using FloatMethod = void(*)(void*, float);

DWORD focus_camera_native(void* camera, float x, float y, float z, float distance) {
    const float position[3] = {x, y, z};
    __try {
        reinterpret_cast<void(*)(void*, const float*, float)>(image.load() + 0x64150)(camera, position, distance);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}

DWORD subtract_building_expense(void* business, int32_t amount, bool& applied) {
    __try {
        applied = reinterpret_cast<bool(*)(void*, int32_t, int32_t)>(image.load() + 0x54d80)(business, amount, 6);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}

DWORD call_string(uintptr_t method, void* receiver, const char* text) {
    GameString value{};
    bool constructed = false;
    DWORD exception = 0;
    const auto base = image.load();
    __try {
        reinterpret_cast<ConstructString>(base + string_construct)(&value, text);
        constructed = true;
        reinterpret_cast<StringMethod>(base + method)(receiver, &value);
    } __except (EXCEPTION_EXECUTE_HANDLER) { exception = GetExceptionCode(); }
    if (constructed) {
        __try { reinterpret_cast<DisposeString>(base + string_dispose)(&value); }
        __except (EXCEPTION_EXECUTE_HANDLER) { if (!exception) exception = GetExceptionCode(); }
    }
    return exception;
}

DWORD call_float(void* receiver, float speed) {
    __try { reinterpret_cast<FloatMethod>(image.load() + time_scale)(receiver, speed); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}

DWORD call_void(uintptr_t method, void* receiver) {
    __try { reinterpret_cast<void(*)(void*)>(image.load() + method)(receiver); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}

DWORD find_region(float x, float z, void*& region) {
    const float position[3]{x, 0, z};
    void* shard = nullptr;
    if (!read_at(singleton(0x13df440), 0x60, shard)) return ERROR_INVALID_ADDRESS;
    __try {
        region = reinterpret_cast<void*(*)(void*, const void*, void*)>(image.load() + 0x5089b0)(shard, position, nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}

struct RegionPurchaseResult { DWORD exception = 0; bool charged = false; bool affordable = false; };

RegionPurchaseResult purchase_region_native(void* region, void* business, int32_t price) {
    const auto base = image.load();
    RegionPurchaseResult result;
    GameString label{};
    bool string_live = false;
    bool edit_live = false;
    __try {
        result.affordable = reinterpret_cast<bool(*)(void*, int)>(base + 0x54d00)(business, price);
        if (result.affordable) {
            reinterpret_cast<ConstructString>(base + string_construct)(&label, "MPTest region purchase");
            string_live = true;
            reinterpret_cast<void(*)(const void*)>(base + 0x265300)(&label);
            edit_live = true;
            result.charged = reinterpret_cast<bool(*)(void*, int, int)>(base + 0x54d80)(business, price, 7);
            if (result.charged) reinterpret_cast<void(*)(void*)>(base + 0x353a50)(region);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { result.exception = GetExceptionCode(); }
    if (edit_live) {
        __try { reinterpret_cast<void(*)()>(base + 0x265650)(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { if (!result.exception) result.exception = GetExceptionCode(); }
    }
    if (string_live) {
        __try { reinterpret_cast<DisposeString>(base + string_dispose)(&label); }
        __except (EXCEPTION_EXECUTE_HANDLER) { if (!result.exception) result.exception = GetExceptionCode(); }
    }
    return result;
}

DWORD first_game_type(void* window, void*& type) {
    __try {
        auto** item = reinterpret_cast<void**(*)(void*, int)>(image.load() + 0xb3c000)(
            static_cast<unsigned char*>(window) + 0x4f8, 0);
        type = item ? *item : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}

DWORD find_typed_object(uint64_t uid, uintptr_t type_rtti, void*& prop, bool* object_found = nullptr) {
    const auto base = image.load();
    if (object_found) *object_found = false;
    __try {
        auto* object = reinterpret_cast<void*(*)(uint64_t)>(base + 0x47ce20)(uid);
        if (object_found) *object_found = object != nullptr;
        prop = object ? reinterpret_cast<void*(*)(void*, const void*, const void*, ptrdiff_t)>(base + 0x12654f0)(
            object, reinterpret_cast<void*>(base + 0x1673900), reinterpret_cast<void*>(base + type_rtti), 0) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}

DWORD find_prop(uint64_t uid, void*& prop) { return find_typed_object(uid, 0x166be50, prop); }

DWORD cast_dungeon(void* object, void*& dungeon) {
    const auto base = image.load();
    __try {
        dungeon = reinterpret_cast<void*(*)(void*, const void*, const void*, ptrdiff_t)>(base + 0x12654f0)(
            object, reinterpret_cast<void*>(base + 0x1673900), reinterpret_cast<void*>(base + 0x164e3d0), 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}

DWORD cast_dungeon_instance(void* object, void*& instance) {
    const auto base = image.load();
    __try {
        instance = reinterpret_cast<void*(*)(void*, const void*, const void*, ptrdiff_t)>(base + 0x12654f0)(
            object, reinterpret_cast<void*>(base + 0x1657270), reinterpret_cast<void*>(base + 0x16650e0), 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}

DWORD find_subscriber_native(void* manager, int32_t serialized_id, void*& subscriber) {
    __try {
        subscriber = reinterpret_cast<void*(*)(void*, int32_t)>(image.load() + 0x2ee130)(manager, serialized_id);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}

bool is_world_prop(const void* prop) {
    void* active_shard = nullptr;
    void* shard = nullptr;
    void* region = nullptr;
    return prop && read_at(singleton(0x13df440), 0x60, active_shard) && active_shard &&
        read_at(prop, 0x170, shard) && shard == active_shard && read_at(prop, 0x178, region) && region;
}

bool prop_state(void* prop, PropState& state) {
    return is_world_prop(prop) && read_at(prop, 0x18, state.uid) && read_at(prop, 0x110, state.x) &&
        read_at(prop, 0x114, state.y) && read_at(prop, 0x118, state.z) &&
        std::isfinite(state.x) && std::isfinite(state.y) && std::isfinite(state.z);
}

DWORD move_prop_native(void* prop, const unsigned char* transform) {
    const auto base = image.load();
    GameString label{};
    alignas(16) unsigned char action[0x130]{};
    bool string_live = false;
    bool edit_live = false;
    bool action_live = false;
    DWORD exception = 0;
    __try {
        reinterpret_cast<ConstructString>(base + string_construct)(&label, "MPTest move");
        string_live = true;
        reinterpret_cast<void(*)(const void*)>(base + 0x265300)(&label);
        edit_live = true;
        reinterpret_cast<void(*)(void*, void*, const void*)>(base + 0x55e8e0)(action, prop, transform);
        action_live = true;
        reinterpret_cast<void(*)(void*, void*)>(base + 0x55ed80)(action, nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) { exception = GetExceptionCode(); }
    if (action_live) {
        __try { reinterpret_cast<void(*)(void*)>(base + 0xcf1600)(action); }
        __except (EXCEPTION_EXECUTE_HANDLER) { if (!exception) exception = GetExceptionCode(); }
    }
    if (edit_live) {
        __try { reinterpret_cast<void(*)()>(base + 0x265650)(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { if (!exception) exception = GetExceptionCode(); }
    }
    if (string_live) {
        __try { reinterpret_cast<DisposeString>(base + string_dispose)(&label); }
        __except (EXCEPTION_EXECUTE_HANDLER) { if (!exception) exception = GetExceptionCode(); }
    }
    return exception;
}

bool read_game_string(const void* object, size_t offset, std::string& value) {
    const char* data = nullptr;
    size_t length = 0;
    if (!read_at(object, offset, data) || !read_at(object, offset + 8, length) ||
        length > 256 || (length && !readable(data, length))) return false;
    value.assign(length ? data : "", length);
    return true;
}

struct PlacementResult {
    DWORD exception = 0;
    int stage = 0;
    bool purchased = false;
    void* building = nullptr;
    float ground_y = 0;
    bool region_found = false;
    bool region_flag_readable = false;
    unsigned char region_active = 0;
    bool buildings_found = false;
    bool paths_found = false;
    bool island_lookup_returned = false;
    bool island_found = false;
    DWORD island_unlock_exception = 0;
};

struct IslandProbe {
    int stage = 17;
    DWORD exception = 0, unlock_exception = 0;
    bool lock_attempted = false, lock_returned = false, lookup_returned = false, island_found = false;
};

IslandProbe probe_root_island(void* shard, const float* position) {
    IslandProbe result;
    void* manager = nullptr;
    bool held = false;
    const auto base = image.load();
    __try {
        do {
            int32_t type = -1;
            auto* map = singleton(0x13df8c0);
            if (island_lock_unknown.load() || !has_vtable(map, 0x1742850) ||
                shard != static_cast<unsigned char*>(map) + 0x1c0 || !has_vtable(shard, 0x174ad10) ||
                !read_at(shard, 0x48, type) || type != 0 || !read_at(map, 0x2b0, manager) || !manager) break;
            void* lock = nullptr;
            if (!read_at(manager, 0, lock) || !lock) break;
            result.stage = 18;
            result.lock_attempted = true;
            reinterpret_cast<void(*)(void*)>(base + 0xa0b120)(manager);
            result.lock_returned = true;
            held = true;
            result.stage = 19;
            const auto* hash = static_cast<unsigned char*>(manager) + 0x28;
            int32_t count = -1, capacity = -1, dim_x = 0, dim_z = 0;
            float size = 0, origin_x = 0, origin_z = 0;
            void* cells = nullptr;
            if (!has_vtable(static_cast<unsigned char*>(manager) + 0x10, 0x1704be0) ||
                !has_vtable(static_cast<unsigned char*>(manager) + 0x68, 0x1749030) ||
                !has_vtable(hash, 0x17471d0) || !read_at(hash, 8, cells) ||
                !read_at(hash, 0x10, count) || !read_at(hash, 0x14, capacity) ||
                !read_at(hash, 0x18, size) || !read_at(hash, 0x1c, origin_x) || !read_at(hash, 0x24, origin_z) ||
                !read_at(hash, 0x28, dim_x) || !read_at(hash, 0x2c, dim_z) ||
                !std::isfinite(size) || size <= 0 || !std::isfinite(origin_x) || !std::isfinite(origin_z) ||
                dim_x <= 0 || dim_z <= 0 || count <= 0 || capacity < count || int64_t(dim_x) * dim_z != count ||
                !cells) break;
            const float cell_x = (position[0] - origin_x) / size;
            const float cell_z = (position[2] - origin_z) / size;
            if (!std::isfinite(cell_x) || !std::isfinite(cell_z) || double(cell_x) < INT32_MIN ||
                double(cell_x) > INT32_MAX || double(cell_z) < INT32_MIN || double(cell_z) > INT32_MAX) break;
            const int32_t x = (std::clamp)(int32_t(cell_x), 0, dim_x - 1);
            const int32_t z = (std::clamp)(int32_t(cell_z), 0, dim_z - 1);
            const auto* cell = static_cast<unsigned char*>(cells) + size_t(int64_t(z) * dim_x + x) * 0x18;
            int32_t islands = -1, allocated = -1;
            void* pointers = nullptr;
            if (!has_vtable(cell, 0x1748fd0) || !read_at(cell, 8, pointers) || !read_at(cell, 0x10, islands) ||
                !read_at(cell, 0x14, allocated) || islands < 0 || allocated < islands ||
                (islands && !readable(pointers, size_t(islands) * sizeof(void*)))) break;
            result.stage = 20;
            result.island_found = reinterpret_cast<void*(*)(void*, const float*)>(base + 0x27b290)(manager, position) != nullptr;
            result.lookup_returned = true;
            result.stage = result.island_found ? 10 : 21;
        } while (false);
    } __except (EXCEPTION_EXECUTE_HANDLER) { result.exception = GetExceptionCode(); }
    if (held) {
        __try { reinterpret_cast<void(*)(void*)>(base + 0xa0b140)(manager); }
        __except (EXCEPTION_EXECUTE_HANDLER) { result.unlock_exception = GetExceptionCode(); }
    }
    if ((result.lock_attempted && !result.lock_returned) || result.unlock_exception) island_lock_unknown.store(true);
    return result;
}

PlacementResult place_building_native(const char* type_text, const char* variant_text, float x, float z,
    bool apply, bool purchase, float expected_y) {
    PlacementResult result;
    const auto base = image.load();
    GameString type{}, variant{}, label{};
    unsigned strings = 0;
    bool shape_live = false;
    bool edit_live = false;
    alignas(16) unsigned char shape[0x70]{};
    alignas(16) unsigned char transform[0x6c]{};
    alignas(8) unsigned char paths[0x18]{};
    alignas(4) unsigned char bounds[0x1c]{};
    float position[3]{x, 0, z};
    const float rotation[4]{0, 0, 0, 1};
    const float scale[3]{1, 1, 1};
    void* path_storage = nullptr;
    __try {
        do {
            reinterpret_cast<ConstructString>(base + string_construct)(&type, type_text);
            strings = 1;
            reinterpret_cast<ConstructString>(base + string_construct)(&variant, variant_text);
            strings = 2;
            auto* models = reinterpret_cast<void*(*)()>(base + 0xb318b0)();
            auto* selected = reinterpret_cast<void*(*)(void*, const void*, const void*)>(base + 0x251650)(models, &type, &variant);
            result.stage = 1;
            if (!selected) break;
            auto* art = reinterpret_cast<void*(*)()>(base + 0xb24600)();
            result.stage = 2;
            if (reinterpret_cast<bool(*)(void*, const void*)>(base + 0x3ca80)(art, &type)) break;
            auto* footprint = reinterpret_cast<void*(*)(void*, const void*, const void*)>(base + 0x251a30)(models, &type, &variant);
            result.stage = 3;
            if (!readable(footprint, 0x98)) break;
            reinterpret_cast<void(*)(void*)>(base + 0xf1810)(shape);
            shape_live = true;
            reinterpret_cast<void(*)(void*, const void*)>(base + 0xf9c30)(shape, footprint);
            reinterpret_cast<void(*)(void*)>(base + 0xf3600)(shape);
            if (!reinterpret_cast<bool(*)(void*)>(base + 0xf1440)(shape)) break;
            std::memcpy(bounds, shape + 0x20, sizeof(bounds));
            for (size_t offset : {size_t(4), size_t(16)}) {
                float value;
                std::memcpy(&value, bounds + offset, 4);
                value += x;
                std::memcpy(bounds + offset, &value, 4);
                std::memcpy(&value, bounds + offset + 8, 4);
                value += z;
                std::memcpy(bounds + offset + 8, &value, 4);
            }
            auto* shard_manager = reinterpret_cast<void*(*)()>(base + 0xb28340)();
            void* shard = nullptr;
            result.stage = 4;
            if (!read_at(shard_manager, 0x60, shard) || !shard ||
                !reinterpret_cast<bool(*)(void*, const void*)>(base + 0x508540)(shard, position)) break;
            position[1] = reinterpret_cast<float(*)(void*, const void*)>(base + 0x508450)(shard, bounds);
            if (!std::isfinite(position[1])) break;
            result.ground_y = position[1];
            if (!purchase && apply && std::abs(position[1] - expected_y) > 0.01f) {
                result.stage = 11;
                break;
            }
            auto* region = reinterpret_cast<void*(*)(void*, const void*, void*)>(base + 0x5089b0)(shard, position, nullptr);
            unsigned char active = 0;
            void* buildings = nullptr;
            void* path_manager = nullptr;
            result.region_found = region != nullptr;
            result.region_flag_readable = read_at(region, 0x850, active);
            result.region_active = active;
            result.buildings_found = read_at(region, 0x950, buildings) && buildings;
            result.paths_found = read_at(region, 0x9a0, path_manager) && path_manager;
            result.stage = 12;
            if (!result.region_found) break;
            result.stage = 13;
            if (!result.region_flag_readable || active > 1) break;
            result.stage = 14;
            if (!active) break;
            result.stage = 15;
            if (!result.buildings_found) break;
            result.stage = 16;
            if (!result.paths_found) break;
            const auto island = probe_root_island(shard, position);
            result.island_lookup_returned = island.lookup_returned;
            result.island_found = island.island_found;
            result.island_unlock_exception = island.unlock_exception;
            if (island.exception || island.unlock_exception || island.stage != 10) {
                result.exception = island.exception;
                result.stage = island.stage == 10 ? 22 : island.stage;
                break;
            }
            reinterpret_cast<void(*)(void*, const void*, const void*, const void*)>(base + 0x9ff370)(transform, rotation, position, scale);
            reinterpret_cast<void(*)(void*, const void*)>(base + 0xf9840)(shape, transform);
            result.stage = 6;
            if (reinterpret_cast<void*(*)(void*, const void*)>(base + 0x250790)(buildings, shape)) break;
            reinterpret_cast<void(*)(void*, void*, const void*)>(base + 0x2b6170)(paths, path_manager, shape);
            int32_t path_count = 0;
            std::memcpy(&path_storage, paths + 8, 8);
            std::memcpy(&path_count, paths + 16, 4);
            result.stage = 7;
            if (path_count != 0) break;
            if (!apply) {
                result.stage = 10;
                break;
            }
            reinterpret_cast<ConstructString>(base + string_construct)(&label, "MPTest building placement");
            strings = 3;
            reinterpret_cast<void(*)(const void*)>(base + 0x265300)(&label);
            edit_live = true;
            result.stage = 8;
            if (purchase) {
                result.purchased = reinterpret_cast<bool(*)(const void*, const void*)>(base + 0x40bb10)(&type, position);
                if (!result.purchased) break;
            }
            result.stage = 9;
            result.building = reinterpret_cast<void*(*)(void*, const void*, const void*, const void*, void*, void*, void*, bool)>(base + 0x24ed30)(
                buildings, &type, &variant, transform, nullptr, nullptr, nullptr, false);
            if (!result.building) break;
            result.stage = 10;
        } while (false);
    } __except (EXCEPTION_EXECUTE_HANDLER) { result.exception = GetExceptionCode(); }
    if (edit_live) {
        __try { reinterpret_cast<void(*)()>(base + 0x265650)(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { if (!result.exception) result.exception = GetExceptionCode(); }
    }
    if (path_storage) {
        __try { reinterpret_cast<void(*)(void*)>(base + 0xa01280)(path_storage); }
        __except (EXCEPTION_EXECUTE_HANDLER) { if (!result.exception) result.exception = GetExceptionCode(); }
    }
    if (shape_live) {
        __try { reinterpret_cast<void(*)(void*)>(base + 0xf15a0)(shape); }
        __except (EXCEPTION_EXECUTE_HANDLER) { if (!result.exception) result.exception = GetExceptionCode(); }
    }
    for (unsigned i = strings; i > 0; --i) {
        void* string = i == 3 ? static_cast<void*>(&label) : i == 2 ? static_cast<void*>(&variant) : static_cast<void*>(&type);
        __try { reinterpret_cast<DisposeString>(base + string_dispose)(string); }
        __except (EXCEPTION_EXECUTE_HANDLER) { if (!result.exception) result.exception = GetExceptionCode(); }
    }
    return result;
}

bool game_string_equals(const void* object, size_t offset, const std::string& expected) {
    const char* text = nullptr;
    size_t length = 0;
    return read_at(object, offset, text) && read_at(object, offset + 8, length) &&
        length == expected.size() && (length == 0 ||
        (readable(text, length) && std::memcmp(text, expected.data(), length) == 0));
}

bool test_world_objects(void*& title, void*& window, std::string& error) {
    if (!check_thread(error)) return false;
    const auto* game = singleton(game_slot);
    void* active = nullptr;
    unsigned char pending = 1;
    if (!game || !read_at(game, 0x28, active) || !read_at(game, 0x350, title) || !title || active != title)
        return fail(error, "test world operation rejected: title mode must be active");
    if (!read_at(title, 0x20, window) || !readable(window, 0x691) ||
        !read_at(title, 0xd0, pending) || pending)
        return fail(error, "test world operation rejected: creation model absent or world transition pending");
    return true;
}

bool valid_test_name(const std::string& name) {
    return name.rfind("MPTest", 0) == 0 && name.size() <= 64 &&
        std::all_of(name.begin(), name.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_' || c == '-';
        });
}
}

bool initialize(std::string& error) {
    if (ready()) { error.clear(); return true; }
    std::string digest;
    if (!hash_executable(digest, error)) return false;
    if (digest != expected_hash) return fail(error, "unsupported executable SHA256=" + digest + " required=" + expected_hash);
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    struct Guard { uintptr_t rva; const char* bytes; size_t size; };
    const Guard guards[] = {
        {0x507b20, "\x53\x48\x83\xec\x20\x83\x79\x48\x02", 9},
        {0x508190, "\x56\x53\x48\x83\xec\x28\x48\x89\xcb\x48\x89\xd6", 12},
        {0xb5d90, "\x57\x56\x53\x48\x83\xec\x20\x48\x83\x79\x38\x00\x48\x89\x51\x30", 16},
        {0xb6530, "\x56\x53\x48\x83\xec\x48\x48\x83\x79\x38\x00\x48\x89\xcb", 14},
        {0xa2dc0, "\x57\x56\x53\x48\x83\xec\x20\x8b\x81\xc8\x04\x00\x00", 13},
        {0x64150, "\x56\x53\x48\x83\xec\x38\x48\x89\xce", 9},
        {0x68670, "\x48\x8b\x81\x98\x00\x00\x00", 7},
        {0x686e0, "\x31\xc0\xeb\x10\x0f\x1f\x40\x00", 8},
        {0x68ea0, "\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53", 12},
        {0x2de7f0, "\x53\x48\x83\xec\x20\x48\x83\x3d\x93\x0e\x10\x01\x00", 13},
        {0x2de820, "\x53\x48\x83\xec\x20\x48\x83\x3d\x63\x0e\x10\x01\x00", 13},
        {0x2de360, "\x41\x55\x41\x54\x55\x57\x56\x53\x48\x81\xec\x88\x00\x00\x00", 15},
        {0x2de6b0, "\x57\x56\x53\x48\x83\xec\x70\x0f\x11\x74\x24\x20", 12},
        {0x2ee130, "\x4c\x63\x49\x48\x45\x85\xc9\x7e\x37\x4c\x8b\x51\x40", 13},
        {0xc06c00, "\x56\x53\x48\x83\xec\x28\x48\x63\xda\x48\x89\xce", 12},
        {0x57570, "\x57\x56\x53\x48\x83\xec\x20\x48\x89\xce\xe8\xa1\xe6\xff\xff", 15},
        {0x2eb530, "\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x81\xec\xc8\x00\x00\x00", 19},
        {0x395ee0, "\xf3\x0f\x5f\x15\x40\x6e\x0f\x01\xf3\x0f\x5d\x15\x40\x6e\x0f\x01", 16},
        {0x395f00, "\x48\x63\xd2\xf3\x0f\x58\x54\x91\x18\xf3\x0f\x5f\x15\x17\x6e\x0f\x01", 17},
        {0x3960a0, "\xf3\x0f\x10\x0d\x80\x6c\x0f\x01\x0f\x10\x52\x18\x0f\x10\x41\x18", 16},
        {0x396120, "\xf3\x0f\x10\x0d\x00\x6c\x0f\x01\x0f\x10\x52\x18\x0f\x10\x41\x18", 16},
        {0x394a20, "\x55\x57\x56\x53\x48\x81\xec\x98\x00\x00\x00", 11},
        {0x38f620, "\x41\x54\x55\x57\x56\x53\x48\x83\xec\x40", 10},
        {0x2e7630, "\x31\xc0\x48\x8b\x91\x80\x00\x00\x00\x48\x85\xd2\x74\x07\x0f\xb6\x82\x25\x08\x00\x00\xc3", 22},
        {0x66340, "\x48\x8b\x81\x8c\x00\x00\x00", 7},
        {0x463ce0, "\x55\x57\x56\x53\x48\x81\xec\x98\x00\x00\x00", 11},
        {0x259d70, "\x56\x53\x48\x83\xec\x48\x0f\x2f\x49\x58", 10},
        {0x4a0570, "\x41\xb8\x01\x00\x00\x00\xe9\x15\xff\xff\xff", 11},
        {0x49e090, "\x55\x57\x56\x53\x48\x83\xec\x38", 8},
        {0x11bd1e0, "\x48\x8b\x01\x48\x83\xc1\x10\x48\x39\xc8", 10},
        {0xa0bc80, "\x48\x83\xec\x28\xe8\xff\x37\x07\x00", 9},
        {0x4b9be0, "\x41\x57\x41\x56\x41\x55\x41\x54", 8},
        {0x4b8770, "\x55\x57\x56\x53\x48\x81\xec\x88\x00\x00\x00", 11},
        {0x770300, "\x41\x57\x41\x56\x41\x55\x41\x54", 8},
        {0xb3c000, "\x41\x54\x55\x57\x56\x53\x48\x81\xec\x80\x02\x00\x00", 13},
        {0x47ce20, "\x53\x45\x31\xc0\x48\x85\xc9", 7},
        {0x55e8e0, "\x41\x57\x41\x56\x41\x55\x41\x54", 8},
        {0x55ed80, "\x55\x57\x56\x53\x48\x83\xec\x48", 8},
        {0x9563b0, "\xc6\x05\xa9\x5d\x2f\x01\x01\xc3", 8},
        {0x4913b0, "\x41\x57\x41\x56\x41\x55\x41\x54", 8},
        {0xb318b0, "\x57\x56\x53\x48\x83\xc4\x80\x48", 8},
        {0x251650, "\x53\x48\x83\xec\x20\x4c\x89\xc3", 8},
        {0xb24600, "\x55\x57\x56\x53\x48\x81\xec\x48", 8},
        {0x3ca80, "\x41\x54\x55\x57\x56\x53\x48\x83", 8},
        {0x251a30, "\x56\x53\x48\x83\xec\x28\x48\x89", 8},
        {0xf1810, "\x56\x53\x48\x83\xec\x28\x48\x89", 8},
        {0xf9c30, "\x41\x57\x41\x56\x41\x55\x41\x54", 8},
        {0xf3600, "\x41\x57\x41\x56\x41\x55\x41\x54", 8},
        {0xf1440, "\x48\x83\x79\x18\x00\x74\x09\x8b", 8},
        {0xb28340, "\x41\x57\x41\x56\x41\x55\x41\x54", 8},
        {0x508540, "\x48\x83\xec\x38\x8b\x41\x48\x83", 8},
        {0x508450, "\x53\x48\x83\xec\x30\x8b\x41\x48", 8},
        {0x5089b0, "\x48\x83\xec\x38\x8b\x41\x48\x4c", 8},
        {0x27b290, "\x48\x83\xc1\x28\xe9\x67\x48\x00\x00", 9},
        {0xa0b120, "\x48\x8b\x09\xe9\x18\x42\x07\x00", 8},
        {0xa0b140, "\x48\x8b\x09\xe9\xf8\x40\x07\x00", 8},
        {0x9ff370, "\x49\x8b\x00\xf3\x0f\x6f\x02\x48", 8},
        {0xf9840, "\x57\x56\x53\x48\x83\xec\x20\x8b", 8},
        {0x250790, "\x41\x54\x55\x57\x56\x53\x48\x81", 8},
        {0x2b6170, "\x41\x57\x41\x56\x41\x55\x41\x54", 8},
        {0x40bb10, "\x55\x57\x56\x53\x48\x81\xec\x88", 8},
        {0x24ed30, "\x41\x57\x41\x56\x41\x55\x41\x54", 8},
        {0xa01280, "\xe9\x63\x03\x09\x00\x66\x66\x2e", 8},
        {0xf15a0, "\x56\x53\x48\x83\xec\x28\x48\x8d", 8},
        {0x54d00, "\x0f\xb6\x81\xf0\x00\x00\x00\x84", 8},
        {0x54d80, "\xb8\x01\x00\x00\x00\x66\x0f\x6e", 8},
        {0x353a50, "\x56\x53\x48\x83\xec\x28\x80\xb9", 8},
        {0x4a1bb0, "\x41\x57\x41\x56\x41\x55\x41\x54", 8},
        {0x259450, "\x41\x54\x55\x57\x56\x53", 6},
        {0x4a2f90, "\x41\x57\x41\x56\x41\x55\x41\x54", 8},
        {0x265300, "\x57\x56\x53\x48\x83\xec\x40", 7},
        {0x265650, "\x41\x57\x41\x56\x41\x55\x41\x54", 8},
    };
    for (const auto& guard : guards) {
        auto* address = reinterpret_cast<const void*>(base + guard.rva);
        if (!readable(address, guard.size) || std::memcmp(address, guard.bytes, guard.size))
            return fail(error, "native prologue guard rejected RVA=" + std::to_string(guard.rva));
    }
    resolved_executable_sha256=digest;
    image.store(base);
    error.clear();
    LOG_INFO("[Native] Resolver ready for build 24993678 sha256={}; live acceptance pending", digest);
    return true;
}

bool ready() { return image.load() != 0; }
bool read_resolver_receipt(uintptr_t& module,uint32_t& thread,std::string& digest,std::string& error) {
    module=0;thread=0;digest.clear();
    if(!check_thread(error))return false;
    if(image.load()!=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))||resolved_executable_sha256.empty())
        return fail(error,"native resolver receipt current process identity differs");
    module=image.load();thread=game_thread.load();digest=resolved_executable_sha256;return true;
}
Capabilities capabilities() {
    return {world_available.load(), world_available.load(), title_available.load(), false, false};
}

void* entrypoint(EntryPoint point) {
    constexpr uintptr_t addresses[] = {0x463ce0, 0x4a1bb0, 0x259450, 0x4a2f90, 0x265300, 0x265650, 0x2de7f0, 0x2de820,
        0x2de360, 0x2de6b0, 0x57570, 0x2eb530, 0x395ee0, 0x395f00, 0x3960a0, 0x396120, 0x394a20, 0x38f620};
    const auto index = static_cast<unsigned>(point);
    if (!ready() || index >= std::size(addresses)) return nullptr;
    return reinterpret_cast<void*>(image.load() + addresses[index]);
}

bool bind_game_thread() {
    if (!ready() || !reinterpret_cast<bool(*)()>(image.load() + 0xa0bc80)()) {
        LOG_ERROR("[Native] Thread binding rejected: engine vsThread::IsMainThread is false or resolver is not ready");
        return false;
    }
    const DWORD current = GetCurrentThreadId();
    DWORD unbound = 0;
    if (game_thread.compare_exchange_strong(unbound, current)) {
        LOG_INFO("[Native] Engine thread bound id={}", current);
        return true;
    }
    if (unbound == current) return true;
    LOG_ERROR("[Native] Thread binding rejected: current={} bound={}", current, unbound);
    return false;
}

bool in_game() {
    if (!ready() || game_thread.load() != GetCurrentThreadId()) return false;
    const auto* mode = singleton(mode_slot);
    const auto* game = singleton(game_slot);
    void* active_mode = nullptr;
    void* world = nullptr;
    int mode_state = -1;
    unsigned char quitting = 1;
    return has_vtable(game, 0x1743730) && read_at(game, 0x28, active_mode) && active_mode == mode &&
        has_vtable(mode, 0x16e6720) && read_at(mode, 0x58, world) && world &&
        read_at(mode, 0x2c4, mode_state) && (mode_state == 0 || mode_state == 4) &&
        read_at(mode, 0x304, quitting) && !quitting;
}

namespace {
DayTickGuard day_receiver_guard(void* receiver, DayTickLeaf leaf, int32_t day, int32_t& subscriber_id) {
    if (!ready()) return DayTickGuard::ResolverUnavailable;
    if (!game_thread.load() || game_thread.load() != GetCurrentThreadId()) return DayTickGuard::WrongThread;
    if (!in_game()) return DayTickGuard::NoActiveWorld;
    if (leaf == DayTickLeaf::BusinessFinances) {
        const auto* business = singleton(0x13df220);
        if (!business) return DayTickGuard::SingletonUnavailable;
        if (!has_vtable(business, 0x16d97d0)) return DayTickGuard::SingletonVtableMismatch;
        if (receiver != business) return DayTickGuard::ReceiverMismatch;
        if (day < 0) return DayTickGuard::DayArgumentInvalid;
    } else {
        if (!has_vtable(receiver, 0x16e75e0)) return DayTickGuard::ReceiverVtableMismatch;
        if (!read_at(receiver, 0x20, subscriber_id)) return DayTickGuard::SubscriberIdUnreadable;
        if (subscriber_id < 0) return DayTickGuard::SubscriberIdInvalid;
    }
    return DayTickGuard::Accepted;
}

void log_day_guard_transition(AtomicDayTickCounters& counters, DayTickLeaf leaf,
    DayTickGuard previous, DayTickGuard guard, int32_t day_argument, int32_t subscriber_id) {
    if (previous == guard) return;
    const auto mask = uint64_t{1} << static_cast<unsigned>(guard);
    const bool first_reason = (counters.logged_reasons.fetch_or(mask) & mask) == 0;
    const auto now = GetTickCount64();
    auto next = counters.next_transition_log_ms.load();
    bool due = first_reason;
    if (first_reason) counters.next_transition_log_ms.store(now + 30000);
    else if (now >= next)
        due = counters.next_transition_log_ms.compare_exchange_strong(next, now + 30000);
    if (!due) {
        counters.deferred_logs.fetch_add(1);
        return;
    }
    const char* operation = leaf == DayTickLeaf::BusinessFinances ? "business_next_day_finances" : "subscriber_new_day";
    if (guard == DayTickGuard::Accepted)
        LOG_INFO("[Native] Day observation {} recovered from={}; original remains unconditional", operation,
            day_tick_guard_name(previous));
    else if (guard == DayTickGuard::NoActiveWorld)
        LOG_INFO("[Native] Day observation {} skipped classification: {}; original remains unconditional", operation,
            day_tick_guard_name(guard));
    else if (guard == DayTickGuard::WrongThread)
        LOG_WARN("[Native] Day observation {} rejected classification: wrong_thread current={} required={}; original remains unconditional",
            operation, GetCurrentThreadId(), game_thread.load());
    else if (guard == DayTickGuard::DayArgumentInvalid || guard == DayTickGuard::SubscriberIdInvalid)
        LOG_WARN("[Native] Day observation {} rejected classification: {} value={} required=nonnegative; original remains unconditional",
            operation, day_tick_guard_name(guard), guard == DayTickGuard::DayArgumentInvalid ? day_argument : subscriber_id);
    else
        LOG_WARN("[Native] Day observation {} rejected classification: {}; original remains unconditional", operation,
            day_tick_guard_name(guard));
}

SoftwareTickGuard software_receiver_guard(void* receiver, bool ticker_receiver) {
    if (!ready()) return SoftwareTickGuard::ResolverUnavailable;
    if (!game_thread.load() || game_thread.load() != GetCurrentThreadId()) return SoftwareTickGuard::WrongThread;
    if (!in_game()) return SoftwareTickGuard::NoActiveWorld;
    const auto* software = singleton(software_slot);
    if (!software) return SoftwareTickGuard::SingletonUnavailable;
    const auto* ticker = static_cast<const unsigned char*>(software) + 0x18;
    if (receiver != (ticker_receiver ? ticker : software)) return SoftwareTickGuard::ReceiverMismatch;
    if (!has_vtable(software, 0x1715440)) return SoftwareTickGuard::PrimaryVtableMismatch;
    if (!has_vtable(ticker, 0x1715500)) return SoftwareTickGuard::SecondaryVtableMismatch;
    uintptr_t primary_tick = 0, secondary_tick = 0;
    const auto* base = reinterpret_cast<const void*>(image.load());
    if (!read_at(base, 0x17154f8, primary_tick) || primary_tick != image.load() + 0x2de7f0 ||
        !read_at(base, 0x1715520, secondary_tick) || secondary_tick != image.load() + 0x2de820)
        return SoftwareTickGuard::TickSlotMismatch;
    return SoftwareTickGuard::Accepted;
}

bool software_cache_read(const void* software, SoftwareCache& cache) {
    return read_at(software, 0x314, cache.rating) && read_at(software, 0x318, cache.fun) &&
        std::isfinite(cache.rating) && std::isfinite(cache.fun);
}

bool software_cache_equal(const SoftwareCache& a, const SoftwareCache& b) {
    return a.rating == b.rating && a.fun == b.fun;
}

bool software_replica_reject(const std::string& cause) {
    if (software_replica.error.empty()) {
        software_replica.error = cause;
        LOG_ERROR("[Native] Software replica epoch={} blocked: {}; native calculations forward", software_replica.epoch, cause);
    }
    return false;
}

bool bind_software_replica(void* software) {
    if (!software_replica.armed || !software_replica.error.empty()) return false;
    void* world = nullptr;
    if (!read_at(singleton(mode_slot), 0x58, world) || !world)
        return software_replica_reject("active world pointer unavailable");
    if (software_replica.bound) {
        if (world != software_replica.world || software != software_replica.software)
            return software_replica_reject("world or software identity changed; a new title-mode arm is required");
        return true;
    }
    std::string folder;
    if (!read_game_string(singleton(game_slot), 0x470, folder))
        return software_replica_reject("active world folder is unreadable");
    // Joined snapshots carry their validated destination folder through the title-mode arm.
    // The legacy diagnostic route has no snapshot folder and keeps its MPTest restriction.
    if (software_replica.world_folder.empty() ? !valid_test_name(folder) : folder != software_replica.world_folder)
        return software_replica_reject("active world folder differs from the armed snapshot world");
    if (!software_cache_read(software, software_replica.initial))
        return software_replica_reject("initial cache pair is unreadable or nonfinite");
    software_replica.world = world;
    software_replica.software = software;
    software_replica.bound = true;
    LOG_INFO("[Native] Software replica bound epoch={} initial_rating={} initial_fun={}",
        software_replica.epoch, software_replica.initial.rating, software_replica.initial.fun);
    return true;
}

DWORD store_software_cache(void* software, const SoftwareCache& values) {
    __try {
        // The native leaves write these adjacent scalar caches without callbacks.
        std::memcpy(static_cast<unsigned char*>(software) + 0x314, &values.rating, sizeof(float));
        std::memcpy(static_cast<unsigned char*>(software) + 0x318, &values.fun, sizeof(float));
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
}

const char* day_tick_guard_name(DayTickGuard guard) {
    constexpr const char* names[] = {"accepted", "resolver_unavailable", "wrong_thread", "no_active_world",
        "singleton_unavailable", "singleton_vtable_mismatch", "receiver_mismatch", "receiver_vtable_mismatch",
        "subscriber_id_unreadable", "subscriber_id_invalid", "day_argument_invalid", "invalid_leaf"};
    const auto index = static_cast<size_t>(guard);
    return index < std::size(names) ? names[index] : "invalid_guard";
}

DayTickGuard observe_day_tick(void* receiver, DayTickLeaf leaf, int32_t day_argument) {
    const auto index = static_cast<size_t>(leaf);
    if (index >= day_tick_counters.size()) {
        if (invalid_day_observe_calls.fetch_add(1) == 0)
            LOG_ERROR("[Native] Day observation rejected: invalid leaf={}; original must still run", index);
        return DayTickGuard::InvalidLeaf;
    }
    auto& counters = day_tick_counters[index];
    counters.entered.fetch_add(1);
    int32_t subscriber_id = -1;
    const auto guard = day_receiver_guard(receiver, leaf, day_argument, subscriber_id);
    counters.guards[static_cast<size_t>(guard)].fetch_add(1);
    if (guard == DayTickGuard::Accepted) {
        counters.accepted.fetch_add(1);
        if (leaf == DayTickLeaf::BusinessFinances) counters.last_valid_day.store(day_argument);
        else counters.last_valid_subscriber_id.store(subscriber_id);
    } else {
        if (guard == DayTickGuard::WrongThread) counters.wrong_thread.fetch_add(1);
        else counters.receiver_guard_rejected.fetch_add(1);
        auto first = DayTickGuard::Accepted;
        counters.first_rejection.compare_exchange_strong(first, guard);
    }
    const auto previous = counters.last_guard.exchange(guard);
    log_day_guard_transition(counters, leaf, previous, guard, day_argument, subscriber_id);
    return guard;
}

void record_day_tick_forwarded(DayTickLeaf leaf) {
    const auto index = static_cast<size_t>(leaf);
    if (index >= day_tick_counters.size()) {
        if (invalid_day_forward_calls.fetch_add(1) == 0)
            LOG_ERROR("[Native] Day forward accounting rejected: invalid leaf={}", index);
        return;
    }
    day_tick_counters[index].forwarded.fetch_add(1);
}

void read_day_tick_state(DayTickState& state) {
    state = {};
    state.resolver_ready = ready();
    state.thread_bound = game_thread.load() != 0;
    state.invalid_observe_calls = invalid_day_observe_calls.load();
    state.invalid_forward_calls = invalid_day_forward_calls.load();
    for (size_t index = 0; index < state.ticks.size(); ++index) {
        const auto& source = day_tick_counters[index];
        auto& target = state.ticks[index];
        target.entered = source.entered.load();
        target.forwarded = source.forwarded.load();
        target.accepted = source.accepted.load();
        target.receiver_guard_rejected = source.receiver_guard_rejected.load();
        target.wrong_thread = source.wrong_thread.load();
        target.deferred_logs = source.deferred_logs.load();
        target.first_rejection = source.first_rejection.load();
        target.last_guard = source.last_guard.load();
        target.last_valid_day = source.last_valid_day.load();
        target.last_valid_subscriber_id = source.last_valid_subscriber_id.load();
        for (size_t reason = 0; reason < target.guards.size(); ++reason)
            target.guards[reason] = source.guards[reason].load();
    }
}

const char* software_tick_guard_name(SoftwareTickGuard guard) {
    constexpr const char* names[] = {"accepted", "resolver_unavailable", "wrong_thread", "no_active_world",
        "singleton_unavailable", "receiver_mismatch", "primary_vtable_mismatch", "secondary_vtable_mismatch", "tick_slot_mismatch"};
    const auto index = static_cast<size_t>(guard);
    return index < std::size(names) ? names[index] : "invalid_guard";
}

SoftwareTickGuard observe_software_tick(void* receiver, bool ticker_receiver) {
    auto& counters = software_tick_counters[ticker_receiver ? 1 : 0];
    counters.entered.fetch_add(1, std::memory_order_relaxed);
    const auto guard = software_receiver_guard(receiver, ticker_receiver);
    counters.guards[static_cast<size_t>(guard)].fetch_add(1, std::memory_order_relaxed);
    return guard;
}

void record_software_tick_forwarded(bool ticker_receiver) {
    software_tick_counters[ticker_receiver ? 1 : 0].forwarded.fetch_add(1, std::memory_order_relaxed);
}

bool arm_software_replica(uint64_t epoch, std::string& error, const std::string& world_folder) {
    if (!check_thread(error)) return false;
    if (!epoch) return fail(error, "software replica arm rejected: epoch must be nonzero");
    if (!world_folder.empty() && !net::snapshot_world_path::valid_folder(world_folder))
        return fail(error, "software replica arm rejected: snapshot world folder is invalid");
    auto* game = singleton(game_slot);
    void* title = nullptr;
    void* active = nullptr;
    unsigned char pending = 1;
    if (!has_vtable(game, 0x1743730) || !read_at(game, 0x350, title) ||
        !has_vtable(title, 0x16dfba0) || !read_at(game, 0x28, active) || active != title ||
        !read_at(title, 0xd0, pending) || pending)
        return fail(error, "software replica arm rejected: idle native title mode required before load");
    if (software_replica.armed && software_replica.epoch == epoch)
        return fail(error, "software replica arm rejected: epoch already used by armed policy");
    software_replica = {};
    software_replica.armed = true;
    software_replica.epoch = epoch;
    software_replica.world_folder = world_folder;
    LOG_INFO("[Native] Software replica armed epoch={} world={}; awaiting native load", epoch,
        world_folder.empty() ? "diagnostic MPTest" : world_folder);
    return true;
}

bool disarm_software_replica(uint64_t epoch, std::string& error) {
    if (!check_thread(error)) return false;
    if (!software_replica.armed || software_replica.epoch != epoch)
        return fail(error, "software replica disarm rejected: armed epoch differs");
    software_replica = {};
    LOG_INFO("[Native] Software replica disarmed epoch={}", epoch);
    return true;
}

bool retire_software_replica(std::string& error) {
    if (!check_thread(error)) return false;
    WorldStatus actual;
    if (!read_world_status(actual, error)) return false;
    if (actual.active_mode_index != 2 || actual.title_transition_pending || actual.saves_in_progress > 0)
        return fail(error, "software replica retirement requires idle native Title");
    if (software_replica.armed && !disarm_software_replica(software_replica.epoch, error)) return false;
    error.clear();
    return true;
}

bool software_calculation_should_forward(void* receiver, SoftwareCalcLeaf leaf) {
    const auto index = static_cast<size_t>(leaf);
    if (index >= software_calculation_counters.size()) {
        LOG_ERROR("[Native] Software calculation rejected: invalid leaf={}; native calculation forwards", index);
        return true;
    }
    auto& counters = software_calculation_counters[index];
    counters.entered.fetch_add(1, std::memory_order_relaxed);
    const auto guard = software_receiver_guard(receiver, false);
    if (guard != SoftwareTickGuard::Accepted) {
        counters.guard_rejected.fetch_add(1, std::memory_order_relaxed);
        if (game_thread.load() == GetCurrentThreadId() && software_replica.armed &&
            (software_replica.bound || guard != SoftwareTickGuard::NoActiveWorld))
            software_replica_reject(std::string("calculation receiver guard: ") + software_tick_guard_name(guard));
        return true;
    }
    if (!software_replica.armed) return true;
    if (!bind_software_replica(receiver)) {
        counters.guard_rejected.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    counters.suppressed.fetch_add(1, std::memory_order_relaxed);
    return false;
}

void record_software_calculation_forwarded(SoftwareCalcLeaf leaf) {
    const auto index = static_cast<size_t>(leaf);
    if (index >= software_calculation_counters.size()) {
        LOG_ERROR("[Native] Software calculation forward accounting rejected: invalid leaf={}", index);
        return;
    }
    software_calculation_counters[index].forwarded.fetch_add(1, std::memory_order_relaxed);
}

bool apply_software_cache(uint64_t epoch, uint64_t revision, const SoftwareCache& expected_before,
    const SoftwareCache& host_values, SoftwareCacheReceipt& receipt, std::string& error) {
    receipt = {};
    receipt.epoch = epoch;
    receipt.revision = revision;
    if (!check_thread(error)) return false;
    auto* software = singleton(software_slot);
    const auto guard = software_receiver_guard(software, false);
    if (guard != SoftwareTickGuard::Accepted)
        return fail(error, std::string("software cache apply rejected: ") + software_tick_guard_name(guard));
    if (!software_replica.armed || epoch != software_replica.epoch || !revision)
        return fail(error, "software cache apply rejected: armed epoch mismatch or zero revision");
    if (!bind_software_replica(software)) return fail(error, "software cache apply rejected: " + software_replica.error);
    if (!std::isfinite(host_values.rating) || host_values.rating < 0 || host_values.rating > 1000 ||
        !std::isfinite(host_values.fun) || !std::isfinite(expected_before.rating) || !std::isfinite(expected_before.fun))
        return fail(error, "software cache apply rejected: rating requires native range 0..1000; all cache values must be finite");
    if (!software_cache_read(software, receipt.before))
        return fail(error, "software cache apply rejected: current cache unreadable or nonfinite");
    receipt.after = receipt.before;
    if (revision == software_replica.revision && software_cache_equal(host_values, software_replica.applied) &&
        software_cache_equal(receipt.before, host_values)) {
        receipt.already_applied = true;
        return true;
    }
    if (revision <= software_replica.revision)
        return fail(error, "software cache apply rejected: stale or conflicting revision=" + std::to_string(revision) +
            " current=" + std::to_string(software_replica.revision));
    if (!software_cache_equal(receipt.before, expected_before))
        return fail(error, "software cache apply rejected: actual before=" + std::to_string(receipt.before.rating) +
            "/" + std::to_string(receipt.before.fun) + " expected=" + std::to_string(expected_before.rating) +
            "/" + std::to_string(expected_before.fun));
    auto* destination = static_cast<unsigned char*>(software) + 0x314;
    MEMORY_BASIC_INFORMATION memory{};
    if (!readable(destination, sizeof(float) * 2) || !mt2mp::game::live_host_state::apply_timing::virtual_query(destination, &memory, sizeof(memory)) ||
        !(memory.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
        return fail(error, "software cache apply rejected: cache pair is not writable");
    LOG_INFO("[Native] Software cache apply attempted epoch={} revision={} before={}/{} host={}/{}", epoch, revision,
        receipt.before.rating, receipt.before.fun, host_values.rating, host_values.fun);
    const DWORD exception = store_software_cache(software, host_values);
    const bool readback = software_cache_read(software, receipt.after);
    if (exception || !readback || !software_cache_equal(receipt.after, host_values)) {
        software_replica_reject("cache write or readback failed");
        return fail(error, "software cache apply failed: exception=" + std::to_string(exception) +
            " readable=" + std::to_string(readback) + " actual=" + std::to_string(receipt.after.rating) +
            "/" + std::to_string(receipt.after.fun));
    }
    software_replica.revision = revision;
    software_replica.applied = host_values;
    receipt.written = true;
    LOG_INFO("[Native] Software cache apply confirmed epoch={} revision={} rating={} fun={}", epoch, revision,
        receipt.after.rating, receipt.after.fun);
    return true;
}

bool read_software_state(SoftwareState& state, std::string& error) {
    state = {};
    if (!check_thread(error)) return false;
    for (size_t entry = 0; entry < state.calculations.size(); ++entry) {
        const auto& source = software_calculation_counters[entry];
        state.calculations[entry] = {source.entered.load(), source.forwarded.load(),
            source.suppressed.load(), source.guard_rejected.load()};
    }
    for (size_t entry = 0; entry < state.ticks.size(); ++entry) {
        const auto& source = software_tick_counters[entry];
        auto& target = state.ticks[entry];
        target.entered = source.entered.load(std::memory_order_relaxed);
        target.forwarded = source.forwarded.load(std::memory_order_relaxed);
        for (size_t reason = 0; reason < target.guards.size(); ++reason)
            target.guards[reason] = source.guards[reason].load(std::memory_order_relaxed);
    }
    auto* software = singleton(software_slot);
    const auto guard = software_receiver_guard(software, false);
    if (software_replica.armed && guard == SoftwareTickGuard::Accepted) bind_software_replica(software);
    else if (software_replica.armed && software_replica.bound)
        software_replica_reject(std::string("read receiver guard: ") + software_tick_guard_name(guard));
    state.replica_armed = software_replica.armed;
    state.replica_bound = software_replica.bound && software_replica.error.empty();
    state.bound_epoch = software_replica.epoch;
    state.applied_revision = software_replica.revision;
    state.initial_rating = software_replica.initial.rating;
    state.initial_fun = software_replica.initial.fun;
    state.replica_error = software_replica.error;
    if (guard != SoftwareTickGuard::Accepted)
        return fail(error, std::string("software read rejected: ") + software_tick_guard_name(guard));
    if (!read_at(software, 0x314, state.rating) || !read_at(software, 0x318, state.fun))
        return fail(error, "software read rejected: native rating or fun unreadable");
    if (!std::isfinite(state.rating) || !std::isfinite(state.fun))
        return fail(error, "software read rejected: rating=" + std::to_string(state.rating) +
            " fun=" + std::to_string(state.fun) + " required=finite");
    return true;
}

namespace {
bool subscriber_login_read(const void* subscriber, bool& logged_in) {
    void* toon = nullptr;
    if (!read_at(subscriber, 0x80, toon)) return false;
    logged_in = false;
    if (!toon) return true;
    unsigned char flag = 0;
    if (!has_vtable(toon, 0x1744e80) || !read_at(toon, 0x825, flag) || flag > 1) return false;
    logged_in = flag != 0;
    return true;
}
}

bool read_subscriber_needs(int32_t serialized_id, SubscriberNeedsState& state, std::string& error) {
    state = {};
    if (!check_thread(error)) return false;
    const std::string operation = "subscriber read id=" + std::to_string(serialized_id);
    if (serialized_id < 0) return fail(error, operation + " rejected: serialized ID must be 0..2147483647");
    if (!in_game()) return fail(error, operation + " rejected: no active world");
    void* world = nullptr;
    if (!read_at(singleton(mode_slot), 0x58, world) || !world)
        return fail(error, operation + " rejected: world identity unavailable");
    auto* manager = singleton(subscriber_manager_slot);
    if (!manager) return fail(error, operation + " rejected: subscriber manager absent");
    if (!has_vtable(manager, 0x1725f30))
        return fail(error, operation + " rejected: subscriber manager vtable mismatch");
    SubscriberNeedsState candidate;
    if (!read_at(manager, 0x48, candidate.subscriber_count))
        return fail(error, operation + " rejected: subscriber count unreadable");
    constexpr int32_t maximum_diagnostic_subscribers = 1000000;
    if (candidate.subscriber_count < 0 || candidate.subscriber_count > maximum_diagnostic_subscribers)
        return fail(error, operation + " rejected: count=" + std::to_string(candidate.subscriber_count) +
            " required diagnostic lookup count=0..1000000");
    void* entries = nullptr;
    if (!read_at(manager, 0x40, entries) || (candidate.subscriber_count &&
        !readable(entries, static_cast<size_t>(candidate.subscriber_count) * sizeof(void*))))
        return fail(error, operation + " rejected: subscriber array unreadable");
    void* subscriber = nullptr;
    const DWORD exception = find_subscriber_native(manager, serialized_id, subscriber);
    if (exception) return fail(error, operation + " lookup failed: native exception=" + std::to_string(exception));
    if (!subscriber) return fail(error, operation + " not found in native subscriber manager");
    if (!has_vtable(subscriber, 0x16e75e0))
        return fail(error, operation + " rejected: subscriber vtable mismatch");
    if (!read_at(subscriber, 0x20, candidate.serialized_id))
        return fail(error, operation + " rejected: returned serialized ID unreadable");
    if (candidate.serialized_id != serialized_id)
        return fail(error, operation + " rejected: returned ID=" + std::to_string(candidate.serialized_id));
    void* brain = nullptr;
    if (!read_at(subscriber, 0x88, brain))
        return fail(error, operation + " rejected: brain field unreadable");
    if (!brain) return fail(error, operation + " unavailable: subscriber has no active brain");
    if (!has_vtable(brain, 0x16fdd50))
        return fail(error, operation + " rejected: ToonBrainAI vtable mismatch");
    void* owner = nullptr;
    if (!read_at(brain, 0x28, owner))
        return fail(error, operation + " rejected: brain owner unreadable");
    if (owner != subscriber) return fail(error, operation + " rejected: brain owner differs from subscriber");
    const auto* needs = static_cast<const unsigned char*>(brain) + 0x180;
    if (!has_vtable(needs, 0x16c0ce0))
        return fail(error, operation + " rejected: NeedSet vtable mismatch");
    for (size_t index = 0; index < candidate.needs.size(); ++index) {
        if (!read_at(needs, 0x18 + index * sizeof(float), candidate.needs[index]))
            return fail(error, operation + " rejected: need index=" + std::to_string(index) + " unreadable");
        const float value = candidate.needs[index];
        if (!std::isfinite(value) || value < -100 || value > 100)
            return fail(error, operation + " rejected: need index=" + std::to_string(index) +
                " value=" + std::to_string(value) + " required finite -100..100");
    }
    if (!read_at(subscriber, 0x24, candidate.satisfaction))
        return fail(error, operation + " rejected: satisfaction unreadable");
    if (!read_at(subscriber, 0x28, candidate.addiction))
        return fail(error, operation + " rejected: addiction unreadable");
    if (!read_at(subscriber, 0x6c, candidate.logged_in_seconds) || !std::isfinite(candidate.logged_in_seconds))
        return fail(error, operation + " rejected: logged-in time unreadable or nonfinite");
    if (!subscriber_login_read(subscriber, candidate.logged_in))
        return fail(error, operation + " rejected: native IsLoggedIn receiver/flag is unreadable or invalid");
    void* final_world = nullptr;
    void* final_brain = nullptr;
    int32_t final_id = -1;
    if (!in_game() || !read_at(singleton(mode_slot), 0x58, final_world) || final_world != world ||
        singleton(subscriber_manager_slot) != manager || !read_at(subscriber, 0x20, final_id) || final_id != serialized_id ||
        !read_at(subscriber, 0x88, final_brain) || final_brain != brain)
        return fail(error, operation + " rejected: world or subscriber identity changed during read");
    state = candidate;
    return true;
}

bool read_subscriber_candidates(SubscriberCandidates& state, std::string& error) {
    state = {};
    if (!check_thread(error)) return false;
    if (!in_game()) return fail(error, "subscriber scan rejected: no active world");
    void* world = nullptr;
    if (!read_at(singleton(mode_slot), 0x58, world) || !world)
        return fail(error, "subscriber scan rejected: world identity unavailable");
    auto* manager = singleton(subscriber_manager_slot);
    if (!has_vtable(manager, 0x1725f30)) return fail(error, "subscriber scan rejected: manager vtable mismatch");
    if (!read_at(manager, 0x48, state.existing_count)) return fail(error, "subscriber scan rejected: count unreadable");
    if (state.existing_count < 0 || state.existing_count > 1000000)
        return fail(error, "subscriber scan rejected: count=" + std::to_string(state.existing_count) + " required=0..1000000");
    void* entries = nullptr;
    if (!read_at(manager, 0x40, entries) || (state.existing_count &&
        !readable(entries, static_cast<size_t>(state.existing_count) * sizeof(void*))))
        return fail(error, "subscriber scan rejected: array unreadable");
    for (int32_t slot = 0; slot < state.existing_count && state.serialized_ids.size() < 32; ++slot) {
        void* subscriber = nullptr;
        const std::string target = " slot=" + std::to_string(slot);
        if (!read_at(entries, static_cast<size_t>(slot) * sizeof(void*), subscriber) ||
            !has_vtable(subscriber, 0x16e75e0))
            return fail(error, "subscriber scan rejected: slot receiver invalid" + target);
        ++state.scanned_count;
        int32_t id = -1;
        if (!read_at(subscriber, 0x20, id) || id < 0)
            return fail(error, "subscriber scan rejected: serialized ID unreadable or negative" + target);
        bool logged_in = false;
        if (!subscriber_login_read(subscriber, logged_in))
            return fail(error, "subscriber scan rejected: login receiver/flag invalid" + target + " id=" + std::to_string(id));
        if (!logged_in) continue;
        void* lookup = nullptr;
        const DWORD exception = find_subscriber_native(manager, id, lookup);
        if (exception) return fail(error, "subscriber scan lookup failed: exception=" + std::to_string(exception) + target);
        if (lookup != subscriber)
            return fail(error, "subscriber scan rejected: manager lookup identity differs" + target + " id=" + std::to_string(id));
        if (std::find(state.serialized_ids.begin(), state.serialized_ids.end(), id) != state.serialized_ids.end())
            return fail(error, "subscriber scan rejected: duplicate serialized ID" + target + " id=" + std::to_string(id));
        SubscriberNeedsState candidate;
        if (!read_subscriber_needs(id, candidate, error)) return false;
        if (!candidate.logged_in) return fail(error, "subscriber scan rejected: login state changed" + target);
        state.serialized_ids.push_back(id);
        ++state.eligible_count;
    }
    void* final_world = nullptr;
    void* final_entries = nullptr;
    int32_t final_count = -1;
    if (!in_game() || !read_at(singleton(mode_slot), 0x58, final_world) || final_world != world ||
        singleton(subscriber_manager_slot) != manager || !read_at(manager, 0x40, final_entries) || final_entries != entries ||
        !read_at(manager, 0x48, final_count) || final_count != state.existing_count)
        return fail(error, "subscriber scan rejected: world or manager array changed during read");
    state.scan_complete = state.scanned_count == state.existing_count;
    return true;
}

namespace {
bool needs_values_valid(const SubscriberNeeds& values) {
    return std::all_of(values.begin(), values.end(), [](float value) {
        return std::isfinite(value) && value >= -100 && value <= 100;
    });
}

bool needs_values_read(const void* needs, SubscriberNeeds& values) {
    for (size_t index = 0; index < values.size(); ++index)
        if (!read_at(needs, 0x18 + index * sizeof(float), values[index])) return false;
    return needs_values_valid(values);
}

bool needs_reject(const char* cause) {
    if (needs_replica.error.empty()) {
        needs_replica.error = cause;
        LOG_ERROR("[Native] Needs replica id={} epoch={} blocked: {}; local writes forward",
            needs_replica.id, needs_replica.epoch, cause);
    }
    return false;
}

bool needs_binding_valid() {
    if (!needs_replica.armed || !needs_replica.error.empty()) return false;
    if (needs_wrong_thread.load()) return needs_reject("selected receiver was observed on an unbound thread");
    if ((needs_hook_mask.load() & 15u) != 15u) return needs_reject("four required write hooks are not installed");
    if (!software_replica.armed || !software_replica.bound || !software_replica.error.empty() ||
        software_replica.epoch != needs_replica.epoch || software_replica.world != needs_replica.world)
        return needs_reject("software policy epoch/world binding differs");
    if (singleton(software_slot) != software_replica.software || !has_vtable(software_replica.software, 0x1715440))
        return needs_reject("bound software singleton identity changed");
    if (!in_game()) return needs_reject("no active native world");
    void* world = nullptr;
    void* brain = nullptr;
    void* owner = nullptr;
    int32_t id = -1;
    if (!read_at(singleton(mode_slot), 0x58, world) || world != needs_replica.world)
        return needs_reject("world identity changed");
    if (singleton(subscriber_manager_slot) != needs_replica.manager || !has_vtable(needs_replica.manager, 0x1725f30))
        return needs_reject("subscriber manager identity changed");
    if (!has_vtable(needs_replica.subscriber, 0x16e75e0) || !read_at(needs_replica.subscriber, 0x20, id) || id != needs_replica.id)
        return needs_reject("subscriber type or serialized ID changed");
    if (!read_at(needs_replica.subscriber, 0x88, brain) || brain != needs_replica.brain || !has_vtable(brain, 0x16fdd50))
        return needs_reject("subscriber brain identity changed");
    if (!read_at(brain, 0x28, owner) || owner != needs_replica.subscriber)
        return needs_reject("brain owner differs from subscriber");
    if (!has_vtable(needs_replica.needs, 0x16c0ce0)) return needs_reject("embedded NeedSet type changed");
    return true;
}

DWORD set_needs_native(void* receiver, const SubscriberNeeds& values, SubscriberNeedsReceipt& receipt) {
    __try {
        const auto setter = reinterpret_cast<void(*)(void*, int32_t, float)>(image.load() + 0x395ee0);
        for (int32_t index = 0; index < 7; ++index) {
            if (!needs_replica.error.empty()) break;
            needs_replica.permit_index = index;
            needs_replica.permit_value = values[index];
            ++receipt.attempted_setters;
            setter(receiver, index, values[index]);
            ++receipt.completed_setters;
            if (needs_replica.permit_hits != static_cast<uint32_t>(index + 1)) break;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
    return 0;
}
}

bool subscriber_needs_hooks_ready(){return (needs_hook_mask.load()&15u)==15u;}
void set_subscriber_needs_hook_mask(uint32_t mask) {
    needs_hook_mask.store(mask & 63u);
    LOG_INFO("[Native] Needs installed hook mask={}", mask & 63u);
}

void record_subscriber_needs_write_forwarded(NeedsWriteLeaf leaf) {
    const auto slot = static_cast<size_t>(leaf);
    if (slot >= needs_write_counters.size()) {
        LOG_ERROR("[Native] Needs forward accounting invalid leaf={}", slot);
        return;
    }
    needs_write_counters[slot].forwarded.fetch_add(1);
}

bool observe_subscriber_needs_tick(void* brain, NeedsTickLeaf leaf) {
    const auto slot = static_cast<size_t>(leaf);
    if (slot >= needs_tick_counters.size()) {
        LOG_ERROR("[Native] Needs tick observation invalid leaf={}; original must run", slot);
        return false;
    }
    if (!brain || brain != needs_brain_target.load()) return false;
    auto& counters = needs_tick_counters[slot];
    counters.entered.fetch_add(1);
    if (!game_thread.load() || game_thread.load() != GetCurrentThreadId()) {
        counters.guard_rejected.fetch_add(1);
        if (!needs_wrong_thread.exchange(true))
            LOG_ERROR("[Native] Selected Brain tick on wrong thread current={} required={}; original must run",
                GetCurrentThreadId(), game_thread.load());
        return true;
    }
    if (!needs_binding_valid()) counters.guard_rejected.fetch_add(1);
    return true;
}

void record_subscriber_needs_tick_forwarded(NeedsTickLeaf leaf, bool selected_observation) {
    const auto slot = static_cast<size_t>(leaf);
    if (slot >= needs_tick_counters.size()) {
        LOG_ERROR("[Native] Needs tick forward accounting invalid leaf={}", slot);
        return;
    }
    if (selected_observation) needs_tick_counters[slot].forwarded.fetch_add(1);
}

bool arm_subscriber_needs_replica(uint64_t epoch, int32_t serialized_id,
    const SubscriberNeeds& expected_initial, std::string& error) {
    if (!check_thread(error)) return false;
    if (!epoch || !software_replica.armed || !software_replica.bound || !software_replica.error.empty() ||
        software_replica.epoch != epoch)
        return fail(error, "needs arm rejected: matching bound client software epoch required");
    if (needs_replica.armed || (needs_replica.epoch == epoch && needs_replica.id == serialized_id))
        return fail(error, "needs arm rejected: policy armed or subscriber epoch already used");
    if ((needs_hook_mask.load() & 15u) != 15u)
        return fail(error, "needs arm rejected: all four native write hooks must be installed");
    if (!needs_values_valid(expected_initial)) return fail(error, "needs arm rejected: expected values require finite -100..100");
    SubscriberNeedsState observed;
    if (!read_subscriber_needs(serialized_id, observed, error)) return false;
    if (observed.needs != expected_initial) return fail(error, "needs arm rejected: current seven values differ from expected initial");
    std::string folder;
    if (!read_game_string(singleton(game_slot), 0x470, folder) || !valid_test_name(folder))
        return fail(error, "needs arm rejected: active world is not a diagnostic MPTest world");
    NeedsReplica candidate;
    candidate.epoch = epoch;
    candidate.id = serialized_id;
    candidate.manager = singleton(subscriber_manager_slot);
    const DWORD exception = find_subscriber_native(candidate.manager, serialized_id, candidate.subscriber);
    if (exception || !candidate.subscriber || !read_at(candidate.subscriber, 0x88, candidate.brain) || !candidate.brain ||
        !read_at(singleton(mode_slot), 0x58, candidate.world) || candidate.world != software_replica.world)
        return fail(error, "needs arm rejected: stable native identity lookup failed exception=" + std::to_string(exception));
    candidate.needs = static_cast<unsigned char*>(candidate.brain) + 0x180;
    candidate.armed = true;
    candidate.initial = expected_initial;
    needs_replica = std::move(candidate);
    needs_wrong_thread.store(false);
    if (!needs_binding_valid()) {
        needs_replica.armed = false;
        return fail(error, "needs arm rejected: " + needs_replica.error);
    }
    SubscriberNeeds final_values;
    if (!needs_values_read(needs_replica.needs, final_values) || final_values != expected_initial) {
        needs_replica.armed = false;
        return fail(error, "needs arm rejected: values changed while binding");
    }
    needs_target.store(needs_replica.needs);
    needs_brain_target.store(needs_replica.brain);
    LOG_INFO("[Native] Needs replica armed id={} epoch={} logged_in={}", serialized_id, epoch, observed.logged_in);
    return true;
}

bool disarm_subscriber_needs_replica(uint64_t epoch, std::string& error) {
    if (!check_thread(error)) return false;
    if (!needs_replica.armed || needs_replica.epoch != epoch || needs_replica.permit)
        return fail(error, "needs disarm rejected: armed epoch mismatch or apply in progress");
    needs_target.store(nullptr);
    needs_brain_target.store(nullptr);
    needs_replica.armed = false;
    LOG_INFO("[Native] Needs replica disarmed id={} epoch={}", needs_replica.id, epoch);
    return true;
}

bool apply_subscriber_needs(uint64_t epoch, int32_t serialized_id, uint64_t revision,
    const SubscriberNeeds& expected_before, const SubscriberNeeds& host_needs,
    SubscriberNeedsReceipt& receipt, std::string& error) {
    receipt = {};
    receipt.epoch = epoch;
    receipt.revision = revision;
    receipt.serialized_id = serialized_id;
    if (!check_thread(error)) return false;
    if (!needs_replica.armed || needs_replica.epoch != epoch || needs_replica.id != serialized_id || !revision)
        return fail(error, "needs apply rejected: bound epoch/id mismatch or zero revision");
    if (!needs_binding_valid()) return fail(error, "needs apply rejected: " + needs_replica.error);
    if (needs_replica.permit) return fail(error, "needs apply rejected: another apply is in progress");
    if (!needs_values_valid(expected_before) || !needs_values_valid(host_needs))
        return fail(error, "needs apply rejected: all values require finite -100..100");
    receipt.before_readable = needs_values_read(needs_replica.needs, receipt.before);
    if (!receipt.before_readable) return fail(error, "needs apply rejected: current values unreadable or outside native range");
    receipt.after = receipt.before;
    receipt.after_readable = true;
    if (revision == needs_replica.revision && host_needs == needs_replica.applied &&
        expected_before == needs_replica.applied_before && receipt.before == host_needs) {
        receipt.already_applied = true;
        return true;
    }
    if (revision <= needs_replica.revision) return fail(error, "needs apply rejected: stale or conflicting revision");
    if (receipt.before != expected_before) return fail(error, "needs apply rejected: seven-value expected before mismatch");
    int32_t count = -1;
    void* entries = nullptr;
    if (!read_at(needs_replica.manager, 0x48, count) || count < 0 || count > 1000000 ||
        !read_at(needs_replica.manager, 0x40, entries) ||
        (count && !readable(entries, static_cast<size_t>(count) * sizeof(void*))))
        return fail(error, "needs apply rejected: bounded subscriber lookup storage is invalid");
    void* resolved = nullptr;
    const DWORD lookup_exception = find_subscriber_native(needs_replica.manager, serialized_id, resolved);
    if (lookup_exception || resolved != needs_replica.subscriber) {
        needs_reject("subscriber no longer resolves to the bound identity");
        return fail(error, "needs apply rejected: identity lookup exception=" + std::to_string(lookup_exception));
    }
    auto* destination = static_cast<unsigned char*>(needs_replica.needs) + 0x18;
    MEMORY_BASIC_INFORMATION memory{};
    if (!readable(destination, sizeof(SubscriberNeeds)) || !mt2mp::game::live_host_state::apply_timing::virtual_query(destination, &memory, sizeof(memory)) ||
        !(memory.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
        return fail(error, "needs apply rejected: complete seven-value destination is not writable");
    needs_replica.permit_index = -1;
    needs_replica.permit_hits = 0;
    needs_replica.permit = true;
    LOG_INFO("[Native] Needs apply attempted id={} epoch={} revision={}", serialized_id, epoch, revision);
    receipt.native_exception = set_needs_native(needs_replica.needs, host_needs, receipt);
    needs_replica.permit = false;
    needs_replica.permit_index = -1;
    const bool identity = needs_binding_valid();
    receipt.after_readable = identity && needs_values_read(needs_replica.needs, receipt.after);
    if (receipt.native_exception || !identity || !receipt.after_readable || receipt.after != host_needs ||
        needs_replica.permit_hits != 7 || receipt.completed_setters != 7) {
        needs_reject("native setter, permit accounting, identity or readback failed; partial writes may remain");
        return fail(error, "needs apply failed: exception=" + std::to_string(receipt.native_exception) +
            " attempted=" + std::to_string(receipt.attempted_setters) + " completed=" + std::to_string(receipt.completed_setters) +
            " permitted=" + std::to_string(needs_replica.permit_hits) + " readback=" + std::to_string(receipt.after_readable));
    }
    needs_replica.revision = revision;
    needs_replica.applied = host_needs;
    needs_replica.applied_before = expected_before;
    receipt.written = true;
    LOG_INFO("[Native] Needs apply confirmed id={} epoch={} revision={}", serialized_id, epoch, revision);
    return true;
}

bool read_subscriber_needs_replica_state(SubscriberNeedsReplicaState& state, std::string& error) {
    state = {};
    if (!check_thread(error)) return false;
    state.armed = needs_replica.armed;
    state.epoch = needs_replica.epoch;
    state.applied_revision = needs_replica.revision;
    state.serialized_id = needs_replica.id;
    state.installed_hook_mask = needs_hook_mask.load();
    state.initial = needs_replica.initial;
    state.applied = needs_replica.applied;
    for (size_t index = 0; index < state.writes.size(); ++index) {
        const auto& source = needs_write_counters[index];
        state.writes[index] = {source.entered.load(), source.forwarded.load(), source.selected.load(),
            source.suppressed.load(), source.permitted.load(), source.guard_rejected.load(), source.wrong_thread.load()};
    }
    for (size_t index = 0; index < state.ticks.size(); ++index) {
        const auto& source = needs_tick_counters[index];
        state.ticks[index] = {source.entered.load(), source.forwarded.load(), source.guard_rejected.load()};
    }
    state.bound = needs_replica.armed && needs_binding_valid();
    if (state.bound && !subscriber_login_read(needs_replica.subscriber, state.logged_in)) {
        needs_reject("native login state is unreadable or invalid");
        state.bound = false;
    }
    state.error = needs_replica.error;
    return true;
}

bool read_clock(ClockState& state, std::string& error) {
    if (!check_thread(error)) return false;
    const auto* clock = singleton(clock_slot);
    if (!clock || !read_at(clock, 0x58, state.time_scale) || !read_at(clock, 0x5c, state.pause_scale) ||
        !read_at(clock, 0x44, state.flat_day) || !read_at(clock, 0x48, state.seconds))
        return fail(error, "clock read rejected: native clock singleton is absent or unreadable");
    if (!std::isfinite(state.time_scale) || !std::isfinite(state.pause_scale))
        return fail(error, "clock read rejected: native scales are not finite");
    state.paused = state.time_scale == 0 || state.pause_scale == 0;
    return true;
}

bool pause_load_clock(void* receiver, bool& changed, ClockState& actual, std::string& error) {
    changed = false;
    actual = {};
    if (!check_thread(error)) return false;
    if (!receiver || receiver != singleton(clock_slot))
        return fail(error, "load pause rejected: clock receiver differs from singleton");
    if (!has_vtable(receiver, 0x17499c0))
        return fail(error, "load pause rejected: clock receiver vtable differs");
    int32_t mode_state = -1;
    if (!read_at(singleton(mode_slot), 0x2c4, mode_state))
        return fail(error, "load pause rejected: mode state is unreadable");
    if (mode_state != 4)
        return fail(error, "load pause rejected: mode state=" + std::to_string(mode_state) + " required=4");
    if (!in_game()) return fail(error, "load pause rejected: active world identity or lifecycle differs");
    if (!read_clock(actual, error)) return false;
    if (actual.time_scale < 0 || actual.pause_scale < 0)
        return fail(error, "load pause rejected: native scales are negative");
    if (actual.time_scale == 0) return true;
    const float before = actual.time_scale;
    std::string write_error;
    const bool written = set_clock(true, 1.0f, write_error);
    std::string read_error;
    if (!read_clock(actual, read_error)) {
        actual = {};
        return fail(error, "load pause readback failed: " + read_error +
            (written ? "" : "; native write cause: " + write_error));
    }
    changed = actual.time_scale != before;
    if (!written) return fail(error, "load pause failed: " + write_error);
    if (actual.time_scale != 0 || !actual.paused)
        return fail(error, "load pause readback differs: time_scale=" + std::to_string(actual.time_scale) + " required=0");
    return true;
}

bool read_prop(uint64_t uid, PropState& state, std::string& error) {
    if (!check_thread(error)) return false;
    if (!in_game()) return fail(error, "prop read rejected: no active world");
    void* prop = nullptr;
    const DWORD exception = find_prop(uid, prop);
    if (exception) return fail(error, "prop lookup failed: uid=" + std::to_string(uid) + " exception=" + std::to_string(exception));
    if (!prop_state(prop, state) || state.uid != uid)
        return fail(error, "prop read rejected: UID missing, wrong type, unreadable or identity mismatch uid=" + std::to_string(uid));
    return true;
}

bool list_props(size_t limit, std::vector<PropState>& states, std::string& error) {
    if (!check_thread(error)) return false;
    states.clear();
    if (!in_game()) return fail(error, "prop inventory rejected: no active world");
    if (!limit || limit > 128) return fail(error, "prop inventory rejected: limit must be 1..128");
    void* node = nullptr;
    if (!read_at(reinterpret_cast<void*>(image.load()), 0x1a363d0, node))
        return fail(error, "prop inventory rejected: object registry is unreadable");
    size_t visited = 0;
    while (node && states.size() < limit) {
        if (++visited > 1000000) return fail(error, "prop inventory rejected: registry traversal exceeded 1000000 nodes");
        void* object = nullptr;
        void* next = nullptr;
        uint64_t uid = 0;
        if (!read_at(node, 0, next) || !read_at(node, 0x10, object) || !read_at(object, 0x18, uid))
            return fail(error, "prop inventory rejected: registry node is unreadable");
        void* prop = nullptr;
        const DWORD exception = find_prop(uid, prop);
        if (exception) return fail(error, "prop inventory lookup failed: uid=" + std::to_string(uid) + " exception=" + std::to_string(exception));
        PropState state;
        if (is_world_prop(prop)) {
            if (!prop_state(prop, state) || state.uid != uid)
                return fail(error, "prop inventory rejected: object identity or coordinates invalid");
            states.push_back(state);
        }
        node = next;
    }
    return true;
}

bool read_building(uint64_t uid, BuildingState& state, std::string& error) {
    if (!check_thread(error)) return false;
    if (!in_game()) return fail(error, "building read rejected: no active world");
    state = {};
    const std::string target = " uid=" + std::to_string(uid);
    auto reject = [&](const char* cause) { return fail(error, std::string("building read rejected: ") + cause + target); };
    void* building = nullptr;
    bool object_found = false;
    const DWORD exception = find_typed_object(uid, 0x1655090, building, &object_found);
    if (exception) return fail(error, "building lookup failed: exception=" + std::to_string(exception) + target);
    if (!building) return reject(object_found ? "typed_uid_wrong_type" : "typed_uid_missing");
    void* active_shard = nullptr;
    void* shard = nullptr;
    void* region = nullptr;
    if (!read_at(singleton(0x13df440), 0x60, active_shard)) return reject("outer_shard_unreadable");
    if (!active_shard) return reject("outer_shard_missing");
    if (!read_at(building, 0x170, shard)) return reject("building_shard_unreadable");
    if (!read_at(building, 0x178, region)) return reject("region_unreadable");
    if (!region) return reject("region_missing");
    if (shard != active_shard) {
        if (!has_vtable(shard, 0x174ad10) || !has_vtable(active_shard, 0x174ad10))
            return reject("outer_shard_mismatch: shard_vtable_invalid");
        int32_t shard_type = -1;
        if (!read_at(shard, 0x48, shard_type)) return reject("dungeon_shard_type_unreadable");
        if (shard_type != 1 && shard_type != 2) return reject("dungeon_shard_type_not1_or2");
        int32_t root_type = -1;
        if (!read_at(active_shard, 0x48, root_type) || root_type != 0)
            return reject("dungeon_root_shard_type_not0");
        void* dungeon = nullptr;
        void* instance = nullptr;
        if (shard_type == 2) {
            if (!read_at(shard, 0x20, instance) || !has_vtable(instance, 0x1719440))
                return reject("dungeon_instance_receiver_invalid");
            void* typed_instance = nullptr;
            const DWORD instance_exception = cast_dungeon_instance(instance, typed_instance);
            if (instance_exception) return fail(error, "dungeon instance RTTI lookup failed: exception=" + std::to_string(instance_exception) + target);
            if (typed_instance != instance) return reject("dungeon_instance_rtti_mismatch");
            void* reverse_shard = nullptr;
            void* reverse_region = nullptr;
            if (!read_at(instance, 0x38, reverse_shard) || reverse_shard != shard)
                return reject("dungeon_instance_shard_reciprocity_mismatch");
            if (!read_at(instance, 0x40, reverse_region) || reverse_region != region)
                return reject("dungeon_instance_region_reciprocity_mismatch");
            if (!read_at(instance, 0x30, dungeon)) return reject("dungeon_instance_owner_unreadable");
        } else if (!read_at(shard, 0x18, dungeon)) {
            return reject("dungeon_receiver_invalid");
        }
        if (!has_vtable(dungeon, 0x16c0550))
            return reject("dungeon_receiver_invalid");
        void* typed_dungeon = nullptr;
        const DWORD dungeon_exception = cast_dungeon(dungeon, typed_dungeon);
        if (dungeon_exception) return fail(error, "dungeon RTTI lookup failed: exception=" + std::to_string(dungeon_exception) + target);
        if (typed_dungeon != dungeon) return reject("dungeon_rtti_mismatch");
        void* reverse_shard = nullptr;
        void* reverse_region = nullptr;
        if (!read_at(dungeon, 0x48, reverse_shard) || !reverse_shard || (shard_type == 1 && reverse_shard != shard))
            return reject("dungeon_shard_reciprocity_mismatch");
        if (!read_at(dungeon, 0x50, reverse_region) || !reverse_region || (shard_type == 1 && reverse_region != region))
            return reject("dungeon_region_reciprocity_mismatch");
        if (shard_type == 2) {
            int32_t template_type = -1;
            void* template_owner = nullptr;
            if (reverse_shard == shard || !has_vtable(reverse_shard, 0x174ad10) ||
                !read_at(reverse_shard, 0x48, template_type) || template_type != 1 ||
                !read_at(reverse_shard, 0x18, template_owner) || template_owner != dungeon)
                return reject("dungeon_instance_template_shard_reciprocity_mismatch");
            void* instances = nullptr;
            int32_t count = 0, capacity = 0;
            if (!read_at(dungeon, 0x4c0, instances) || !read_at(dungeon, 0x4c8, count) || !read_at(dungeon, 0x4cc, capacity))
                return reject("dungeon_instance_owner_list_unreadable");
            if (!instances || count <= 0 || count > 1000000 || capacity < count)
                return fail(error, "building read rejected: dungeon_instance_owner_list_bounds count=" + std::to_string(count) +
                    " capacity=" + std::to_string(capacity) + " required_count=1..1000000 capacity>=count" + target);
            int occurrences = 0;
            for (int32_t i = 0; i < count; ++i) {
                void* candidate = nullptr;
                if (!read_at(instances, size_t(i) * sizeof(void*), candidate)) return reject("dungeon_instance_owner_list_entry_unreadable");
                if (candidate == instance && ++occurrences > 1) return reject("dungeon_instance_owner_list_duplicate");
            }
            if (occurrences != 1) return reject("dungeon_instance_not_in_owner_list");
            void* final_instances = nullptr;
            int32_t final_count = 0, final_capacity = 0;
            if (!read_at(dungeon, 0x4c0, final_instances) || !read_at(dungeon, 0x4c8, final_count) ||
                !read_at(dungeon, 0x4cc, final_capacity) || final_instances != instances || final_count != count || final_capacity != capacity)
                return reject("dungeon_instance_owner_list_changed_during_read");
        }
        void* exterior = nullptr;
        if (!read_at(dungeon, 0x6f8, exterior) || !exterior) return reject("dungeon_exterior_missing");
        if (exterior == building) return reject("dungeon_exterior_cycle");
        uint64_t exterior_uid = 0;
        if (!read_at(exterior, 0x18, exterior_uid)) return reject("dungeon_exterior_uid_unreadable");
        void* typed_exterior = nullptr;
        const DWORD exterior_exception = find_typed_object(exterior_uid, 0x1655090, typed_exterior);
        if (exterior_exception) return fail(error, "dungeon exterior lookup failed: exception=" + std::to_string(exterior_exception) + target);
        if (typed_exterior != exterior) return reject("dungeon_exterior_registry_mismatch");
        void* reverse_dungeon = nullptr;
        void* exterior_shard = nullptr;
        if (!read_at(exterior, 0xa38, reverse_dungeon) || reverse_dungeon != dungeon)
            return reject("dungeon_exterior_reciprocity_mismatch");
        if (!read_at(exterior, 0x170, exterior_shard) || exterior_shard != active_shard)
            return reject("dungeon_exterior_not_direct_root");
        // The direct-root check bounds this nested read to a single exterior building.
        BuildingState exterior_state;
        if (!read_building(exterior_uid, exterior_state, error)) {
            const std::string cause = error;
            return fail(error, "building dungeon exterior read failed: " + cause + target);
        }
        if (exterior_state.type != "DungeonExterior") return reject("dungeon_exterior_type_mismatch");
        state.dungeon_membership = true;
        state.dungeon_instance_membership = shard_type == 2;
        state.exterior_uid = exterior_uid;
    }
    if (!read_at(building, 0x18, state.prop.uid)) return reject("uid_unreadable");
    if (state.prop.uid != uid) return reject("uid_mismatch");
    if (!read_at(building, 0x110, state.prop.x) || !read_at(building, 0x114, state.prop.y) ||
        !read_at(building, 0x118, state.prop.z)) return reject("transform_unreadable");
    if (!std::isfinite(state.prop.x) || !std::isfinite(state.prop.y) || !std::isfinite(state.prop.z))
        return reject("transform_nonfinite");
    for(size_t index=0;index<state.rotation.size();++index) {
        if(!read_at(building,0x100+index*sizeof(float),state.rotation[index])||!std::isfinite(state.rotation[index]))
            return reject("rotation_unreadable_or_nonfinite");
    }
    for(size_t index=0;index<state.scale.size();++index) {
        if(!read_at(building,0x11c+index*sizeof(float),state.scale[index])||!std::isfinite(state.scale[index]))
            return reject("scale_unreadable_or_nonfinite");
    }
    if (!read_game_string(building, 0x768, state.type)) return reject("type_string_unreadable");
    if (!read_game_string(building, 0x788, state.variant)) return reject("variant_string_unreadable");
    if (state.dungeon_membership) {
        void* final_root = nullptr;
        if (!in_game() || !read_at(singleton(0x13df440), 0x60, final_root) || final_root != active_shard)
            return reject("dungeon_loaded_world_changed_during_read");
    }
    return true;
}

bool list_world_props(size_t limit, std::vector<WorldPropState>& states, std::string& error) {
    states.clear();
    std::vector<PropState> props;
    if (!list_props(limit, props, error)) return false;
    for (const auto& prop : props) {
        WorldPropState state;
        state.prop = prop;
        void* object = nullptr;
        DWORD exception = find_prop(prop.uid, object);
        if (exception) return fail(error, "world inventory lookup failed: exception=" + std::to_string(exception));
        void* region = nullptr;
        unsigned char active = 0;
        if (!read_at(object, 0x178, region) || !read_at(region, 0x850, active) || active > 1)
            return fail(error, "world inventory rejected: region activity unreadable or invalid");
        state.region_active = active != 0;
        void* building = nullptr;
        exception = find_typed_object(prop.uid, 0x1655090, building);
        if (exception) return fail(error, "world inventory building lookup failed: exception=" + std::to_string(exception));
        state.building = building != nullptr;
        if (building && (!read_game_string(building, 0x768, state.type) ||
            !read_game_string(building, 0x788, state.variant)))
            return fail(error, "world inventory rejected: building catalog identifier unreadable");
        states.push_back(std::move(state));
    }
    return true;
}

bool read_simulation_state(SimulationState& state, std::string& error) {
    if (!check_thread(error)) return false;
    state = {};
    if (!in_game()) return fail(error, "simulation state rejected: no active world");
    const auto* business = singleton(0x13df220);
    const auto* subscribers = singleton(0x13df690);
    if (!has_vtable(singleton(clock_slot), 0x17499c0) || !has_vtable(business, 0x16d97d0) ||
        !has_vtable(subscribers, 0x1725f30))
        return fail(error, "simulation state rejected: clock/business/subscriber identity differs");
    if (!read_clock(state.clock, error)) return false;
    void* record = nullptr;
    if (!read_at(business, 0x38, state.cash) || !read_at(business, 0x100, record) ||
        !read_at(subscribers, 0x90, state.logged_in_subscribers) || state.logged_in_subscribers < 0)
        return fail(error, "simulation state rejected: balance, ledger or subscriber count unreadable/invalid");
    if (record) {
        if (!has_vtable(record, 0x1712bf0) || !read_at(record, 0x1c, state.record_day) ||
            !read_at(record, 0x20, state.record_cash) || !read_at(record, 0x28, state.record_income) ||
            !read_at(record, 0x2c, state.record_expense))
            return fail(error, "simulation state rejected: current ledger identity or fields differ");
        state.record_present = true;
        for (size_t i = 0; i < state.line_counts.size(); ++i) {
            const auto* item = static_cast<const unsigned char*>(record) + 0x30 + i * 0x48;
            int32_t category = -1;
            if (!has_vtable(item, 0x171d0c0) || !read_at(item, 0x20, category) || category != int32_t(i) ||
                !read_at(item, 0x18, state.line_counts[i]) || state.line_counts[i] < 0 ||
                !read_at(item, 0x1c, state.line_income[i]))
                return fail(error, "simulation state rejected: ledger line identity/count differs; category=" + std::to_string(i));
        }
    }
    return true;
}

bool diagnostic_reconcile_building_ledger(const SimulationState& expected_before,
    const SimulationState& host, SimulationState& local, std::string& error) {
    if (!read_simulation_state(local, error)) return false;
    std::string folder;
    if (!read_game_string(singleton(game_slot), 0x470, folder) || !valid_test_name(folder))
        return fail(error, "ledger reconcile rejected: active MPTest world required");
    if (!expected_before.record_present || expected_before.clock.flat_day != local.clock.flat_day ||
        expected_before.cash != local.cash || expected_before.record_day != local.record_day ||
        expected_before.record_cash != local.record_cash || expected_before.record_income != local.record_income ||
        expected_before.record_expense != local.record_expense || expected_before.line_counts != local.line_counts ||
        expected_before.line_income != local.line_income)
        return fail(error, "ledger reconcile rejected: local ledger differs from expected host state before placement");
    if (!host.record_present || !local.record_present || host.clock.flat_day != local.clock.flat_day ||
        host.record_day != local.record_day)
        return fail(error, "ledger reconcile rejected: clock/record day differs; host_clock=" + std::to_string(host.clock.flat_day) +
            " local_clock=" + std::to_string(local.clock.flat_day) + " host_record=" + std::to_string(host.record_day) +
            " local_record=" + std::to_string(local.record_day));
    if (local.clock.time_scale != 0)
        return fail(error, "ledger reconcile rejected: local time_scale=" + std::to_string(local.clock.time_scale) + " required=0");
    const int64_t amount = int64_t(local.cash) - host.cash;
    if (amount <= 0 || amount > INT32_MAX || local.cash != local.record_cash || host.cash != host.record_cash ||
        int64_t(host.record_expense) - local.record_expense != amount || host.record_income != local.record_income ||
        int64_t(host.line_counts[6]) - local.line_counts[6] != 1 ||
        int64_t(local.line_income[6]) - host.line_income[6] != amount)
        return fail(error, "ledger reconcile rejected: state does not describe one missing category6 expense; cash_delta=" + std::to_string(amount));
    for (size_t i = 0; i < local.line_counts.size(); ++i) {
        if (i != 6 && (local.line_counts[i] != host.line_counts[i] || local.line_income[i] != host.line_income[i]))
            return fail(error, "ledger reconcile rejected: unrelated category differs; category=" + std::to_string(i));
    }
    int32_t saves = -1, edits = -1;
    if (!read_saves_in_progress(saves, error)) return false;
    if (!read_at(reinterpret_cast<void*>(image.load()), 0x13beda0, edits) || saves != 0 || edits != 0)
        return fail(error, "ledger reconcile rejected: save/edit busy; saves=" + std::to_string(saves) + " edits=" + std::to_string(edits));
    void* record_before = nullptr;
    void* record_after = nullptr;
    void* business = singleton(0x13df220);
    if (!read_at(business, 0x100, record_before)) return fail(error, "ledger reconcile rejected: current record unreadable");
    bool applied = false;
    LOG_INFO("[Native] Building ledger reconcile attempted amount={} record_day={}", amount, local.record_day);
    const DWORD exception = subtract_building_expense(business, int32_t(amount), applied);
    if (exception || !applied)
        return fail(error, "ledger reconcile failed: exception=" + std::to_string(exception) + " applied=" + std::to_string(applied));
    if (!read_simulation_state(local, error)) return false;
    if (!read_at(business, 0x100, record_after) || record_before != record_after || local.cash != host.cash ||
        local.record_day != host.record_day || local.record_cash != host.record_cash ||
        local.record_income != host.record_income || local.record_expense != host.record_expense ||
        local.line_counts != host.line_counts || local.line_income != host.line_income)
        return fail(error, "ledger reconcile readback differs after native expense; do not replay without fresh state");
    LOG_INFO("[Native] Building ledger balance, totals and 20 categories read back; record identity retained");
    return true;
}

bool read_camera_state(CameraState& state, std::string& error) {
    if (!check_thread(error)) return false;
    state = {};
    if (!in_game()) return fail(error, "camera read rejected: no active world");
    void* camera = nullptr;
    void* edit = nullptr;
    void* weight = nullptr;
    if (!read_at(singleton(game_slot), 0x90, camera) || !has_vtable(camera, 0x1786c10))
        return fail(error, "camera read rejected: native camera identity differs");
    if (!read_at(camera, 0x128, edit) || !has_vtable(edit, 0x16e4cd0))
        return fail(error, "camera read rejected: edit camera identity differs");
    unsigned char pending = 0;
    if (!read_at(camera, 0x18, state.x) || !read_at(camera, 0x1c, state.y) || !read_at(camera, 0x20, state.z) ||
        !read_at(edit, 0x98, state.focus_x) || !read_at(edit, 0x9c, state.focus_y) || !read_at(edit, 0xa0, state.focus_z) ||
        !read_at(edit, 0x3dc, state.distance) || !read_at(edit, 0xb4, state.minimum_distance) ||
        !read_at(edit, 0x3d0, state.maximum_distance) || !read_at(camera, 0x130, weight) ||
        !read_at(weight, 8, state.edit_weight) || !read_at(edit, 0x494, pending) || pending > 1)
        return fail(error, "camera read rejected: native position, focus, zoom or script fields unreadable");
    const float values[] = {state.x, state.y, state.z, state.focus_x, state.focus_y, state.focus_z,
        state.distance, state.minimum_distance, state.maximum_distance, state.edit_weight};
    if (!std::all_of(std::begin(values), std::end(values), [](float v) { return std::isfinite(v); }) ||
        state.minimum_distance < 0 || state.maximum_distance <= state.minimum_distance ||
        state.edit_weight < 0 || state.edit_weight > 1.001f)
        return fail(error, "camera read rejected: native values invalid; position=" + std::to_string(state.x) + "," +
            std::to_string(state.y) + "," + std::to_string(state.z) + " focus=" + std::to_string(state.focus_x) + "," +
            std::to_string(state.focus_y) + "," + std::to_string(state.focus_z) + " distance=" + std::to_string(state.distance) +
            " minimum_distance=" + std::to_string(state.minimum_distance) + " maximum_distance=" + std::to_string(state.maximum_distance) +
            " edit_weight=" + std::to_string(state.edit_weight) + " required=min>=0,max>min,weight[0,1.001],finite");
    state.focus_pending = pending != 0;
    if (!read_at(edit, 0x488, state.focus_blend) || !read_at(edit, 0x484, state.focus_end_blend))
        return fail(error, "camera read rejected: focus blend fields unreadable");
    if (!std::isfinite(state.focus_blend) || !std::isfinite(state.focus_end_blend) ||
        state.focus_blend < 0 || state.focus_blend > 1 || state.focus_end_blend < 0 || state.focus_end_blend > 1)
        return fail(error, "camera read rejected: focus_blend=" + std::to_string(state.focus_blend) +
            " focus_end_blend=" + std::to_string(state.focus_end_blend) + " required=finite,[0,1]");
    state.focus_target_available = state.focus_pending || state.focus_blend > 0;
    if (state.focus_target_available) {
        if (!read_at(edit, 0x46c, state.target_x) || !read_at(edit, 0x470, state.target_y) ||
            !read_at(edit, 0x474, state.target_z) || !read_at(edit, 0x47c, state.target_distance))
            return fail(error, "camera read rejected: active focus target unreadable");
        if (!std::isfinite(state.target_x) || !std::isfinite(state.target_y) || !std::isfinite(state.target_z) ||
            !std::isfinite(state.target_distance))
            return fail(error, "camera read rejected: active focus target invalid; x=" + std::to_string(state.target_x) +
                " y=" + std::to_string(state.target_y) + " z=" + std::to_string(state.target_z) +
                " distance=" + std::to_string(state.target_distance) + " required=finite");
    }
    std::array<float, 200> zoom_table{};
    if (!read_at(edit, 0xb0, state.zoom_index) || !read_at(edit, 0x3e0, state.desired_distance) ||
        !read_at(edit, 0xb4, zoom_table))
        return fail(error, "camera read rejected: native zoom index, desired distance or table unreadable");
    if (state.zoom_index < 0 || state.zoom_index >= static_cast<int32_t>(zoom_table.size()))
        return fail(error, "camera read rejected: zoom_index=" + std::to_string(state.zoom_index) + " required=[0,199]");
    for (size_t i = 0; i < zoom_table.size(); ++i) {
        if (!std::isfinite(zoom_table[i]) || zoom_table[i] < 0 || (i && zoom_table[i] <= zoom_table[i - 1]))
            return fail(error, "camera read rejected: zoom table invalid at index=" + std::to_string(i) +
                " value=" + std::to_string(zoom_table[i]) + " required=finite,nonnegative,strictly_increasing");
    }
    state.zoom_table_value = zoom_table[state.zoom_index];
    state.zoom_lower_neighbor = zoom_table[state.zoom_index > 0 ? state.zoom_index - 1 : 0];
    if (!std::isfinite(state.desired_distance) || state.desired_distance != state.zoom_table_value)
        return fail(error, "camera read rejected: desired_distance=" + std::to_string(state.desired_distance) +
            " table_value=" + std::to_string(state.zoom_table_value) + " required=equal");
    if (state.focus_target_available && state.target_distance >= zoom_table.front() &&
        state.target_distance <= zoom_table.back()) {
        const auto target = std::lower_bound(zoom_table.begin(), zoom_table.end(), state.target_distance);
        state.target_zoom_index = static_cast<int32_t>(target - zoom_table.begin());
        state.representable_target = *target;
    } else if (state.focus_target_available) {
        return fail(error, "camera read rejected: active target_distance=" + std::to_string(state.target_distance) +
            " required=finite,within_native_zoom_table");
    }
    return true;
}

bool diagnostic_focus_camera(float x, float y, float z, float distance, CameraState& state, std::string& error) {
    if (!read_camera_state(state, error)) return false;
    std::string folder;
    if (!read_game_string(singleton(game_slot), 0x470, folder) || !net::snapshot_world_path::valid_folder(folder))
        return fail(error, "camera focus rejected: active profile world required");
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(distance))
        return fail(error, "camera focus rejected: finite target and distance required");
    if (distance <= 0 || distance < state.minimum_distance || distance > state.maximum_distance)
        return fail(error, "camera focus rejected: distance=" + std::to_string(distance) +
            " native_min=" + std::to_string(state.minimum_distance) + " native_max=" + std::to_string(state.maximum_distance));
    if (state.edit_weight < 0.999f)
        return fail(error, "camera focus rejected: edit script weight=" + std::to_string(state.edit_weight) + " required>=0.999");
    void* region = nullptr;
    const DWORD lookup_exception = find_region(x, z, region);
    if (lookup_exception || !region)
        return fail(error, "camera focus rejected: target region missing; exception=" + std::to_string(lookup_exception));
    void* camera = nullptr;
    void* dungeon = nullptr;
    if (!read_at(singleton(game_slot), 0x90, camera) || !read_at(camera, 0x138, dungeon) ||
        !has_vtable(dungeon, 0x1706640))
        return fail(error, "camera focus rejected: dungeon script identity differs");
    LOG_INFO("[Native] Diagnostic camera focus attempted x={} y={} z={} distance={}", x, y, z, distance);
    const DWORD exception = focus_camera_native(camera, x, y, z, distance);
    if (exception) return fail(error, "camera focus call failed: exception=" + std::to_string(exception));
    if (!read_camera_state(state, error)) return false;
    if (!state.focus_pending || !state.focus_target_available || state.target_x != x || state.target_y != y ||
        state.target_z != z || state.target_distance != distance || state.focus_end_blend != 1)
        return fail(error, "camera focus target readback differs after native request");
    LOG_INFO("[Native] Diagnostic camera target readback confirmed; motion pending");
    return true;
}

bool read_region_state(float x, float z, RegionState& state, std::string& error) {
    if (!check_thread(error)) return false;
    state = {};
    if (!in_game() || !std::isfinite(x) || !std::isfinite(z))
        return fail(error, "region read rejected: active world and finite position required");
    void* region = nullptr;
    const DWORD exception = find_region(x, z, region);
    if (exception) return fail(error, "region lookup failed: exception=" + std::to_string(exception));
    if (!region) return fail(error, "region read rejected: no region at position");
    const auto* business = singleton(0x13df220);
    void* config = nullptr;
    void* manager = nullptr;
    unsigned char owned = 0;
    if (!has_vtable(region, 0x1787920) || !read_at(region, 0x830, state.type) ||
        !read_at(region, 0x850, owned) || owned > 1)
        return fail(error, "region read rejected: native identity/type/ownership differs");
    state.owned = owned != 0;
    if (!read_at(region, 0x950, manager)) return fail(error, "region read rejected: building manager field unreadable");
    state.building_manager_present = manager != nullptr;
    if (!read_at(region, 0x9a0, manager)) return fail(error, "region read rejected: path manager field unreadable");
    state.path_manager_present = manager != nullptr;
    if (!has_vtable(business, 0x16d97d0) || !read_at(business, 0x108, config) ||
        !has_vtable(config, 0x1712b30) || !read_at(config, 0x24, state.purchase_price) ||
        !read_at(business, 0x38, state.cash))
        return fail(error, "region read rejected: native business configuration differs");
    return true;
}

bool diagnostic_purchase_region(float x, float z, RegionState& state, std::string& error) {
    if (!read_region_state(x, z, state, error)) return false;
    std::string folder;
    if (!read_game_string(singleton(game_slot), 0x470, folder) || !valid_test_name(folder))
        return fail(error, "region purchase rejected: active MPTest world required");
    if (state.type == 0) return fail(error, "region purchase rejected: native region type is zero");
    if (state.owned) return fail(error, "region purchase rejected: region already owned");
    if (state.purchase_price < 0) return fail(error, "region purchase rejected: native purchase price is negative");
    int32_t edits = -1, saves = -1;
    unsigned char locked = 1;
    if (!read_at(reinterpret_cast<void*>(image.load()), 0x13beda0, edits) || edits != 0 ||
        !read_at(reinterpret_cast<void*>(image.load()), 0x1a30742, locked) || locked ||
        !read_saves_in_progress(saves, error) || saves != 0)
        return fail(error, "region purchase rejected: edit/save busy; edits=" + std::to_string(edits) +
            " locked=" + std::to_string(locked) + " saves=" + std::to_string(saves));
    void* region = nullptr;
    const DWORD exception = find_region(x, z, region);
    if (exception || !region) return fail(error, "region purchase lookup failed: exception=" + std::to_string(exception));
    const int32_t before = state.cash;
    const int32_t price = state.purchase_price;
    LOG_INFO("[Native] Diagnostic region purchase attempted x={} z={} price={} cash={}", x, z, price, before);
    const auto result = purchase_region_native(region, singleton(0x13df220), price);
    if (result.exception || !result.affordable || !result.charged)
        return fail(error, "region purchase failed: exception=" + std::to_string(result.exception) +
            " affordable=" + std::to_string(result.affordable) + " charged=" + std::to_string(result.charged));
    if (!read_region_state(x, z, state, error)) return false;
    if (!state.owned || int64_t(state.cash) != int64_t(before) - price)
        return fail(error, "region purchase readback differs: owned=" + std::to_string(state.owned) +
            " cash=" + std::to_string(state.cash) + " expected_cash=" + std::to_string(int64_t(before) - price));
    LOG_INFO("[Native] Diagnostic region ownership/cash readback confirmed x={} z={} cash={}; persistence pending", x, z, state.cash);
    return true;
}

bool diagnostic_move_prop(const PropState& expected, const PropState& target, std::string& error) {
    if (!check_thread(error)) return false;
    std::string folder;
    if (!read_game_string(singleton(game_slot), 0x470, folder) || !valid_test_name(folder))
        return fail(error, "diagnostic move rejected: active game folder must have MPTest prefix");
    if (!target.uid || expected.uid != target.uid || !std::isfinite(target.x) ||
        !std::isfinite(target.y) || !std::isfinite(target.z))
        return fail(error, "diagnostic move rejected: target UID or coordinates invalid");
    PropState current;
    if (!read_prop(expected.uid, current, error)) return false;
    if (current.x != expected.x || current.y != expected.y || current.z != expected.z)
        return fail(error, "diagnostic move rejected: expected position differs from native state");
    int32_t edits = -1;
    int32_t saves = -1;
    unsigned char locked = 1;
    if (!read_at(reinterpret_cast<void*>(image.load()), 0x13beda0, edits) || edits != 0 ||
        !read_at(reinterpret_cast<void*>(image.load()), 0x1a30742, locked) || locked ||
        !read_saves_in_progress(saves, error) || saves != 0)
        return fail(error, "diagnostic move rejected: edit/snapshot busy; edits=" + std::to_string(edits) +
            " locked=" + std::to_string(locked) + " saves=" + std::to_string(saves));
    void* prop = nullptr;
    DWORD exception = find_prop(target.uid, prop);
    if (exception || !readable(prop, 0x16c))
        return fail(error, "diagnostic move lookup failed: exception=" + std::to_string(exception));
    alignas(16) unsigned char transform[0x6c]{};
    std::memcpy(transform, static_cast<unsigned char*>(prop) + 0x100, sizeof(transform));
    std::memcpy(transform + 0x10, &target.x, sizeof(float));
    std::memcpy(transform + 0x14, &target.y, sizeof(float));
    std::memcpy(transform + 0x18, &target.z, sizeof(float));
    transform[0x28] = 1;
    LOG_INFO("[Native] Diagnostic move attempted uid={} target=({},{},{})", target.uid, target.x, target.y, target.z);
    exception = move_prop_native(prop, transform);
    if (exception) return fail(error, "diagnostic move native call failed: exception=" + std::to_string(exception));
    if (!read_prop(target.uid, current, error)) return false;
    if (current.x != target.x || current.y != target.y || current.z != target.z)
        return fail(error, "diagnostic move coordinate readback differs from requested target");
    LOG_INFO("[Native] Diagnostic move coordinate readback confirmed uid={}; spatial/persistence acceptance pending", target.uid);
    return true;
}

static bool placement_operation(const std::string& type, const std::string& variant,
    float x, float z, bool apply, bool purchase, float expected_y, BuildingState& created, std::string& error) {
    if (!check_thread(error)) return false;
    std::string folder;
    if (!in_game() || !read_game_string(singleton(game_slot), 0x470, folder) || !net::snapshot_world_path::valid_folder(folder))
        return fail(error, "placement rejected: active profile world required");
    if (type.empty() || type.size() > 256 || variant.size() > 256 ||
        type.find('\0') != std::string::npos || variant.find('\0') != std::string::npos ||
        !std::isfinite(x) || !std::isfinite(z))
        return fail(error, "diagnostic placement rejected: catalog identifier or position invalid");
    int32_t edits = -1;
    int32_t saves = -1;
    unsigned char locked = 1;
    if (!read_at(reinterpret_cast<void*>(image.load()), 0x13beda0, edits) || edits != 0 ||
        !read_at(reinterpret_cast<void*>(image.load()), 0x1a30742, locked) || locked ||
        !read_saves_in_progress(saves, error) || saves != 0)
        return fail(error, "diagnostic placement rejected: edit/save busy; edits=" + std::to_string(edits) +
            " locked=" + std::to_string(locked) + " saves=" + std::to_string(saves));
    LOG_INFO("[Native] Diagnostic building placement attempted apply={} purchase={} type={} variant={} x={} z={}", apply, purchase, type, variant, x, z);
    const auto result = place_building_native(type.c_str(), variant.c_str(), x, z, apply, purchase, expected_y);
    const char* stages[] = {"string construction", "catalog variant unavailable", "catalog type locked",
        "footprint unavailable/invalid", "shard bounds/ground unavailable", "active region/managers unavailable",
        "building intersection", "path intersection", "purchase rejected", "building creation returned null", "completed", "replica ground differs from host",
        "region missing at position", "region activity flag unreadable/invalid", "region inactive",
        "building manager absent/unreadable", "path manager absent/unreadable",
        "root island manager/lock unavailable", "native island read-lock failed", "island hash/cell shape invalid",
        "native island lookup failed", "unwalkable: no native island at position", "native island unlock failed"};
    if (result.exception || result.island_unlock_exception || result.stage != 10)
        return fail(error, std::string("diagnostic placement failed: stage=") + stages[result.stage] +
            " exception=" + std::to_string(result.exception) + " purchase_applied=" + std::to_string(result.purchased) +
            " region_found=" + std::to_string(result.region_found) + " region_flag_readable=" + std::to_string(result.region_flag_readable) +
            " region_active=" + std::to_string(result.region_active) + " buildings_found=" + std::to_string(result.buildings_found) +
            " paths_found=" + std::to_string(result.paths_found) +
            " island_lookup_returned=" + std::to_string(result.island_lookup_returned) +
            " island_found=" + std::to_string(result.island_found) +
            " island_unlock_exception=" + std::to_string(result.island_unlock_exception) +
            " island_lock_unknown=" + std::to_string(island_lock_unknown.load()));
    if (!apply) {
        created = {{0, x, result.ground_y, z}, type, variant};
        LOG_INFO("[Native] Placement probe accepted type={} variant={} position=({},{},{})", type, variant, x, result.ground_y, z);
        return true;
    }
    uint64_t uid = 0;
    if (!read_at(result.building, 0x18, uid) || !uid)
        return fail(error, "diagnostic placement acknowledgement failed: returned building has no readable UID");
    if (!read_building(uid, created, error)) return false;
    if (created.type != type || created.variant != variant || created.prop.x != x || created.prop.z != z)
        return fail(error, "diagnostic placement acknowledgement failed: UID catalog/position readback differs");
    LOG_INFO("[Native] Diagnostic building registered uid={} type={} variant={} position=({},{},{}); spatial/persistence acceptance pending",
        uid, type, variant, created.prop.x, created.prop.y, created.prop.z);
    return true;
}

bool diagnostic_place_building(const std::string& type, const std::string& variant,
    float x, float z, BuildingState& created, std::string& error) {
    return placement_operation(type, variant, x, z, true, true, 0, created, error);
}

bool probe_building_placement(const std::string& type, const std::string& variant,
    float x, float z, PropState& grounded, std::string& error) {
    BuildingState candidate;
    if (!placement_operation(type, variant, x, z, false, false, 0, candidate, error)) return false;
    grounded = candidate.prop;
    return true;
}

bool diagnostic_replicate_building(const BuildingState& host, BuildingState& local, std::string& error) {
    if (!host.prop.uid || !std::isfinite(host.prop.y))
        return fail(error, "diagnostic replica rejected: host UID or height invalid");
    return placement_operation(host.type, host.variant, host.prop.x, host.prop.z,
        true, false, host.prop.y, local, error);
}

bool read_world_status(WorldStatus& state, std::string& error) {
    if (!check_thread(error)) return false;
    state = {};
    world_available.store(false);
    title_available.store(false);
    const auto* game = singleton(game_slot);
    const auto* mode = singleton(mode_slot);
    void* active = nullptr;
    void* title = nullptr;
    void* world = nullptr;
    unsigned char flag = 0;
    if (!has_vtable(game, 0x1743730) || !read_at(game, 0x28, active) || !read_at(game, 0x350, title) ||
        !read_at(game, 0x434, state.requested_mode_index))
        return fail(error, "world status rejected: game state is unreadable");
    if (state.requested_mode_index < 0 || state.requested_mode_index >= 30)
        return fail(error, "world status rejected: requested mode outside native table: " + std::to_string(state.requested_mode_index));
    for (int i = 0; i < 30; ++i) {
        void* candidate = nullptr;
        if (read_at(game, 0x340 + i * 8, candidate) && candidate && candidate == active)
            state.active_mode_index = i;
    }
    if (state.active_mode_index < 0)
        return fail(error, "world status rejected: active mode absent from native mode table");
    if (state.active_mode_index == 3) {
        if (active != mode || !has_vtable(mode, 0x16e6720))
            return fail(error, "world status rejected: active world mode identity differs");
        if (!read_at(mode, 0x58, world) || !read_at(mode, 0x2c4, state.in_game_state) ||
            !read_at(mode, 0x304, flag) || !read_at(mode, 0x30c, state.saves_in_progress))
            return fail(error, "world status rejected: in-game mode is unreadable");
        state.world_present = world != nullptr;
        state.quitting = flag != 0;
        world_available.store(in_game());
    }
    if (!read_at(reinterpret_cast<void*>(image.load()), 0x13beda0, state.edit_operations))
        return fail(error, "world status rejected: edit counter is unreadable");
    if (title && active == title) {
        if (!has_vtable(title, 0x16dfba0))
            return fail(error, "world status rejected: title mode identity differs");
        void* window = nullptr;
        if (!read_at(title, 0xd0, flag) || !read_at(title, 0x20, window))
            return fail(error, "world status rejected: title mode is unreadable");
        state.title_transition_pending = flag != 0;
        title_available.store(!state.title_transition_pending);
        if (readable(window, 0x691)) {
            void* task = nullptr;
            void* layout = nullptr;
            if (!read_at(window, 0x688, task) || !read_at(window, 0x690, flag) || !read_at(window, 0x678, layout))
                return fail(error, "world status rejected: creation model is unreadable");
            state.creation_model_available = true;
            state.map_task_pending = task != nullptr;
            state.map_dirty = flag != 0;
            if (layout && !read_at(layout, 0x80, state.map_regions))
                state.creation_model_available = false;
        }
    }
    if (state.active_mode_index == 26) {
        if (!has_vtable(active, 0x171acc0) || !read_at(active, 0x21, flag) || flag > 1)
            return fail(error, "world status rejected: crash mode identity or pending flag differs");
        state.crash_report_active = true;
        state.crash_report_dismiss_pending = flag != 0;
    }
    return true;
}

bool request_dismiss_crash_report(std::string& error) {
    WorldStatus state;
    if (!read_world_status(state, error)) return false;
    if (!state.crash_report_active)
        return fail(error, "crash report dismissal rejected: active mode must be 26");
    if (state.crash_report_dismiss_pending)
        return fail(error, "crash report dismissal rejected: transition already pending");
    void* mode = nullptr;
    unsigned char uploading = 0;
    if (!read_at(singleton(game_slot), 0x28, mode) || !read_at(mode, 0x20, uploading) || uploading)
        return fail(error, "crash report dismissal rejected: upload state must be idle");
    LOG_INFO("[Native] Crash report dismissal attempted command=Cancel");
    const DWORD exception = call_string(0x4913b0, mode, "Cancel");
    if (exception) return fail(error, "crash report dismissal failed: exception=" + std::to_string(exception));
    unsigned char pending = 0;
    if (!read_at(mode, 0x20, uploading) || uploading || !read_at(mode, 0x21, pending) || pending != 1)
        return fail(error, "crash report dismissal failed: native transition readback differs");
    LOG_INFO("[Native] Crash report dismissal transition requested; title activation pending");
    return true;
}

bool set_clock(bool paused, float speed, std::string& error) {
    if (!check_thread(error)) return false;
    if (!std::isfinite(speed) || speed < 0) return fail(error, "clock write rejected: speed must be finite and nonnegative");
    auto* clock = singleton(clock_slot);
    if (!in_game() || !readable(clock, 0x60)) return fail(error, "clock write rejected: no active world/clock");
    const float target = paused ? 0.0f : speed;
    LOG_INFO("[Native] Clock change attempted paused={} speed={}", paused, target);
    const DWORD exception = call_float(clock, target);
    if (exception) return fail(error, "clock native call failed: exception=" + std::to_string(exception));
    float actual = -1;
    if (!read_at(clock, 0x58, actual) || actual != target)
        return fail(error, "clock readback differs: actual=" + std::to_string(actual) + " required=" + std::to_string(target));
    LOG_INFO("[Native] Clock scale readback confirmed={}", actual);
    return true;
}

bool read_initial_save_world(void* receiver, std::string& folder, std::string& error) {
    folder.clear();
    if(!check_thread(error))return false;
    const auto* game=singleton(game_slot);
    const auto* mode=singleton(mode_slot);
    void* active=nullptr;void* world=nullptr;int state=-1;unsigned char quitting=1;
    if(!has_vtable(game,0x1743730) || !read_at(game,0x28,active) || active!=mode || receiver!=mode ||
        !has_vtable(mode,0x16e6720) || !read_at(mode,0x58,world) || !world ||
        !read_at(mode,0x2c4,state) || state!=3 || !read_at(mode,0x304,quitting) || quitting ||
        !read_game_string(game,0x470,folder) || !net::snapshot_world_path::valid_folder(folder)) {
        folder.clear();return fail(error,"initial save requires the exact current state3 world and folder");
    }
    error.clear();return true;
}

bool read_active_world_folder(std::string& folder, std::string& error) {
    folder.clear();
    if (!check_thread(error)) return false;
    if (!in_game() || !read_game_string(singleton(game_slot), 0x470, folder) ||
        !net::snapshot_world_path::valid_folder(folder)) {
        folder.clear();
        return fail(error, "snapshot world folder unavailable or invalid on active native world");
    }
    error.clear();
    return true;
}

bool capture_snapshot(const std::string& filename, std::string& error) {
    if (!check_thread(error)) return false;
    const std::filesystem::path path(filename);
    if (path.is_absolute() || filename.rfind("user/save/", 0) != 0 ||
        path.extension() != ".vrz" || filename.find("..") != std::string::npos ||
        filename.find_first_of("\\:\r\n\t") != std::string::npos || filename.find('\0') != std::string::npos)
        return fail(error, "snapshot rejected: required virtual path user/save/<game-folder>/<filename>.vrz");
    if (!in_game()) return fail(error, "snapshot rejected: no active world");
    auto* mode = singleton(mode_slot);
    int saves = -1;
    if (!read_at(mode, 0x30c, saves) || saves != 0)
        return fail(error, "snapshot rejected: saves in progress=" + std::to_string(saves) + " required=0");
    LOG_INFO("[Native] Snapshot task dispatch attempted path={}", filename);
    const std::string stem = filename.substr(0, filename.size() - 4);
    const DWORD exception = call_string(save_absolute, mode, stem.c_str());
    if (exception) return fail(error, "snapshot dispatch failed: native exception=" + std::to_string(exception));
    LOG_INFO("[Native] Snapshot task dispatched path={}; persistence awaits file acknowledgement", filename);
    return true;
}

bool read_saves_in_progress(int32_t& count, std::string& error) {
    if (!check_thread(error)) return false;
    const auto* mode = singleton(mode_slot);
    void* active = nullptr;
    count = -1;
    if (!read_at(singleton(game_slot), 0x28, active) || active != mode || !has_vtable(mode, 0x16e6720))
        return fail(error, "save status rejected: in-game mode is not active");
    if (!read_at(mode, 0x30c, count) || count < 0)
        return fail(error, "save status rejected: native counter absent, unreadable or negative");
    return true;
}

bool request_load(const std::string& filename, std::string& error) {
    if (!check_thread(error)) return false;
    if (software_replica.armed && (software_replica.bound || !software_replica.error.empty()))
        return fail(error, "load rejected: software replica requires a new title-mode epoch before replacing its world");
    const std::filesystem::path path(filename);
    const auto folder = path.parent_path().filename().string();
    const auto stem = path.stem().string();
    if (path.is_absolute() || path.extension() != ".vrz" ||
        path.parent_path().parent_path().generic_string() != "save" ||
        folder.empty() || folder == "." || folder == ".." || stem.empty() || stem == "." || stem == ".." ||
        filename.find_first_of("\"\r\n\t") != std::string::npos || filename.find('\0') != std::string::npos)
        return fail(error, "load rejected: required virtual path save/<game-folder>/<filename>.vrz");
    auto* game = singleton(game_slot);
    void* active_mode = nullptr;
    void* title = nullptr;
    void* load_window = nullptr;
    void* game_selector = nullptr;
    unsigned char pending = 1;
    if (!game || !read_at(game, 0x28, active_mode) || !read_at(game, 0x350, title) || !title || active_mode != title)
        return fail(error, "load rejected: title mode must be active before world replacement");
    if (!read_at(title, 0x88, load_window) || !readable(load_window, 0x560) ||
        !read_at(title, 0xd8, game_selector) || !readable(game_selector, 0x38) ||
        !read_at(title, 0xd0, pending) || pending)
        return fail(error, "load rejected: title widgets unavailable or a load is already pending");
    const std::string command = "Load \"" + stem + "\" \"" + folder + "\"";
    LOG_INFO("[Native] Title load request attempted virtual_path={}", filename);
    const DWORD exception = call_string(title_command, title, command.c_str());
    if (exception) return fail(error, "title load native call failed: exception=" + std::to_string(exception));
    if (!read_at(title, 0xd0, pending) || !pending ||
        !game_string_equals(game, 0x450, stem) || !game_string_equals(game, 0x470, folder))
        return fail(error, "title load acknowledgement failed: pending flag or native filename differs");
    LOG_INFO("[Native] Title load request acknowledged virtual_path={}; world activation pending", filename);
    return true;
}

bool dispatch_command(const std::string&, std::string& error) {
    return fail(error, "action replay unavailable: ModeInGame Command accepts menu commands, not gameplay edits");
}

bool request_return_to_title(std::string& error) {
    if (!check_thread(error)) return false;
    if (!in_game()) return fail(error, "return to title rejected: no active world");
    int32_t saves = -1;
    if (!read_saves_in_progress(saves, error)) return false;
    if (saves != 0) return fail(error, "return to title rejected: saves in progress=" + std::to_string(saves) + " required=0");
    auto* mode = singleton(mode_slot);
    LOG_INFO("[Native] Return to title attempted; native autosave enabled");
    const DWORD exception = call_string(0x4a2f90, mode, "Quit");
    if (exception) return fail(error, "return to title native call failed: exception=" + std::to_string(exception));
    unsigned char quitting = 0;
    unsigned char desktop_exit = 1;
    if (!read_at(mode, 0x304, quitting) || !quitting || !read_at(mode, 0x305, desktop_exit) || desktop_exit)
        return fail(error, "return to title acknowledgement failed: quit flags differ");
    LOG_INFO("[Native] Return to title acknowledged; autosave and title activation pending");
    return true;
}

bool request_exit_game(std::string& error) {
    if (!check_thread(error)) return false;
    WorldStatus state;
    if (!read_world_status(state, error)) return false;
    if (state.active_mode_index != 2 || state.title_transition_pending || state.saves_in_progress > 0)
        return fail(error, "exit rejected: title mode with no pending transition/save required; active=" +
            std::to_string(state.active_mode_index) + " pending=" + std::to_string(state.title_transition_pending) +
            " saves=" + std::to_string(state.saves_in_progress));
    LOG_INFO("[Native] Exit request attempted from idle title");
    const DWORD exception = call_void(0x9563b0, nullptr);
    if (exception) return fail(error, "exit native call failed: exception=" + std::to_string(exception));
    unsigned char requested = 0;
    if (!read_at(reinterpret_cast<void*>(image.load()), 0x1c4c160, requested) || !requested)
        return fail(error, "exit acknowledgement failed: native exit-request flag is false");
    LOG_INFO("[Native] Exit flag acknowledged; process termination pending");
    return true;
}

bool show_new_game_window(void* expected_title, void* expected_window, bool& attempted, std::string& error) {
    attempted = false;
    if (!check_thread(error)) return false;
    const auto* game = singleton(game_slot);
    void* active = nullptr;
    void* title = nullptr;
    void* window = nullptr;
    unsigned char pending = 1, visible = 1;
    int32_t requested = -1;
    if (!has_vtable(game, 0x1743730) || !read_at(game, 0x28, active) ||
        !read_at(game, 0x350, title) || title != expected_title || active != title ||
        !has_vtable(title, 0x16dfba0) || !read_at(game, 0x434, requested) || requested != 2)
        return fail(error, "New Game launch rejected: idle current Title owner required");
    if (!read_at(title, 0xd0, pending) || pending || !read_at(title, 0x20, window) ||
        window != expected_window || !has_vtable(window, 0x1707fa0) ||
        !read_at(window, 0x45f, visible) || visible)
        return fail(error, "New Game launch rejected: hidden owned wizard required, with no pending transition");
    attempted = true;
    const auto exception = call_void(0x4b8770, title);
    if (exception) return fail(error, "New Game ShowCreateWindow failed: native exception=" + std::to_string(exception));
    error.clear();
    return true;
}

bool prepare_test_world(const std::string& name, std::string& error) {
    void* title = nullptr;
    void* window = nullptr;
    if (!test_world_objects(title, window, error)) return false;
    if (!valid_test_name(name)) return fail(error, "test world name rejected: MPTest prefix and ASCII letters/digits/_/- required");
    LOG_INFO("[Native] Test world preparation attempted name={}", name);
    DWORD exception = call_void(0x4b8770, title);
    if (exception) return fail(error, "test world ShowCreateWindow failed: exception=" + std::to_string(exception));
    const std::string command = "MessageTo WindowName \"NewGame\" SetCreateGameName \"" + name + "\"";
    exception = call_string(pane_command, window, command.c_str());
    if (exception) return fail(error, "test world SetCreateGameName failed: exception=" + std::to_string(exception));
    if (!game_string_equals(window, 0x570, name)) return fail(error, "test world name readback differs from requested name");
    int32_t types = 0;
    void* type = nullptr;
    if (!read_at(window, 0x508, types) || types <= 0)
        return fail(error, "test world preparation rejected: native game-type list is empty");
    exception = first_game_type(window, type);
    if (exception) return fail(error, "test world game-type lookup failed: exception=" + std::to_string(exception));
    std::string type_name;
    if (!read_game_string(type, 0x18, type_name) || type_name.empty() ||
        type_name.find_first_of("\"\\\r\n\t") != std::string::npos || type_name.find('\0') != std::string::npos)
        return fail(error, "test world preparation rejected: native game-type identifier is absent or invalid");
    const std::string select_type = "MessageTo WindowName \"NewGame\" SetCreateGameType \"" + type_name + "\"";
    exception = call_string(pane_command, window, select_type.c_str());
    if (exception) return fail(error, "test world SetCreateGameType failed: exception=" + std::to_string(exception));
    if (!game_string_equals(window, 0x5b0, type_name))
        return fail(error, "test world game-type readback differs from native selection");
    LOG_INFO("[Native] Test world type selection acknowledged={}", type_name);
    LOG_INFO("[Native] Test world model name acknowledged={}; map generation pending", name);
    return true;
}

bool test_world_ready(bool& complete, std::string& error) {
    complete = false;
    void* title = nullptr;
    void* window = nullptr;
    if (!test_world_objects(title, window, error)) return false;
    void* task = nullptr;
    void* layout = nullptr;
    unsigned char dirty = 1;
    int32_t regions = 0;
    if (!read_at(window, 0x688, task) || !read_at(window, 0x690, dirty) ||
        !read_at(window, 0x678, layout)) return fail(error, "test world readiness fields are unreadable");
    if (layout && !read_at(layout, 0x80, regions)) return fail(error, "test world region count is unreadable");
    complete = layout && !task && !dirty && regions > 0;
    return true;
}

bool create_test_world(const std::string& name, std::string& error) {
    void* title = nullptr;
    void* window = nullptr;
    if (!test_world_objects(title, window, error)) return false;
    if (!valid_test_name(name) || !game_string_equals(window, 0x570, name))
        return fail(error, "test world start rejected: requested diagnostic name does not match prepared native model");
    std::string type_name;
    if (!read_game_string(window, 0x5b0, type_name) || type_name.empty())
        return fail(error, "test world start rejected: native game type is not selected");
    bool complete = false;
    if (!test_world_ready(complete, error)) return false;
    if (!complete) return fail(error, "test world start rejected: map layout is not ready; required task=null dirty=0 regions>0");
    LOG_INFO("[Native] Test world creation attempted name={}", name);
    const DWORD exception = call_string(pane_command, window, "MessageTo WindowName \"NewGame\" CreateGame");
    if (exception) return fail(error, "test world CreateGame failed: exception=" + std::to_string(exception));
    unsigned char pending = 0;
    const auto* game = singleton(game_slot);
    if (!read_at(title, 0xd0, pending) || !pending || !game_string_equals(game, 0x470, name))
        return fail(error, "test world creation not acknowledged: native validation rejected it or transition was not queued");
    LOG_INFO("[Native] Test world creation acknowledged name={}; world activation pending", name);
    return true;
}

namespace {
bool exact_entry(uintptr_t rva,const unsigned char* expected,size_t count,std::string& error) {
    const void* address=reinterpret_cast<void*>(image.load()+rva);
    if(!readable(address,count)||std::memcmp(address,expected,count))return fail(error,"native entry guard differs RVA="+std::to_string(rva));
    return true;
}
bool editable(std::string& error) {
    int32_t edits=-1,saves=-1;unsigned char locked=1;
    if(!check_thread(error)||!in_game())return fail(error,"Object action requires an active world on the game thread");
    if(!read_at(reinterpret_cast<void*>(image.load()),0x13beda0,edits)||edits!=0||
       !read_at(reinterpret_cast<void*>(image.load()),0x1a30742,locked)||locked||!read_saves_in_progress(saves,error)||saves!=0)
        return fail(error,"Object action requires idle edit/save state; edits="+std::to_string(edits)+" saves="+std::to_string(saves)+" lock="+std::to_string(locked));
    return true;
}
struct MoveReceipt {bool loc=false,begun=false,opened=false,constructed=false,attempted=false,returned=false;void* graph=nullptr;DWORD exception=0,cleanup=0;};
void move_once(uintptr_t base,void* target,const unsigned char* pose,MoveReceipt* receipt) {
    alignas(16) unsigned char loc[0x38]{},action[0x130]{};
    __try {
        reinterpret_cast<void(*)(void*,const char*)>(base+0xa16f10)(loc,"Move");receipt->loc=true;
        reinterpret_cast<void(*)(const void*)>(base+0x265300)(loc);receipt->begun=true;
        receipt->graph=reinterpret_cast<void*(*)(const void*)>(base+0x539990)(loc);receipt->opened=true;
        if(receipt->graph){reinterpret_cast<void(*)(void*,void*,const void*)>(base+0x55e8e0)(action,target,pose);receipt->constructed=true;
            receipt->attempted=true;reinterpret_cast<void(*)(void*,void*)>(base+0x55ed80)(action,receipt->graph);receipt->returned=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){receipt->exception=GetExceptionCode();}
    if(receipt->constructed){__try{reinterpret_cast<void(*)(void*)>(base+0xcf1600)(action);}__except(EXCEPTION_EXECUTE_HANDLER){receipt->cleanup=GetExceptionCode();}}
    if(receipt->opened&&receipt->graph){__try{reinterpret_cast<void(*)()>(base+0x5379c0)();}__except(EXCEPTION_EXECUTE_HANDLER){receipt->cleanup=GetExceptionCode();}}
    if(receipt->begun){__try{reinterpret_cast<void(*)()>(base+0x265650)();}__except(EXCEPTION_EXECUTE_HANDLER){receipt->cleanup=GetExceptionCode();}}
    if(receipt->loc){
        __try{reinterpret_cast<void(*)(void*)>(base+0x11b2b00)(loc+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){receipt->cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(base+0x11bd1e0)(loc);}__except(EXCEPTION_EXECUTE_HANDLER){receipt->cleanup=GetExceptionCode();}
    }
}
struct NativePropArray {uintptr_t addresspoint;void** data;int32_t count,capacity;};
DWORD remove_once(uintptr_t base,void* manager,void* target,bool* attempted) {
    NativePropArray array{base+0x1748c50,&target,1,1};
    __try{*attempted=true;reinterpret_cast<void(*)(void*,NativePropArray*)>(base+0x2d42c0)(manager,&array);}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
DWORD model_once(uintptr_t base,const char* name,void** result) {
    GameString text{};bool constructed=false;DWORD exception=0;
    __try{reinterpret_cast<ConstructString>(base+string_construct)(&text,name);constructed=true;*result=reinterpret_cast<void*(*)(const void*)>(base+0x9a02e0)(&text);}
    __except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}
    if(constructed){__try{reinterpret_cast<DisposeString>(base+string_dispose)(&text);}__except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}}
    return exception;
}
DWORD select_once(uintptr_t base,void* manager,void* target) {
    __try {
        reinterpret_cast<void(*)(void*,void*)>(base+0x2d5c60)(manager,target);
        reinterpret_cast<void(*)(void*)>(base+0x2d28b0)(manager);
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    return 0;
}
}
bool selected_prop(PropState& result,bool& selected,std::string& error) {
    selected=false;if(!check_thread(error)||!in_game())return fail(error,"Selection requires an active world");
    auto manager=singleton(0x13df630);void* data=nullptr;int32_t count=0,capacity=0;uintptr_t table=0;
    uintptr_t array_table=0;
    // The active selection is the native array at +0x20; +0x08 is the hover candidate array.
    if(!read_at(manager,0,table)||table!=image.load()+0x17205d0||!read_at(manager,0x20,array_table)||array_table!=image.load()+0x1748c50||!read_at(manager,0x28,data)||!read_at(manager,0x30,count)||!read_at(manager,0x34,capacity)||count<0||capacity<count||count>4096)
        return fail(error,"Selection owner/array differs");
    if(!count)return true;
    void* object=nullptr;if(!read_at(data,0,object)||!prop_state(object,result))return fail(error,"First selected object is not a supported world prop");
    selected=true;return true;
}
bool diagnostic_select_prop(uint64_t uid,std::string& error) {
    if(!check_thread(error)||!in_game())return fail(error,"Selection requires an active world");
    constexpr unsigned char select_guard[]={0x57,0x56,0x53,0x48,0x83,0xec,0x40,0x48,0x89,0xcf,0x48,0x89,0xd6,0x48,0x85,0xd2};
    constexpr unsigned char info_guard[]={0x53,0x48,0x83,0xec,0x20,0x8b,0x41,0x30,0x48,0x89,0xcb,0x85,0xc0,0x75,0x51};
    if(!exact_entry(0x2d5c60,select_guard,sizeof(select_guard),error)||!exact_entry(0x2d28b0,info_guard,sizeof(info_guard),error))return false;
    void* target=nullptr;PropState before;auto exception=find_prop(uid,target);auto manager=singleton(0x13df630);uintptr_t table=0;
    if(exception||!prop_state(target,before)||before.uid!=uid||!read_at(manager,0,table)||table!=image.load()+0x17205d0)return fail(error,"Selection target or native manager identity differs");
    exception=select_once(image.load(),manager,target);
    if(exception)return fail(error,"Selection/show original raised exception="+std::to_string(exception));
    PropState after;bool selected=false;
    if(!selected_prop(after,selected,error))return false;
    return selected&&after.uid==uid?true:fail(error,"Selection original returned but selected identity differs");
}
bool place_authored_building(const std::string& type,const std::string& variant,float x,float z,BuildingState& result,bool& attempted,std::string& error) {
    attempted=false;if(!editable(error))return false;
    if(type.empty()||variant.empty()||type.size()>256||variant.size()>256||type.find('\0')!=std::string::npos||variant.find('\0')!=std::string::npos||!std::isfinite(x)||!std::isfinite(z))return fail(error,"Invalid placement definition or coordinates");
    auto receipt=place_building_native(type.c_str(),variant.c_str(),x,z,true,true,0);
    attempted=receipt.stage>=8;
    if(receipt.exception||receipt.stage!=10||!receipt.building){
        const char* reason=receipt.stage==6?"Building footprint overlaps another building":receipt.stage==7?"Building footprint overlaps a path":receipt.stage==2?"Building type is locked":receipt.stage==14?"Region is not owned":receipt.stage==8?"Purchase was refused":"Native placement rejected or incomplete";
        return fail(error,std::string(reason)+"; stage="+std::to_string(receipt.stage)+" exception="+std::to_string(receipt.exception));}
    uint64_t uid=0;if(!read_at(receipt.building,0x18,uid)||!uid)return fail(error,"Placed building UID readback failed");
    if(!read_building(uid,result,error))return false;
    if(result.type!=type||result.variant!=variant||result.prop.x!=x||result.prop.z!=z)return fail(error,"Placed building identity/pose readback differs");
    return true;
}
bool move_authored_prop(uint64_t uid,float x,float y,float z,bool& attempted,std::string& error) {
    attempted=false;if(!editable(error))return false;
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return fail(error,"Move target must be finite");
    void* target=nullptr;auto exception=find_prop(uid,target);PropState before;
    if(exception||!prop_state(target,before)||before.uid!=uid||!readable(static_cast<unsigned char*>(target)+0x100,0x6c))return fail(error,"Move target no longer exists or its native pose is unreadable");
    alignas(16) unsigned char pose[0x6c]{};std::memcpy(pose,static_cast<unsigned char*>(target)+0x100,sizeof(pose));
    std::memcpy(pose+0x10,&x,4);std::memcpy(pose+0x14,&y,4);std::memcpy(pose+0x18,&z,4);pose[0x28]=1;
    MoveReceipt receipt;move_once(image.load(),target,pose,&receipt);attempted=receipt.attempted;
    if(receipt.exception||receipt.cleanup||!receipt.returned)return fail(error,"Move original incomplete; attempted="+std::to_string(attempted)+" exception="+std::to_string(receipt.exception)+" cleanup="+std::to_string(receipt.cleanup));
    PropState after;if(!read_prop(uid,after,error))return false;
    if(after.x!=x||after.y!=y||after.z!=z)return fail(error,"Move original returned but pose readback differs");
    return true;
}
bool remove_authored_prop(uint64_t uid,bool& attempted,std::string& error) {
    attempted=false;if(!editable(error))return false;
    constexpr unsigned char guard[]={0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xa8};
    // Registration verifies the original prolog before the framework installs its own observer detour.
    if(!object_hook_entries_verified&&!exact_entry(0x2d42c0,guard,sizeof(guard),error))return false;
    void* target=nullptr;PropState before;auto exception=find_prop(uid,target);auto manager=singleton(0x13df630);uintptr_t table=0;
    if(exception||!prop_state(target,before)||before.uid!=uid||!read_at(manager,0,table)||table!=image.load()+0x17205d0)return fail(error,"Remove target or native manager identity differs");
    exception=remove_once(image.load(),manager,target,&attempted);
    if(exception)return fail(error,"Remove original raised exception="+std::to_string(exception));
    void* remaining=nullptr;exception=find_prop(uid,remaining);
    if(exception||remaining)return fail(error,"Remove original returned but object registry still contains target");
    return true;
}
bool load_authored_model(const std::string& name,std::string& error) {
    if(!check_thread(error))return false;
    if(name.empty()||name.find("..")!=std::string::npos||name.find_first_of("\\:")!=std::string::npos)return fail(error,"Model path must be a confined virtual resource");
    constexpr unsigned char guard[]={0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0x88};
    if(!exact_entry(0x9a02e0,guard,sizeof(guard),error))return false;
    void* result=nullptr;auto exception=model_once(image.load(),name.c_str(),&result);
    if(exception||!readable(result,sizeof(void*)))return fail(error,"Native model load failed; exception="+std::to_string(exception));
    // Static content shares the engine process lifetime. It is never reloaded in place.
    static std::vector<void*> retained_models;retained_models.push_back(result);
    return true;
}
bool saved_location(uint64_t uid,nlohmann::json& location,std::string& error) {
    BuildingState state;if(!read_building(uid,state,error))return false;
    if(state.dungeon_membership)return fail(error,"Persistent building references require the root world");
    void* object=nullptr;if(find_prop(uid,object)||!object)return fail(error,"Persistent reference target is missing");
    void* region=nullptr;void* manager=nullptr;void* data=nullptr;int32_t region_id=-1,count=0,capacity=0;
    if(!read_at(object,0x178,region)||!read_at(region,0x834,region_id)||region_id<0||!read_at(region,0x950,manager)||
       !read_at(manager,0x410,data)||!read_at(manager,0x418,count)||!read_at(manager,0x41c,capacity)||count<0||count>4096||capacity<count)
        return fail(error,"Saved region/building array bounds differ");
    int child=-1;for(int i=0;i<count;++i){void* candidate=nullptr;if(!read_at(data,size_t(i)*8,candidate))return fail(error,"Saved building child is unreadable");if(candidate==object){if(child!=-1)return fail(error,"Saved building occurs twice");child=i;}}
    if(child<0)return fail(error,"Object is absent from its saved building array");
    location={{"region",region_id},{"child",child},{"type",state.type},{"variant",state.variant},{"x",state.prop.x},{"y",state.prop.y},{"z",state.prop.z}};return true;
}
bool resolve_location(const nlohmann::json& location,PropState& prop,std::string& error) {
    if(!check_thread(error)||!in_game())return fail(error,"Persistent reference requires an active world");
    auto map=singleton(0x13df8c0);void* data=nullptr;int32_t count=0,capacity=0;
    if(!has_vtable(map,0x1742850)||!read_at(map,0x238,data)||!read_at(map,0x240,count)||!read_at(map,0x244,capacity)||count<0||count>4096||capacity<count)
        return fail(error,"Saved root region array differs");
    int wanted=location.at("region"),child=location.at("child");void* region=nullptr;
    for(int i=0;i<count;++i){void* row=nullptr;int32_t id=-1;if(!read_at(data,size_t(i)*8,row)||!read_at(row,0x834,id))return fail(error,"Saved region identity is unreadable");if(id==wanted){if(region)return fail(error,"Saved region identity is duplicated");region=row;}}
    void* manager=nullptr;void* buildings=nullptr;void* object=nullptr;uint64_t uid=0;
    if(!region||child<0||!read_at(region,0x950,manager)||!read_at(manager,0x410,buildings)||!read_at(manager,0x418,count)||!read_at(manager,0x41c,capacity)||count<0||count>4096||capacity<count||child>=count||!read_at(buildings,size_t(child)*8,object)||!read_at(object,0x18,uid))
        return fail(error,"Saved building reference is missing or outside array bounds");
    nlohmann::json actual;if(!saved_location(uid,actual,error))return false;
    if(actual!=location)return fail(error,"Saved building identity differs; reference was not rebound");
    return read_prop(uid,prop,error);
}
namespace {
using CreateBuilding=void*(*)(void*,const void*,const void*,const void*,void*,void*,void*,bool);
using MoveBuilding=void(*)(void*,void*);
using RemoveBuildings=void(*)(void*,void*);
CreateBuilding original_create_building=nullptr;
MoveBuilding original_move_building=nullptr;
RemoveBuildings original_remove_buildings=nullptr;
std::vector<ObjectChange> observed_changes;
uint64_t dropped_changes=0;
bool observing(){return game_thread.load()==GetCurrentThreadId()&&in_game();}
void observe(const char* operation,const PropState& prop,bool removed=false) {
    if(observed_changes.size()>=4096){if(!dropped_changes)LOG_WARN("[Framework] Native object event budget exceeded (4096); events are being counted as dropped");++dropped_changes;return;}
    observed_changes.push_back({operation,prop,removed});
}
void* create_observed(void* manager,const void* type,const void* variant,const void* pose,void* a,void* b,void* c,bool flag) {
    auto result=original_create_building(manager,type,variant,pose,a,b,c,flag);
    try{if(observing()&&result){PropState prop;if(prop_state(result,prop))observe("actions.place",prop);else LOG_WARN("[Framework] Created building event omitted: native prop identity is unreadable");}}
    catch(const std::exception& error){LOG_ERROR("[Framework] Created building observer failed: {}",error.what());}
    return result;
}
void* create_building(void* manager,const void* type,const void* variant,const void* pose,void* a,void* b,void* c,bool flag) {
    MT2FW_MT2_CreateCall call{sizeof(call),flag?1u:0u,0,GetCurrentThreadId(),manager,type,variant,pose,a,b,c};
    return mt2fw::mt2::dispatch_create(call,create_observed);
}
void move_building(void* action,void* graph) {
    uint64_t uid=0;PropState before;std::string error;bool watched=false;
    try{if(observing()){
        if(!read_at(action,0xb8,uid))LOG_WARN("[Framework] Move event target UID unreadable");
        else if(!read_prop(uid,before,error))LOG_WARN("[Framework] Move event target read failed: {}",error);
        else watched=true;
    }}
    catch(const std::exception& problem){LOG_ERROR("[Framework] Move event pre-read failed: {}",problem.what());}
    original_move_building(action,graph);
    try{if(watched){PropState after;if(!read_prop(uid,after,error))LOG_WARN("[Framework] Move event readback failed: {}",error);
        else if(before.x!=after.x||before.y!=after.y||before.z!=after.z)observe("actions.move",after);}}
    catch(const std::exception& problem){LOG_ERROR("[Framework] Move event observer failed: {}",problem.what());}
}
void remove_observed(void* manager,void* array) {
    std::vector<PropState> before;
    try{if(observing()){
        void* data=nullptr;int32_t count=0,capacity=0;
        if(!read_at(array,8,data)||!read_at(array,0x10,count)||!read_at(array,0x14,capacity)||count<0||count>4096||capacity<count)LOG_WARN("[Framework] Remove event omitted: native array bounds differ");
        else for(int32_t i=0;i<count;++i){void* target=nullptr;PropState row;if(read_at(data,size_t(i)*8,target)&&prop_state(target,row))before.push_back(row);else LOG_WARN("[Framework] Remove event target unreadable at index={}",i);}
    }}catch(const std::exception& problem){LOG_ERROR("[Framework] Remove event pre-read failed: {}",problem.what());}
    original_remove_buildings(manager,array);
    try{for(auto& row:before){void* remaining=nullptr;auto exception=find_prop(row.uid,remaining);
        if(exception)LOG_WARN("[Framework] Remove event registry read failed: exception={}",exception);
        else if(!remaining)observe("actions.remove",row,true);}}
    catch(const std::exception& problem){LOG_ERROR("[Framework] Remove event observer failed: {}",problem.what());}
}
__declspec(noinline) void remove_buildings(void* manager,void* array) {
    auto slot=reinterpret_cast<uintptr_t>(_AddressOfReturnAddress());
    auto address=reinterpret_cast<uintptr_t>(_ReturnAddress());
    MT2FW_MT2_RemoveCall call{sizeof(call),0,0,GetCurrentThreadId(),manager,array,slot,address};
    mt2fw::mt2::dispatch_remove(call,remove_observed);
}
}
std::vector<ObservationHook> observation_hooks() {
    constexpr unsigned char create_guard[]={0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0x58,0x01,0,0};
    constexpr unsigned char move_guard[]={0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x48,0x48,0x89,0xcf,0x48,0x8b,0x89,0xb8,0,0,0};
    constexpr unsigned char remove_guard[]={0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xa8};
    std::string error;
    if(!exact_entry(0x24ed30,create_guard,sizeof(create_guard),error)||!exact_entry(0x55ed80,move_guard,sizeof(move_guard),error)||!exact_entry(0x2d42c0,remove_guard,sizeof(remove_guard),error))throw std::runtime_error(error);
    object_hook_entries_verified=true;
    return {{reinterpret_cast<void*>(image.load()+0x24ed30),reinterpret_cast<void*>(create_building),reinterpret_cast<void**>(&original_create_building)},
            {reinterpret_cast<void*>(image.load()+0x55ed80),reinterpret_cast<void*>(move_building),reinterpret_cast<void**>(&original_move_building)},
            {reinterpret_cast<void*>(image.load()+0x2d42c0),reinterpret_cast<void*>(remove_buildings),reinterpret_cast<void**>(&original_remove_buildings)}};
}
std::vector<ObjectChange> take_object_changes(){auto result=std::move(observed_changes);observed_changes.clear();if(dropped_changes){LOG_WARN("[Framework] Native object event queue recovered; dropped={}",dropped_changes);dropped_changes=0;}return result;}
} // namespace mt2mp::game::native

#include "global_root.inc"
