#pragma once
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace mt2mp::game::snapshot_building_key {
inline std::string root_suffix(int32_t region, int32_t child) {
    return "inline:region{" + std::to_string(region) + "}/buildingManager/building[" + std::to_string(child) + "]";
}
inline bool number(std::string_view text, uint64_t maximum, bool positive) {
    if (text.empty() || (text.size() > 1 && text.front() == '0') || text.front() < '0' || text.front() > '9') return false;
    uint64_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && value <= maximum && (!positive || value);
}
inline bool valid(std::string_view value) {
    if (value.size() <= 80 || value.substr(0, 64).find_first_not_of("0123456789abcdef") != std::string_view::npos ||
        value.substr(64, 16) != ":world:building:") return false;
    const auto suffix = value.substr(80);
    if (!suffix.starts_with("inline:")) return number(suffix, INT64_MAX, true);
    constexpr std::string_view prefix = "inline:region{", middle = "}/buildingManager/building[";
    if (!suffix.starts_with(prefix) || !suffix.ends_with(']')) return false;
    const auto split = suffix.find(middle, prefix.size());
    if (split == std::string_view::npos) return false;
    if(!number(suffix.substr(prefix.size(), split-prefix.size()),INT32_MAX,false))return false;
    auto children=suffix.substr(split+middle.size());
    const auto parent_end=children.find(']');
    if(parent_end==std::string_view::npos||!number(children.substr(0,parent_end),INT32_MAX,false))return false;
    children.remove_prefix(parent_end+1);
    if(children.empty())return true;
    constexpr std::string_view child_array="/buildingManager/building[";
    if(children.starts_with("/template"))children.remove_prefix(9);
    else if(children.starts_with("/instance[")){
        children.remove_prefix(10);const auto end=children.find(']');
        if(end==std::string_view::npos||!number(children.substr(0,end),INT32_MAX,false))return false;
        children.remove_prefix(end+1);
    }else return false;
    return children.starts_with(child_array)&&children.ends_with(']')&&
        number(children.substr(child_array.size(),children.size()-child_array.size()-1),INT32_MAX,false);
}
}
