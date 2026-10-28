#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::npc_census {

struct Memory {
    virtual ~Memory() = default;
    virtual bool copy(uintptr_t address, void* destination, size_t size, std::string& error) = 0;
};

struct Row {
    uintptr_t local_pointer = 0;
    uint64_t local_uid = 0;
    int32_t type = -1;
    int32_t local_tag = -1;
    int32_t character_slot = -1;
    int32_t character_type = -1;
    int32_t level = -1;
    int32_t quest_count = -1;
    std::string name;
    std::array<float, 4> rotation{};
    std::array<float, 3> position{};
    std::array<float, 3> scale{};
    bool operator==(const Row&) const = default;
};

struct State {
    uintptr_t manager = 0;
    uint32_t next_local_tag = 0;
    std::array<uint64_t, 4> global_random{};
    std::vector<Row> rows;
};

struct InventoryHeader {
    uintptr_t items = 0;
    int32_t count = 0;
    bool operator==(const InventoryHeader&) const = default;
};

inline bool reject(std::string& error, const char* reason) {
    error = reason;
    return false;
}

inline bool same_semantic(const Row& a, const Row& b) {
    return a.local_pointer == b.local_pointer && a.local_tag == b.local_tag &&
        a.type == b.type && a.character_slot == b.character_slot &&
        a.character_type == b.character_type && a.level == b.level &&
        a.quest_count == b.quest_count && a.name == b.name &&
        a.rotation == b.rotation && a.position == b.position && a.scale == b.scale;
}

inline bool added_one(const State& before, const State& after, Row& created,
                      std::string& error) {
    if (!before.manager || before.manager != after.manager ||
        after.rows.size() != before.rows.size() + 1)
        return reject(error, "NPC placement manager or total count differs");
    std::set<uint64_t> prior;
    for (const auto& old : before.rows) prior.insert(old.local_uid);
    if (prior.size() != before.rows.size())
        return reject(error, "NPC placement before UIDs duplicate");
    std::set<uint64_t> after_uids;
    Row candidate;
    bool found = false;
    for (const auto& now : after.rows) {
        if (!now.local_uid || !after_uids.insert(now.local_uid).second)
            return reject(error, "NPC placement after UID absent or duplicated");
        if (!prior.contains(now.local_uid)) {
            if (found) return reject(error, "NPC placement created more than one UID");
            candidate = now;
            found = true;
            continue;
        }
        bool matched = false;
        for (const auto& old : before.rows) {
            if (old.local_uid == now.local_uid) {
                if (!same_semantic(old, now))
                    return reject(error, "NPC placement changed an existing NPC");
                matched = true;
                break;
            }
        }
        if (!matched) return reject(error, "NPC placement old UID match absent");
    }
    if (!found || candidate.type < 0 || candidate.type > 2 ||
        (candidate.type != 0 && candidate.quest_count != 0))
        return reject(error, "NPC placement created type or non-QuestGiver quest list differs");
    created = std::move(candidate);
    error.clear();
    return true;
}

template<class T>
bool field(Memory& memory, uintptr_t base, size_t offset, T& value, std::string& error) {
    if (!base || base > UINTPTR_MAX - offset)
        return reject(error, "NPC census address is null or overflows");
    return memory.copy(base + offset, &value, sizeof value, error);
}

inline bool read_inventory_header(Memory& memory, uintptr_t inventory,
                                  InventoryHeader& output, std::string& error) {
    InventoryHeader header;
    if (!field(memory, inventory, 8, header.items, error) ||
        !field(memory, inventory, 0x10, header.count, error)) return false;
    if (header.count < 0 || header.count > 4096 ||
        (header.count && !header.items) ||
        header.items > UINTPTR_MAX - size_t(header.count) * sizeof(uintptr_t))
        return reject(error, "NPC native inventory pointer/count span differs");
    output = header;
    error.clear();
    return true;
}

