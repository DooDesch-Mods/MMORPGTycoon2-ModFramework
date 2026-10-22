#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
namespace mt2fw::mt2::gameplay::actors::model {
using Json = nlohmann::json;
inline void need(bool value, const std::string& cause) {if (!value) throw std::runtime_error(cause);}
inline int32_t integer(const Json& value, int32_t minimum = INT32_MIN, int32_t maximum = INT32_MAX) {
    need(value.is_number_integer(), "actor value must be integral");
    if (value.is_number_unsigned()) need(value.get<uint64_t>() <= uint64_t(INT32_MAX), "actor integer exceeds int32");
    auto out = value.get<int64_t>(); need(out >= minimum && out <= maximum, "actor integer is outside its native bound"); return int32_t(out);
}
inline std::string text(const Json& value, size_t maximum = 4096) {
    need(value.is_string(), "actor text must be a string"); auto out = value.get<std::string>();
    need(out.size() <= maximum && out.find('\0') == std::string::npos, "actor text exceeds its bound or contains a NUL"); return out;
}
inline std::array<float,4> color(const Json& value) {
    need(value.is_array() && value.size() == 4, "phase color requires four scalars"); std::array<float,4> out{};
    for (size_t i=0;i<4;++i) {need(value[i].is_number(), "phase color must be numeric"); out[i]=value[i].get<float>(); need(std::isfinite(out[i]), "phase color must be finite");} return out;
}
inline Json phase_update(const Json& current, const Json& settings) {
    need(settings.is_object() && !settings.empty(), "phase settings must be a nonempty table"); auto out=current;
    for (auto it=settings.begin();it!=settings.end();++it) {
        const auto& key=it.key();
        if(key=="name" || key=="vfx") out[key]=text(it.value());
        else if(key=="health") out[key]=integer(it.value());
        else if(key=="color") out[key]=color(it.value());
        else throw std::runtime_error("unknown phase setting: "+key);
    }
    return out;
}
inline size_t reordered_index(size_t source,size_t boundary,size_t count) {
    need(source<count && boundary<=count, "phase reorder index/boundary is absent"); return boundary>source?boundary-1:boundary;
}
struct Pose {std::array<float,4> rotation;std::array<float,3> position,scale;};
inline Pose pose(const Json& value) {
    need(value.is_object() && value.size()==3 && value.contains("rotation") && value.contains("position") && value.contains("scale"),"actor pose requires rotation, position and scale");Pose out{};
    auto read=[](const Json& in,auto& fields){need(in.is_array() && in.size()==fields.size(),"actor pose component count differs");for(size_t i=0;i<fields.size();++i){need(in[i].is_number(),"actor pose component must be numeric");fields[i]=in[i].template get<float>();need(std::isfinite(fields[i]) && std::abs(fields[i])<=100000000,"actor pose component is not finite or exceeds world bound");}};
    read(value.at("rotation"),out.rotation);read(value.at("position"),out.position);read(value.at("scale"),out.scale);double norm=0;for(auto v:out.rotation)norm+=double(v)*v;need(std::abs(norm-1)<=0.001,"actor pose rotation must be a unit quaternion");for(auto v:out.scale)need(v>0,"actor pose scale must be positive");return out;
}
inline Json charge(const Json& before,int32_t cost,size_t line) {
    need(before.at("present")==true && line<20,"actor purchase requires current native accounting record");auto after=before;
    auto delta=[](Json& field,int32_t change){auto v=int64_t(integer(field))+change;need(v>=INT32_MIN && v<=INT32_MAX,"actor accounting delta exceeds int32");field=int32_t(v);};
    delta(after["cash"],-cost);delta(after["record_cash"],-cost);delta(after["expense"],cost);delta(after["line_counts"][line],1);delta(after["line_income"][line],-cost);return after;
}
inline bool retires_quest(const Json& row,const std::string& uid) {return row.at("handle").at("owner").at("uid")==uid || row.at("destination").at("uid")==uid;}
inline Json surviving_quests(const Json& rows,const std::string& uid) {
    Json out=Json::array();for(const auto& row:rows)if(!retires_quest(row,uid)){auto value=row;auto slot=integer(row.at("handle").at("slot"),0);int32_t preceding=0;
        for(const auto& removed:rows)if(retires_quest(removed,uid) && removed.at("handle").at("owner")==row.at("handle").at("owner") && integer(removed.at("handle").at("slot"),0)<slot)++preceding;
        value["handle"]["slot"]=slot-preceding;value["handle"]["sequence"]=slot-preceding;out.push_back(value);
    }return out;
}
inline int64_t coordinate(const Json& value) {need(value.is_number_integer(),"Zone polygon coordinate must be integral");if(value.is_number_unsigned())need(value.get<uint64_t>()<=1000000000000ULL,"Zone polygon coordinate exceeds native bound");auto out=value.get<int64_t>();need(out>=-1000000000000LL && out<=1000000000000LL,"Zone polygon coordinate exceeds native bound");return out;}
inline void polygon(const Json& value) {
    need(value.is_array() && !value.empty() && value.size()<=128,"Zone polygon requires 1 through 128 paths");size_t remaining=8192;
    for(const auto& path:value){need(path.is_array() && path.size()>=3 && path.size()<=remaining,"Zone polygon path requires at least three vertices within its total bound");remaining-=path.size();for(const auto& point:path){need(point.is_object() && point.size()==2 && point.contains("x") && point.contains("z"),"Zone polygon point requires x and z");coordinate(point.at("x"));coordinate(point.at("z"));}}
}
}
