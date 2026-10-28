#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <string>
namespace mt2mp::game::subscriber_activity {
// These are stored Subscriber fields. Satisfaction is not derived from NeedSet.
struct State {int32_t satisfaction=0,addiction=0;uint32_t logged_in_time_bits=0;bool operator==(const State&)const=default;};
inline constexpr std::array<size_t,3> offsets{0x24,0x28,0x6c};
inline std::array<uint32_t,3> patterns(const State& value){return {std::bit_cast<uint32_t>(value.satisfaction),std::bit_cast<uint32_t>(value.addiction),value.logged_in_time_bits};}
inline bool valid_time(uint32_t bits){const float value=std::bit_cast<float>(bits);return std::isfinite(value)&&value>=0.0f;}
inline bool valid(const State& value,std::string& error){if(!valid_time(value.logged_in_time_bits)){error="Subscriber activity logged-in time requires finite nonnegative native float bits="+std::to_string(value.logged_in_time_bits);return false;}return true;}
template<class Read>bool sample(Read read,uintptr_t image,uintptr_t owner,int32_t required_id,State& out,std::string& error){
    out={};uintptr_t type=0;int32_t id=-1;State value;
    if(!read(owner,0,type,error)||type!=image+0x16e75f0||!read(owner,0x20,id,error)||id!=required_id){if(error.empty())error="Subscriber activity native AP/ID differs";return false;}
    if(!read(owner,offsets[0],value.satisfaction,error)||!read(owner,offsets[1],value.addiction,error)||!read(owner,offsets[2],value.logged_in_time_bits,error)||!valid(value,error))return false;
    out=value;return true;
}
}
