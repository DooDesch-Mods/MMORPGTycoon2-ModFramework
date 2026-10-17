#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
namespace mt2fw {
inline bool native_initial_metadata(const nlohmann::json& data) {
    if(!data.contains("phase"))return false;
    if(!data.at("phase").is_string())throw std::runtime_error("Save metadata phase is invalid");
    if(data.at("phase")!="native_initial")return false;
    if(!data.contains("epoch") || !data.at("epoch").is_null() ||
        !data.contains("world") || !data.at("world").is_string() || data.at("world").get<std::string>().empty() ||
        !data.contains("mods") || !data.at("mods").is_object() || !data.at("mods").empty() ||
        !data.contains("objects") || !data.at("objects").is_object() || !data.at("objects").empty() ||
        !data.contains("required") || !data.at("required").is_array())
        throw std::runtime_error("Native initial metadata requires no initialized epoch, mod data or object bindings");
    return true;
}
}
