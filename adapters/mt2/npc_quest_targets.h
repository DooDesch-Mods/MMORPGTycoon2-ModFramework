#pragma once
#include "region_saved_identity.h"
#include "snapshot_zone_key.h"
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mt2mp::game::npc_quest_targets {
// These observers wrap the engine's existing snapshot calls. They never call
// Save, Load, Register or a game mutation themselves.
// The enumerated property inlines the array routines in normal world saves.
inline constexpr uintptr_t save_property_rva = 0xe3c6e0;
inline constexpr uintptr_t load_property_rva = 0xe3c0f0;
inline constexpr uintptr_t load_children_return_rva = 0xe3c1b3;
inline constexpr uintptr_t load_link_return_rva = 0xe3c56a;
inline constexpr uintptr_t load_inline_return_rva = 0xe3c638;
using PropertyCall = bool(*)(const void*, void*, void*, void*);

struct Budget { size_t zones = 0, sections = 0, vertices = 0, buildings = 0; };
struct Origin {
    std::string uuid, sha256;
    uint64_t world_epoch = 0, world_generation = 0;
    bool operator==(const Origin&) const = default;
};
struct World {
    Origin origin;
    uintptr_t image = 0, map = 0, root = 0, region = 0, selection = 0;
    int32_t saved_region = -1;
};
struct Content {
    int32_t type = -1, density = 0, level = 0, monster_type = 0, constructed_day = 0;
    std::vector<std::vector<std::array<uint32_t, 3>>> sections;
    bool operator==(const Content&) const = default;
};
struct Zone {
    uintptr_t pointer = 0;
    uint64_t uid = 0;
    int32_t link_id = 0;
    size_t saved_child = 0;
    Content content;
    bool operator==(const Zone&) const = default;
};
struct Array {
    uintptr_t pointer = 0;
    std::vector<Zone> zones;
    bool operator==(const Array&) const = default;
};
struct SavedPool {
    Origin origin;
    int32_t saved_region = -1;
    Array array;
    std::map<uintptr_t,std::string> action_keys;
};
struct Pools { std::vector<std::string> buildings, monster_zones; };

inline bool fail(std::string& error, const std::string& cause) { error = cause; return false; }
inline bool valid_origin(const Origin& value) {
    if (value.uuid.size() != 36 || value.sha256.size() != 64 || !value.world_epoch || !value.world_generation) return false;
    const auto hex = [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); };
    for (size_t i = 0; i < value.uuid.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (value.uuid[i] != '-') return false; }
        else if (!hex(value.uuid[i])) return false;
    }
    for (const auto c : value.sha256) if (!hex(c)) return false;
    return true;
}
inline std::string zone_key(const std::string& sha, int32_t region, const Zone& zone) {
    const auto suffix = zone.link_id > 0 ? std::to_string(zone.link_id) :
        snapshot_zone_key::root_suffix(region, static_cast<int32_t>(zone.saved_child));
    return sha + ":world:zone:" + suffix;
}
inline std::string zone_key(const SavedPool& saved,const Zone& zone) {
    const auto action=saved.action_keys.find(zone.pointer);
    return action==saved.action_keys.end()?zone_key(saved.origin.sha256,saved.saved_region,zone):action->second;
}
template<class Memory, class T>
bool at(Memory& memory, uintptr_t object, size_t offset, T& out, std::string& error) {
    if (!object || object > UINTPTR_MAX - offset || object + offset > UINTPTR_MAX - sizeof(T))
        return fail(error, "quest target address null/overflow");
    return memory.read(object + offset, &out, sizeof out, error);
}
template<class Memory>
bool typed(Memory& memory, uintptr_t image, uintptr_t object, uintptr_t rva, std::string& error) {
    uintptr_t ap = 0;
    if (!at(memory, object, 0, ap, error)) return false;
    return ap == image + rva || fail(error, "quest target address point differs rva=" + std::to_string(rva));
}
struct Bounds { uintptr_t data = 0; int32_t count = 0, capacity = 0; bool operator==(const Bounds&) const = default; };
template<class Memory>
bool bounds(Memory& memory, uintptr_t array, size_t budget, Bounds& out, std::string& error) {
    if (!at(memory, array, 8, out.data, error) || !at(memory, array, 0x10, out.count, error) ||
        !at(memory, array, 0x14, out.capacity, error)) return false;
    if (out.count < 0 || out.capacity < out.count || static_cast<size_t>(out.count) > budget || (out.count && !out.data))
        return fail(error, "quest target array bounds/resource budget differ count=" + std::to_string(out.count));
    return true;
}
template<class Memory>
bool unchanged(Memory& memory, uintptr_t array, size_t budget, const Bounds& before, std::string& error) {
    Bounds after;
    return bounds(memory, array, budget, after, error) &&
        (before == after || fail(error, "quest target array changed during read"));
}
template<class Memory>
bool content(Memory& memory, uintptr_t image, uintptr_t object, const Budget& budget, Content& out, std::string& error, int32_t expected_type=1) {
    out = {};
    if (!typed(memory, image, object, 0x1745ce0, error) ||
        !at(memory, object, 0x24c, out.type, error) || !at(memory, object, 0x250, out.density, error) ||
        !at(memory, object, 0x360, out.level, error) || !at(memory, object, 0x560, out.monster_type, error) ||
        !at(memory, object, 0x56c, out.constructed_day, error)) return false;
    if (expected_type<0||expected_type>3||out.type!=expected_type) return fail(error, expected_type==1?"quest target is not an original Monster zone":"Zone content actual type differs from its current owner");
    const auto shape = object + 0x2f0;
    const auto array = shape + 0x40;
    Bounds sections;
    if (!typed(memory, image, shape, 0x16e0140, error) || !typed(memory, image, array, 0x16eb080, error) ||
        !bounds(memory, array, budget.sections, sections, error)) return false;
    size_t vertices_read = 0;
    std::set<uintptr_t> unique;
    for (int32_t n = 0; n < sections.count; ++n) {
        uintptr_t section = 0;
        if (!at(memory, sections.data, static_cast<size_t>(n) * 8, section, error) ||
            !typed(memory, image, section, 0x16fef20, error)) return false;
        if (!unique.insert(section).second) return fail(error, "quest target repeats a shape section");
        Bounds vertices;
        if (!typed(memory, image, section + 0x40, 0x1746160, error) ||
            !bounds(memory, section + 0x40, budget.vertices - vertices_read, vertices, error)) return false;
        std::vector<std::array<uint32_t, 3>> points(static_cast<size_t>(vertices.count));
        for (size_t v = 0; v < points.size(); ++v)
            if (!at(memory, vertices.data, v * sizeof(points[v]), points[v], error)) return false;
        if (!unchanged(memory, section + 0x40, budget.vertices - vertices_read, vertices, error)) return false;
        vertices_read += points.size(); out.sections.push_back(std::move(points));
    }
    return unchanged(memory, array, budget.sections, sections, error);
}
// Reads a serialization array even before Region::Init has assigned owners.
// The completed-world binder below supplies the missing owner proof.
template<class Memory, class LinkId>
bool observe_array(Memory& memory, uintptr_t image, uintptr_t array, const Budget& budget,
                   LinkId&& link_id, Array& out, std::string& error) {
    out = {}; out.pointer = array;
    Bounds initial;
    if (!typed(memory, image, array, 0x172e1e0, error) || !bounds(memory, array, budget.zones, initial, error)) return false;
    std::set<uintptr_t> pointers; std::set<uint64_t> uids; std::set<int32_t> ids;
    for (int32_t n = 0; n < initial.count; ++n) {
        Zone zone; zone.saved_child = static_cast<size_t>(n);
        if (!at(memory, initial.data, static_cast<size_t>(n) * 8, zone.pointer, error) ||
            !at(memory, zone.pointer, 0x18, zone.uid, error) ||
            !link_id(zone.pointer, zone.link_id, error) ||
            !content(memory, image, zone.pointer, budget, zone.content, error)) return false;
        if (!zone.uid || zone.link_id < 0 || !pointers.insert(zone.pointer).second || !uids.insert(zone.uid).second ||
            (zone.link_id && !ids.insert(zone.link_id).second)) return fail(error, "quest snapshot Zone pointer/UID/link duplicate or invalid");
        out.zones.push_back(std::move(zone));
    }
    if (!unchanged(memory, array, budget.zones, initial, error)) return false;
    error.clear(); return true;
}
template<class Memory>
bool owner(Memory& memory, const World& world, uintptr_t& building_manager, uintptr_t& zone_manager, std::string& error) {
    uintptr_t selected = 0;
    auto read = [&](uintptr_t object, size_t offset, auto& out, std::string& cause) { return at(memory, object, offset, out, cause); };
    if (!region_saved_identity::resolve(world.image, world.map, world.root, world.saved_region, world.region, selected, read, error) ||
        !at(memory, world.region, 0x950, building_manager, error) || !at(memory, world.region, 0x968, zone_manager, error) ||
        !typed(memory, world.image, building_manager, 0x17183c0, error) || !typed(memory, world.image, zone_manager, 0x16fe170, error)) return false;
    uintptr_t building_region = 0, building_root = 0, zone_region = 0;
    if (!at(memory, building_manager, 0x450, building_root, error) || !at(memory, building_manager, 0x458, building_region, error) ||
        !at(memory, zone_manager, 0xf0, zone_region, error)) return false;
    return (building_root == world.root && building_region == world.region && zone_region == world.region) ||
        fail(error, "quest target manager reciprocal owner differs");
}
template<class Memory>
bool validate_capture(Memory& memory, const World& world, const SavedPool& saved, const Budget& budget, std::string& error) {
    if (saved.saved_region != world.saved_region)
        return fail(error, "quest zone captured region binding differs");
    uintptr_t building = 0, manager = 0;
    if (!owner(memory, world, building, manager, error)) return false;
    if (saved.array.pointer != manager + 0x48) return fail(error, "quest saved Monster array differs from current owner");
    auto links = [&](uintptr_t pointer, int32_t& id, std::string& cause) {
        for (const auto& zone : saved.array.zones) if (zone.pointer == pointer) { id = zone.link_id; return true; }
        return fail(cause, "quest zone is absent from the bound snapshot array");
    };
    Array fresh;
    if (!observe_array(memory, world.image, saved.array.pointer, budget, links, fresh, error)) return false;
    // Array order is the original RNG pool order, not a sorted key order.
    if (fresh != saved.array) return fail(error, "quest zone saved membership/order/content changed");
    for (const auto& zone : fresh.zones) {
        uintptr_t root = 0, region = 0;
        if (!at(memory, zone.pointer, 0x170, root, error) || !at(memory, zone.pointer, 0x178, region, error)) return false;
        if (root != world.root || region != world.region) return fail(error, "quest zone current root/region owner differs");
    }
    error.clear(); return true;
}
template<class Memory>
bool validate_saved(Memory& memory, const World& world, const SavedPool& saved, const Budget& budget, std::string& error) {
    if (!valid_origin(world.origin)) return fail(error, "quest target current snapshot/world proof absent");
    if (saved.origin != world.origin)
        return fail(error, "quest zone snapshot SHA/UUID/world/region binding differs");
    return validate_capture(memory, world, saved, budget, error);
}
// BuildingKey must call existing remove_diagnostics::resolve_local_target with
// World.selection and compare its snapshot UUID, epoch and world generation.
template<class Memory, class BuildingKey>
bool read(Memory& memory, const World& world, const SavedPool& saved, const Budget& budget,
          BuildingKey&& building_key, Pools& out, std::string& error) {
    out = {};
    if (!validate_saved(memory, world, saved, budget, error)) return false;
    uintptr_t manager = 0;
    if (!at(memory, world.region, 0x950, manager, error) || !typed(memory, world.image, manager + 0x408, 0x172d7c0, error)) return false;
    Bounds initial;
    if (!bounds(memory, manager + 0x408, budget.buildings, initial, error)) return false;
    std::set<std::string> unique;
    for (int32_t n = 0; n < initial.count; ++n) {
        uintptr_t pointer = 0, root = 0, region = 0; std::string key;
        if (!at(memory, initial.data, static_cast<size_t>(n) * 8, pointer, error) ||
            !at(memory, pointer, 0x170, root, error) || !at(memory, pointer, 0x178, region, error) ||
            !building_key(pointer, key, error)) return false;
        if (root != world.root || region != world.region || key.empty() || !unique.insert(key).second)
            return fail(error, "quest Building pool owner/key differs or repeats");
        out.buildings.push_back(std::move(key));
    }
    if (!unchanged(memory, manager + 0x408, budget.buildings, initial, error)) return false;
    for (const auto& zone : saved.array.zones) out.monster_zones.push_back(zone_key(saved, zone));
    error.clear(); return true;
}
template<class Memory, class BuildingKey>
bool destination(Memory& memory, const World& world, const SavedPool& saved, const Budget& budget,
                 BuildingKey&& building_key, uintptr_t pointer, std::string& key, std::string& error) {
    key.clear();
    if (!pointer) return fail(error, "quest destination is null");
    uintptr_t ap = 0;
    if (!at(memory, pointer, 0, ap, error)) return false;
    if (ap == world.image + 0x1745ce0) {
        if (!validate_saved(memory, world, saved, budget, error)) return false;
        for (const auto& zone : saved.array.zones) if (zone.pointer == pointer) {
            key = zone_key(saved, zone); error.clear(); return true;
        }
        return fail(error, "quest destination Zone is not bound to the saved Monster pool");
    }
    return building_key(pointer, key, error) && (!key.empty() || fail(error, "quest Building destination key is empty"));
}

