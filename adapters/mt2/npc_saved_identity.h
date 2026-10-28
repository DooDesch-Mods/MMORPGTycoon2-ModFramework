#pragma once
#include "npc_quest_targets.h"
#include "npc_census.h"
#include "snapshot_npc_key.h"

namespace mt2mp::game::npc_saved_identity {
namespace read = npc_quest_targets;
using read::at;
using read::typed;
using read::bounds;
using read::unchanged;
using read::fail;
using read::Bounds;
inline constexpr uintptr_t save_property_rva = 0xe3af60, load_property_rva = 0xe3aeb0;
inline constexpr uintptr_t load_children_return_rva = 0xc0f116;
inline constexpr uintptr_t load_link_return_rva = 0xc0f457, load_inline_return_rva = 0xc0f515;
inline constexpr size_t maximum_npcs = 100000;
struct Record {
    uintptr_t pointer = 0;
    uint64_t uid = 0;
    int32_t type = -1, child = -1, link_id = 0;
    bool operator==(const Record&) const = default;
};
struct Pool {
    uintptr_t manager = 0;
    std::vector<Record> records;
    bool operator==(const Pool&) const = default;
};
struct SavedPool { int32_t region = -1; Pool pool; };
struct Current {
    std::string key;
    int32_t saved_region = -1;
    npc_census::Row row;
    uintptr_t native_shard=0,native_region=0;
    std::string scene="world";
};
inline std::string key(const std::string& sha, int32_t region, const Record& record) {
    return sha + ":world:npc:" + (record.link_id > 0 ? std::to_string(record.link_id) :
        snapshot_npc_key::root_suffix(region, record.type, record.child));
}
template<class Memory>
bool property(Memory& memory, uintptr_t image, uintptr_t object, uintptr_t manager, std::string& error) {
    uint64_t offset = 0; int32_t count = 0;
    if (!typed(memory, image, object, 0x1740bd0, error) || !typed(memory, image, manager, 0x16e6aa0, error) ||
        !at(memory, object, 0x38, offset, error) || !at(memory, object, 0x40, count, error)) return false;
    return (object == image + 0x13bf5c0 && offset == 0x38 && count == 3) || fail(error, "NPC saved property layout differs");
}
template<class Memory, class LinkId>
bool observe(Memory& memory, uintptr_t image, uintptr_t manager, LinkId&& links, Pool& out, std::string& error) {
    Pool result; result.manager = manager;
    std::set<uintptr_t> pointers; std::set<uint64_t> uids; std::set<int32_t> ids;
    if (!typed(memory, image, manager, 0x16e6aa0, error)) return false;
    for (int32_t type = 0; type < 3; ++type) {
        const auto array = manager + 0x38 + static_cast<size_t>(type) * 0x30;
        Bounds initial;
        if (!typed(memory, image, array, 0x172de80, error) || !bounds(memory, array, maximum_npcs, initial, error)) return false;
        if (result.records.size() + static_cast<size_t>(initial.count) > maximum_npcs) return fail(error, "NPC snapshot count exceeds bound");
        for (int32_t n = 0; n < initial.count; ++n) {
            Record row; row.child = n;
            if (!at(memory, initial.data, static_cast<size_t>(n) * 8, row.pointer, error) ||
                !typed(memory, image, row.pointer, 0x1742980, error) ||
                !at(memory, row.pointer, 0x18, row.uid, error) || !at(memory, row.pointer, 0x80c, row.type, error) ||
                !links(row.pointer, row.link_id, error)) return false;
            if (!row.uid || row.type != type || row.link_id < 0 || !pointers.insert(row.pointer).second ||
                !uids.insert(row.uid).second || (row.link_id && !ids.insert(row.link_id).second))
                return fail(error, "NPC saved pointer, UID, type or link duplicate/invalid");
            result.records.push_back(row);
        }
        if (!unchanged(memory, array, maximum_npcs, initial, error)) return false;
    }
    out = std::move(result); error.clear(); return true;
}
struct SaveFrame {
    uintptr_t property_object = 0, manager = 0, writer = 0, context = 0;
    Pool before;
};
template<class Memory, class LinkId>
bool begin_save(Memory& memory, uintptr_t image, uintptr_t prop, uintptr_t manager,
                uintptr_t writer, uintptr_t context, LinkId&& links, SaveFrame& frame, std::string& error) {
    frame = {};
    if (!writer || !context) return fail(error, "NPC save writer/context absent");
    if (!property(memory, image, prop, manager, error) || !observe(memory, image, manager, links, frame.before, error)) return false;
    frame.property_object = prop; frame.manager = manager; frame.writer = writer; frame.context = context; return true;
}
template<class Memory, class LinkId>
bool finish_save(Memory& memory, uintptr_t image, const SaveFrame& frame, uintptr_t prop, uintptr_t manager,
                 uintptr_t writer, uintptr_t context, bool result, LinkId&& links, Pool& out, std::string& error) {
    if (!result || prop != frame.property_object || manager != frame.manager || writer != frame.writer || context != frame.context)
        return fail(error, "NPC original save result/receivers differ");
    if (!property(memory, image, prop, manager, error) || !observe(memory, image, manager, links, out, error)) return false;
    return out == frame.before || fail(error, "NPC snapshot order/identity/link changed during SaveStream");
}
struct ChildHeader { std::string label; int32_t count = -1; };
template<class Memory>
bool child_header(Memory& memory, uintptr_t reader, ChildHeader& out, std::string& error) {
    out = {}; uint8_t valid = 0, binary = 0; int32_t token = -1; uintptr_t data = 0; uint64_t size = 0;
    if (!at(memory, reader, 0x18, valid, error) || !at(memory, reader, 0x90, binary, error) ||
        !at(memory, reader, 0x8c, out.count, error) || !at(memory, reader, 0x20, token, error) ||
        !at(memory, reader, 0x28, data, error) || !at(memory, reader, 0x30, size, error)) return false;
    if (valid != 1 || binary != 1 || token != 1 || out.count < 0 || !data || size > 12 || data > UINTPTR_MAX - size)
        return fail(error, "NPC enum child header invalid");
    out.label.resize(static_cast<size_t>(size));
    if (size && !memory.read(data, out.label.data(), out.label.size(), error)) return false;
    for (const auto name : snapshot_npc_key::type_names) if (name == out.label) return true;
    return fail(error, "NPC enum child label differs");
}
struct LoadFrame {
    uintptr_t manager = 0, reader = 0, context = 0;
    std::array<int32_t, 3> declared{-1, -1, -1};
    std::array<std::vector<std::pair<uintptr_t, int32_t>>, 3> registrations;
    int32_t active_type = -1;
    bool children(uintptr_t caller, uintptr_t actual_reader, const std::string& label,
                  int32_t before_count, int32_t count, std::string& error) {
        if (caller != load_children_return_rva || actual_reader != reader || count != before_count || count < 0 ||
            static_cast<size_t>(count) > maximum_npcs) return fail(error, "NPC original child count/reader differs");
        int32_t type = -1;
        for (int32_t n = 0; n < 3; ++n) if (snapshot_npc_key::type_names[static_cast<size_t>(n)] == label) type = n;
        if (type < 0 || declared[static_cast<size_t>(type)] != -1 ||
            (active_type >= 0 && registrations[static_cast<size_t>(active_type)].size() != static_cast<size_t>(declared[static_cast<size_t>(active_type)])))
            return fail(error, "NPC type section repeated/unknown or prior children incomplete");
        active_type = type; declared[static_cast<size_t>(type)] = count; return true;
    }
    bool registered(uintptr_t caller, uintptr_t ctx, int32_t id, uintptr_t object, int32_t type, std::string& error) {
        const bool linked = caller == load_link_return_rva;
        if ((!linked && caller != load_inline_return_rva) || ctx != context || !object || type != active_type ||
            type < 0 || type >= 3 || (linked ? id <= 0 : id != 0)) return fail(error, "NPC original registration receiver/type/link differs");
        auto& rows = registrations[static_cast<size_t>(type)];
        if (rows.size() >= static_cast<size_t>(declared[static_cast<size_t>(type)])) return fail(error, "NPC registration exceeds declared children");
        for (const auto& bucket : registrations) for (const auto& old : bucket)
            if (old.first == object || (id && old.second == id)) return fail(error, "NPC original registration duplicate");
        rows.emplace_back(object, id); return true;
    }
    template<class Memory>
    bool finish(Memory& memory, uintptr_t image, Pool& out, std::string& error) const {
        for (size_t type = 0; type < 3; ++type)
            if (declared[type] < 0 || registrations[type].size() != static_cast<size_t>(declared[type]))
                return fail(error, "NPC original load skipped type/child/registration");
        auto links = [&](uintptr_t object, int32_t& id, std::string& cause) {
            for (const auto& bucket : registrations) for (const auto& entry : bucket)
                if (entry.first == object) { id = entry.second; return true; }
            return fail(cause, "NPC array member lacks original registration");
        };
        if (!observe(memory, image, manager, links, out, error)) return false;
        size_t cursor = 0;
        for (size_t type = 0; type < 3; ++type) for (const auto& entry : registrations[type]) {
            if (cursor >= out.records.size() || out.records[cursor].pointer != entry.first ||
                out.records[cursor].type != static_cast<int32_t>(type)) return fail(error, "NPC loaded array differs from original record order");
            ++cursor;
        }
        return cursor == out.records.size() || fail(error, "NPC loaded array has extra members");
    }
};
template<class Memory>
bool begin_load(Memory& memory, uintptr_t image, uintptr_t prop, uintptr_t manager,
                uintptr_t reader, uintptr_t context, LoadFrame& frame, std::string& error) {
    frame = {};
    if (!reader || !context || !property(memory, image, prop, manager, error)) return fail(error, error.empty() ? "NPC load reader/context absent" : error);
    for (size_t type = 0; type < 3; ++type) {
        Bounds initial; const auto array = manager + 0x38 + type * 0x30;
        if (!typed(memory, image, array, 0x172de80, error) || !bounds(memory, array, maximum_npcs, initial, error)) return false;
        if (initial.count) return fail(error, "NPC native load requires initially empty arrays");
    }
    frame.manager = manager; frame.reader = reader; frame.context = context; return true;
}
template<class Memory>
bool registered(Memory& memory, uintptr_t image, LoadFrame& frame, uintptr_t caller,
                uintptr_t context, int32_t id, uintptr_t object, std::string& error) {
    int32_t type = -1; uintptr_t actual_context = 0;
    if (!typed(memory, image, object, 0x1742980, error) || !at(memory, object, 0x80c, type, error) ||
        !at(memory, object, 8, actual_context, error)) return false;
    if (context != actual_context) return fail(error, "NPC registered object context differs");
    return frame.registered(caller, context, id, object, type, error);
}
// Only current members may be dereferenced. Saved pointers remain comparison
// values after removal; an array compaction cannot transfer their saved key.
template<class Lookup>
bool current_rows(const std::string& sha, const SavedPool& saved, const npc_census::State& census,
                  Lookup&& lookup, std::vector<Current>& out, std::string& error) {
    if (census.manager != saved.pool.manager) return fail(error, "NPC current owner manager differs from snapshot");
    std::vector<Current> result;
    for (const auto& old : saved.pool.records) {
        for (const auto& row : census.rows) {
            if (row.local_pointer != old.pointer && row.local_uid != old.uid) continue;
            if (row.local_pointer != old.pointer || row.local_uid != old.uid || row.type != old.type)
                return fail(error, "NPC saved pointer/UID/type was replaced");
            uintptr_t found = 0;
            if (!lookup(old.uid, found, error)) return false;
            if (found != row.local_pointer) return fail(error, "NPC current UID lookup differs from manager member");
            auto canonical = key(sha, saved.region, old);
            if (!snapshot_npc_key::valid(canonical)) return fail(error, "NPC snapshot key invalid");
            result.push_back({std::move(canonical), saved.region, row});
        }
    }
    out = std::move(result); error.clear(); return true;
}
}
