#pragma once
#include "snapshot_building_key.h"
#include <array>

namespace mt2mp::game::snapshot_npc_key {
inline constexpr std::array<std::string_view, 3> type_names{"QuestGiver", "Guard", "ClassTrainer"};
inline std::string root_suffix(int32_t region, int32_t type, int32_t child) {
    if (region < 0 || type < 0 || type >= 3 || child < 0) return {};
    return "inline:region{" + std::to_string(region) + "}/npcManager/type/" +
        std::string(type_names[static_cast<size_t>(type)]) + "/npc[" + std::to_string(child) + "]";
}
inline bool valid(std::string_view value) {
    constexpr std::string_view domain = ":world:npc:";
    if (value.size() <= 64 + domain.size() ||
        value.substr(0, 64).find_first_not_of("0123456789abcdef") != std::string_view::npos ||
        value.substr(64, domain.size()) != domain) return false;
    const auto suffix = value.substr(64 + domain.size());
    if (!suffix.starts_with("inline:")) return snapshot_building_key::number(suffix, INT32_MAX, true);
    constexpr std::string_view prefix = "inline:region{", middle = "}/npcManager/type/";
    if (!suffix.starts_with(prefix) || !suffix.ends_with(']')) return false;
    const auto split = suffix.find(middle, prefix.size());
    if (split == std::string_view::npos ||
        !snapshot_building_key::number(suffix.substr(prefix.size(), split - prefix.size()), INT32_MAX, false)) return false;
    const auto tail = suffix.substr(split + middle.size());
    for (const auto type : type_names) {
        const auto marker = std::string(type) + "/npc[";
        if (tail.starts_with(marker))
            return snapshot_building_key::number(tail.substr(marker.size(), tail.size() - marker.size() - 1), INT32_MAX, false);
    }
    return false;
}
}
