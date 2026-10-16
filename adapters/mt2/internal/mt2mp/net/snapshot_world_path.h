#pragma once
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include "vendor/nlohmann/json.hpp"

namespace mt2mp::net::snapshot_world_path {
inline std::filesystem::path utf8_path(std::string_view value) {
    return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(value.data()),value.size()));
}
inline std::string utf8(const std::filesystem::path& value) {
    const auto bytes=value.u8string();
    return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};
}
inline bool valid_folder(std::string_view value) {
    if(value.empty()||value.size()>200||value=="."||value==".."||
       value.find("..")!=std::string_view::npos||value.back()==' '||value.back()=='.')return false;
    for(const unsigned char c:value)if(c<32||c==127||std::string_view("/\\:\"<>|?*").find(char(c))!=std::string_view::npos)return false;
    const auto dot=value.find('.');
    auto base=std::string(value.substr(0,dot));
    for(auto& c:base)if(c>='a'&&c<='z')c=char(c-'a'+'A');
    if(base=="CON"||base=="PRN"||base=="AUX"||base=="NUL"||
       (base.size()==4&&(base.starts_with("COM")||base.starts_with("LPT"))&&base[3]>='1'&&base[3]<='9'))return false;
    try {
        // The native game-folder string and PhysFS virtual names use UTF-8.
        const auto folder=utf8_path(value);
        return !folder.is_absolute()&&!folder.has_root_path()&&folder.filename()==folder;
    } catch(const std::filesystem::filesystem_error&) {return false;}
}
inline bool valid_uuid(std::string_view value) {
    return value.size()==36&&value.find_first_not_of("0123456789abcdef-")==std::string_view::npos;
}
inline void require(std::string_view folder,std::string_view uuid) {
    if(!valid_folder(folder))throw std::runtime_error("snapshot world_name is not one safe native game-folder component");
    if(!valid_uuid(uuid))throw std::runtime_error("snapshot path UUID invalid");
}
inline std::string from_manifest(const nlohmann::json& manifest) {
    const auto found=manifest.find("world_name");
    if(found==manifest.end())return "MPTestSession";
    if(!found->is_string())throw std::runtime_error("snapshot world_name requires string");
    auto name=found->get<std::string>();
    if(!valid_folder(name))throw std::runtime_error("snapshot world_name is not one safe native game-folder component");
    return name;
}
inline std::string load(const std::string& folder,const std::string& uuid) {
    require(folder,uuid);
    return "save/"+folder+"/snapshot-"+uuid+".vrz";
}
inline std::string capture(const std::string& folder,const std::string& uuid) {return "user/"+load(folder,uuid);}
inline std::filesystem::path physical(const std::filesystem::path& write,const std::string& folder,const std::string& uuid) {
    require(folder,uuid);
    return write/"save"/utf8_path(folder)/("snapshot-"+uuid+".vrz");
}
}