inline bool utf8(const std::string& text) {
    for (size_t i = 0; i < text.size();) {
        const unsigned char a = static_cast<unsigned char>(text[i]);
        size_t width = 0;
        uint32_t value = 0;
        if (a < 0x80) { ++i; continue; }
        if (a >= 0xc2 && a <= 0xdf) { width = 2; value = a & 0x1f; }
        else if (a >= 0xe0 && a <= 0xef) { width = 3; value = a & 0x0f; }
        else if (a >= 0xf0 && a <= 0xf4) { width = 4; value = a & 0x07; }
        else return false;
        if (i + width > text.size()) return false;
        for (size_t j = 1; j < width; ++j) {
            const unsigned char b = static_cast<unsigned char>(text[i + j]);
            if ((b & 0xc0) != 0x80) return false;
            value = (value << 6) | (b & 0x3f);
        }
        if ((width == 2 && value < 0x80) || (width == 3 && value < 0x800) ||
            (width == 4 && value < 0x10000) ||
            (value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff)
            return false;
        i += width;
    }
    return true;
}

inline bool read_name(Memory& memory, uintptr_t npc, std::string& name, std::string& error) {
    uintptr_t data = 0;
    size_t length = 0;
    if (!field(memory, npc, 0x38, data, error) ||
        !field(memory, npc, 0x40, length, error)) return false;
    if (!data || !length || length > 256 || data > UINTPTR_MAX - length)
        return reject(error, "NPC name native GNU string span differs");
    name.resize(length);
    if (!memory.copy(data, name.data(), length, error)) return false;
    if (name.find('\0') != std::string::npos || !utf8(name))
        return reject(error, "NPC name has NUL or invalid UTF-8");
    return true;
}

inline bool read_global_random(Memory& memory,uintptr_t image,std::array<uint64_t,4>& out,std::string& error){
    uintptr_t global=0;
    if(!field(memory,image,0x164b250,global,error))return false;
    if(!global)return reject(error,"NPC global random source absent");
    return field(memory,global,0,out,error);
}

inline bool read(Memory& memory, uintptr_t image, uintptr_t region,
                 State& output, std::string& error) {
    if (!image || !region) return reject(error, "NPC census image or region is null");
    State result;
    if (!read_global_random(memory, image, result.global_random, error) ||
        !field(memory, region, 0x998, result.manager, error)) return false;
    if (!result.manager) return reject(error, "NPC region manager absent");
    uintptr_t owner = 0;
    if (!field(memory, result.manager, 0xc8, owner, error) || owner != region)
        return reject(error, "NPC manager actual region owner differs");
    if (!field(memory, result.manager, 0xdc, result.next_local_tag, error)) return false;
    std::set<uintptr_t> seen;
    std::set<uint64_t> seen_uids;
    for (int32_t type = 0; type < 3; ++type) {
        uintptr_t entries = 0;
        int32_t count = 0, capacity = 0;
        const size_t bucket = size_t(type) * 0x30;
        if (!field(memory, result.manager, 0x40 + bucket, entries, error) ||
            !field(memory, result.manager, 0x48 + bucket, count, error) ||
            !field(memory, result.manager, 0x4c + bucket, capacity, error)) return false;
        if (count < 0 || count > 100000 || capacity < count || capacity > 100000 ||
            (count && !entries) || result.rows.size() + size_t(count) > 100000)
            return reject(error, "NPC manager type array span differs");
        for (int32_t index = 0; index < count; ++index) {
            Row row;
            if (!field(memory, entries, size_t(index) * 8, row.local_pointer, error)) return false;
            if (!row.local_pointer || !seen.insert(row.local_pointer).second)
                return reject(error, "NPC manager entry is null or duplicated");
            uintptr_t ap = 0, actual_region = 0;
            if (!field(memory, row.local_pointer, 0, ap, error) ||
                !field(memory, row.local_pointer, 0x18, row.local_uid, error) ||
                !field(memory, row.local_pointer, 0x178, actual_region, error) ||
                !field(memory, row.local_pointer, 0x80c, row.type, error) ||
                !field(memory, row.local_pointer, 0x830, row.local_tag, error) ||
                !field(memory, row.local_pointer, 0x4a8, row.character_slot, error) ||
                !field(memory, row.local_pointer, 0x4a4, row.character_type, error) ||
                !field(memory, row.local_pointer, 0x6f8, row.level, error) ||
                !field(memory, row.local_pointer, 0x870, row.quest_count, error) ||
                !field(memory, row.local_pointer, 0x100, row.rotation, error) ||
                !field(memory, row.local_pointer, 0x110, row.position, error) ||
                !field(memory, row.local_pointer, 0x11c, row.scale, error) ||
                !read_name(memory, row.local_pointer, row.name, error)) return false;
            if (ap != image + 0x1742980 || actual_region != region || row.type != type ||
                !row.local_uid || !seen_uids.insert(row.local_uid).second ||
                row.local_tag < 0 || row.local_tag > 99 || row.character_slot < 0 ||
                row.character_type < 0 || row.level < 0 || row.quest_count < 0 || row.quest_count > 100000)
                return reject(error, "NPC native class, region, type or fields differ");
            result.rows.push_back(std::move(row));
        }
    }
    output = std::move(result);
    error.clear();
    return true;
}

} // namespace mt2mp::npc_census
