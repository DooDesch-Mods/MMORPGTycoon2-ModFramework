#pragma once
#include "mt2fw/core.h"
#include <array>
#include <climits>
#include <cmath>
#include <stdexcept>

namespace mt2fw::mt2::gameplay::dungeons::detail {
inline void valid(bool condition,const std::string& cause) {if(!condition)throw std::invalid_argument(cause);}
inline int32_t integer(const Json& value,int32_t minimum,int32_t maximum,const char* name) {
    valid(value.is_number_integer(),std::string("Dungeon ")+name+" requires an integer");
    if(value.is_number_unsigned())valid(value.get<uint64_t>()<=uint64_t(maximum),std::string("Dungeon ")+name+" exceeds its native integer range");
    const auto number=value.get<int64_t>();valid(number>=minimum && number<=maximum,std::string("Dungeon ")+name+" must be "+std::to_string(minimum)+".."+std::to_string(maximum));return int32_t(number);
}
template<size_t N>std::array<int32_t,N> integers(const Json& value,const char* name) {
    valid(value.is_array() && value.size()==N,std::string("Dungeon ")+name+" has invalid dimensions");std::array<int32_t,N> out{};
    for(size_t i=0;i<N;++i)out[i]=integer(value[i],INT32_MIN,INT32_MAX,name);return out;
}
template<size_t N>std::array<float,N> numbers(const Json& value,const char* name) {
    valid(value.is_array() && value.size()==N,std::string("Dungeon ")+name+" has invalid dimensions");std::array<float,N> out{};
    for(size_t i=0;i<N;++i){valid(value[i].is_number(),std::string("Dungeon ")+name+" requires numeric channels");out[i]=value[i].get<float>();valid(std::isfinite(out[i]),std::string("Dungeon ")+name+" is non-finite");}return out;
}
inline std::array<std::array<float,4>,4> color_input(const Json& value) {
    valid(value.is_array() && value.size()==4,"Dungeon colors require four RGBA arrays");std::array<std::array<float,4>,4> out{};
    for(size_t i=0;i<4;++i){out[i]=numbers<4>(value[i],"color");for(float x:out[i])valid(x>=0 && x<=1,"Dungeon color channel must be 0..1");}return out;
}
}
