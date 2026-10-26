#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <map>
#include <stdexcept>
#include <vector>

namespace mt2fw::mt2::gameplay::regions {
using Json=nlohmann::json;
template<class Read>Json sample_grid(int32_t width,int32_t height,const Json& points,Read&& read){
    auto require=[](bool value,const char* cause){if(!value)throw std::runtime_error(cause);};
    require(width>0&&height>0&&int64_t(width)*height<=1000000,"Terrain sample grid dimensions are invalid");
    require(points.is_array()&&!points.empty()&&points.size()<=128,"Terrain samples require 1..128 grid points");
    std::vector<std::pair<int32_t,int32_t>> coordinates;
    for(const auto& point:points){
        require(point.is_object()&&point.size()==2&&point.contains("column")&&point.contains("row"),"Terrain sample requires column and row only");
        auto coordinate=[&](const char* name,int32_t extent){const auto& value=point.at(name);
            require(value.is_number_integer(),"Terrain sample coordinates must be integers");
            if(value.is_number_unsigned())require(value.get<uint64_t>()<uint64_t(extent),"Terrain sample coordinate is outside the grid");
            const auto index=value.get<int64_t>();require(index>=0&&index<extent,"Terrain sample coordinate is outside the grid");return int32_t(index);};
        coordinates.emplace_back(coordinate("column",width),coordinate("row",height));
    }
    Json out=Json::array();std::map<int32_t,float> values;
    for(const auto& [column,row]:coordinates){const auto index=row*width+column;
        if(!values.contains(index)){const auto value=read(size_t(index));require(std::isfinite(value),"Terrain sample height is not finite");values.emplace(index,value);}
        out.push_back({{"column",column},{"row",row},{"index",index},{"height",values.at(index)}});
    }
    return out;
}
}