// Per-original-call state, owned by the existing snapshot operation. The outer
// wrapper validates NativeOpen reader/filename/context/thread before every event.
struct LoadFrame {
    uintptr_t array = 0, reader = 0, context = 0;
    int32_t declared = -1;
    bool counted = false;
    std::map<uintptr_t, int32_t> registrations;
    bool children(uintptr_t caller, uintptr_t actual_reader, const std::string& before_label,
                  int32_t before_declared, int32_t count, std::string& error) {
        if (caller != load_children_return_rva || actual_reader != reader)
            return fail(error, "quest Zone load BeginChildren receipt differs");
        if (before_label != "Monster") return true;
        if (counted || count != before_declared || count < 0) return fail(error, "quest Monster saved child count differs/repeats");
        declared = count; counted = true; return true;
    }
    bool registered(uintptr_t caller, uintptr_t actual_context, int32_t id, uintptr_t object, std::string& error) {
        const bool linked = caller == load_link_return_rva;
        if ((!linked && caller != load_inline_return_rva) || !counted || actual_context != context || !object ||
            (linked ? id <= 0 : id != 0) || registrations.size() >= static_cast<size_t>(declared) ||
            !registrations.emplace(object, id).second) return fail(error, "quest Zone load registration receipt differs");
        return true;
    }
    template<class Memory>
    bool finish(Memory& memory, uintptr_t image, const Budget& budget, Array& out, std::string& error) const {
        if (!counted || declared < 0 || registrations.size() != static_cast<size_t>(declared))
            return fail(error, "quest Zone load skipped a declared child or registration");
        auto links = [&](uintptr_t object, int32_t& id, std::string& cause) {
            const auto found = registrations.find(object);
            if (found == registrations.end()) return fail(cause, "quest loaded Zone has no original registration");
            id = found->second; return true;
        };
        if (!observe_array(memory, image, array, budget, links, out, error)) return false;
        return out.zones.size() == static_cast<size_t>(declared) || fail(error, "quest loaded Zone count differs from saved children");
    }
};
struct ChildHeader { std::string label; int32_t count = -1; };
// The owned NativeOpen reader guard runs before this fixed-header read. Copy
// the label before original BeginChildren; the reader reuses its current record.
template<class Memory>
bool child_header(Memory& memory, uintptr_t reader, ChildHeader& out, std::string& error) {
    out = {};
    uint8_t valid = 0, binary = 0; int32_t token = -1; uintptr_t data = 0; uint64_t size = 0;
    if (!at(memory, reader, 0x18, valid, error) || !at(memory, reader, 0x90, binary, error) ||
        !at(memory, reader, 0x8c, out.count, error) || !at(memory, reader, 0x20, token, error) ||
        !at(memory, reader, 0x28, data, error) || !at(memory, reader, 0x30, size, error)) return false;
    if (valid != 1 || binary != 1 || token != 1 || out.count < 0 || !data || size > 7 || data > UINTPTR_MAX - size)
        return fail(error, "quest Zone enum child header invalid");
    out.label.resize(static_cast<size_t>(size));
    if (size && !memory.read(data, out.label.data(), out.label.size(), error)) return false;
    if (out.label != "None" && out.label != "Monster" && out.label != "Puzzle" && out.label != "Story")
        return fail(error, "quest Zone enum child label differs");
    error.clear(); return true;
}
template<class Memory>
bool property(Memory& memory, uintptr_t image, uintptr_t object, uintptr_t manager, std::string& error) {
    uint64_t offset = 0; int32_t count = 0;
    if (!typed(memory, image, object, 0x1740cb0, error) || !typed(memory, image, manager, 0x16fe170, error) ||
        !at(memory, object, 0x38, offset, error) || !at(memory, object, 0x40, count, error)) return false;
    return (offset == 0x18 && count == 4) || fail(error, "quest Zone enumerated property layout differs");
}
struct SaveFrame {
    uintptr_t property_object = 0, manager = 0, writer = 0, context = 0;
    Array before;
};
// The outer snapshot operation owns the writer/file receipt, context and thread.
// LinkId uses the original SaveContext ContainsObject convention: absent -> 0.
template<class Memory, class LinkId>
bool begin_save(Memory& memory, uintptr_t image, uintptr_t property_object, uintptr_t manager,
                uintptr_t writer, uintptr_t context, const Budget& budget, LinkId&& links, SaveFrame& frame, std::string& error) {
    frame = {};
    if (!writer || !context) return fail(error, "quest Zone save writer/context absent");
    if (!property(memory, image, property_object, manager, error) ||
        !observe_array(memory, image, manager + 0x48, budget, links, frame.before, error)) return false;
    frame.property_object = property_object; frame.manager = manager; frame.writer = writer; frame.context = context;
    return true;
}
template<class Memory, class LinkId>
bool finish_save(Memory& memory, uintptr_t image, const SaveFrame& frame, uintptr_t property_object, uintptr_t manager,
                 uintptr_t writer, uintptr_t context, bool original_result, const Budget& budget, LinkId&& links,
                 Array& out, std::string& error) {
    out = {};
    if (!original_result || frame.property_object != property_object || frame.manager != manager ||
        frame.writer != writer || frame.context != context) return fail(error, "quest Zone original save result/receivers differ");
    if (!property(memory, image, property_object, manager, error) ||
        !observe_array(memory, image, manager + 0x48, budget, links, out, error)) return false;
    return out == frame.before || fail(error, "quest Zone content/order/link changed during original save");
}
template<class Memory>
bool begin_load(Memory& memory, uintptr_t image, uintptr_t property_object, uintptr_t manager,
                uintptr_t reader, uintptr_t context, const Budget& budget, LoadFrame& frame, std::string& error) {
    frame = {};
    if (!reader || !context) return fail(error, "quest Zone load reader/context absent");
    if (!property(memory, image, property_object, manager, error)) return false;
    Bounds initial;
    if (!typed(memory, image, manager + 0x48, 0x172e1e0, error) ||
        !bounds(memory, manager + 0x48, budget.zones, initial, error)) return false;
    if (initial.count) return fail(error, "quest native load requires initially empty Monster array");
    frame.array = manager + 0x48; frame.reader = reader; frame.context = context;
    error.clear(); return true;
}
template<class Memory>
bool registered(Memory& memory, uintptr_t image, LoadFrame& frame, uintptr_t caller,
                uintptr_t context, int32_t id, uintptr_t object, std::string& error) {
    int32_t type = -1; uintptr_t registered_context = 0;
    if (!typed(memory, image, object, 0x1745ce0, error) || !at(memory, object, 0x24c, type, error) ||
        !at(memory, object, 8, registered_context, error)) return false;
    if (registered_context != context) return fail(error, "quest loaded Zone object context differs from original registration");
    if (type != 1) return true;
    return frame.registered(caller, context, id, object, error);
}
}
