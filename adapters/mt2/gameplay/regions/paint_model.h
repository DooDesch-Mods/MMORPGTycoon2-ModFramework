#pragma once
#include "mt2fw/core.h"
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace mt2fw::mt2::gameplay::regions::paint_model {
struct Point {int64_t x=0,z=0;bool operator==(const Point&)const=default;};
struct Stamp {std::array<float,3> position{};float radius=0;std::vector<std::vector<Point>> paths;};
inline void need(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
inline int64_t coordinate(const Json& value){
    need(value.is_number_integer(),"Terrain polygon coordinate requires an integer");
    constexpr auto bound=INT64_MAX>>2;
    if(value.is_number_unsigned())need(value.get<uint64_t>()<=uint64_t(bound),"Terrain polygon coordinate exceeds the native signed 62-bit domain");
    auto number=value.get<int64_t>();need(number>=-bound&&number<=bound,"Terrain polygon coordinate exceeds the native signed 62-bit domain");return number;
}
inline std::vector<Stamp> parse(const Json& values){
    need(values.is_array()&&!values.empty()&&values.size()<=64,"Terrain paint requires 1..64 stamps");std::vector<Stamp> out;size_t vertices=0;
    for(const auto& value:values){need(value.is_object()&&value.size()==3&&value.contains("position")&&value.contains("radius")&&value.contains("shape"),"Terrain stamp requires position, radius and shape");
        Stamp stamp;const auto& position=value.at("position");need(position.is_array()&&position.size()==3,"Terrain stamp position requires three coordinates");
        for(size_t i=0;i<3;++i){need(position[i].is_number(),"Terrain stamp position must be numeric");auto number=position[i].get<double>();
            need(std::isfinite(number)&&std::abs(number)<=1000000,"Terrain stamp position exceeds finite world bounds");stamp.position[i]=float(number);}
        need(value.at("radius").is_number(),"Terrain stamp radius must be a number");auto radius=value.at("radius").get<double>();
        need(std::isfinite(radius)&&radius>0&&radius<=std::numeric_limits<float>::max(),"Terrain stamp radius must be positive and a finite native float");stamp.radius=float(radius);
        need(std::isfinite(stamp.radius)&&stamp.radius>0,"Terrain stamp radius is not representable as a positive native float");
        const auto& paths=value.at("shape");need(paths.is_array()&&!paths.empty()&&paths.size()<=128,"Terrain stamp shape requires 1..128 polygon paths");
        for(const auto& path:paths){need(path.is_array()&&path.size()>=3&&path.size()<=16384-vertices,"Terrain paint vertex budget exceeds 16384");vertices+=path.size();auto& points=stamp.paths.emplace_back();
            for(const auto& point:path){need(point.is_object()&&point.size()==2&&point.contains("x")&&point.contains("z"),"Terrain polygon point requires integral x and z");points.push_back({coordinate(point.at("x")),coordinate(point.at("z"))});}}
        out.push_back(std::move(stamp));}return out;
}
}
