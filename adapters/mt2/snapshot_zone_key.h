#pragma once
#include "snapshot_building_key.h"

namespace mt2mp::game::snapshot_zone_key {
inline std::string root_suffix(int32_t region, int32_t child) {
    return "inline:region{" + std::to_string(region) + "}/zoneManager/type/Monster/zone[" + std::to_string(child) + "]";
}
inline std::string scene_suffix(int32_t region,int32_t exterior,int32_t instance,int32_t child){
    return "inline:dungeon:region{"+std::to_string(region)+"}/building["+std::to_string(exterior)+"]"+
        (instance<0?"/template/":"/instance["+std::to_string(instance)+"]/")+"monsterZone["+std::to_string(child)+"]";
}
inline bool scene_suffix_valid(std::string_view s){
    constexpr std::string_view prefix="inline:dungeon:region{";
    if(!s.starts_with(prefix))return false;s.remove_prefix(prefix.size());
    const auto number=[&](std::string_view suffix){const auto end=s.find(suffix);
        if(end==std::string_view::npos||!snapshot_building_key::number(s.substr(0,end),INT32_MAX,false))return false;
        s.remove_prefix(end+suffix.size());return true;};
    if(!number("}/building[")||!number("]"))return false;
    if(s.starts_with("/template/monsterZone["))s.remove_prefix(22);
    else if(s.starts_with("/instance[")){s.remove_prefix(10);if(!number("]/monsterZone["))return false;}
    else return false;
    return s.ends_with(']')&&snapshot_building_key::number(s.substr(0,s.size()-1),INT32_MAX,false);
}
inline bool valid(std::string_view value) {
    constexpr std::string_view domain = ":world:zone:";
    if (value.size() <= 64 + domain.size() || value.substr(0, 64).find_first_not_of("0123456789abcdef") != std::string_view::npos ||
        value.substr(64, domain.size()) != domain) return false;
    const auto suffix = value.substr(64 + domain.size());
    if (!suffix.starts_with("inline:")) return snapshot_building_key::number(suffix, INT32_MAX, true);
    if(suffix.starts_with("inline:dungeon:"))return scene_suffix_valid(suffix);
    constexpr std::string_view prefix = "inline:region{", middle = "}/zoneManager/type/Monster/zone[";
    if (!suffix.starts_with(prefix) || !suffix.ends_with(']')) return false;
    const auto split = suffix.find(middle, prefix.size());
    if (split == std::string_view::npos) return false;
    return snapshot_building_key::number(suffix.substr(prefix.size(), split - prefix.size()), INT32_MAX, false) &&
        snapshot_building_key::number(suffix.substr(split + middle.size(), suffix.size() - split - middle.size() - 1), INT32_MAX, false);
}
}
