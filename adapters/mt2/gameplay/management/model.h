#pragma once
#include "mt2fw/core.h"
#include <array>
#include <algorithm>
#include <limits>

namespace mt2fw::mt2::gameplay::management::model {
inline int32_t integer(const Json& value, int32_t minimum=0, int32_t maximum=INT32_MAX) {
    if (!value.is_number_integer() || (value.is_number_unsigned()
        ? value.get<uint64_t>()>uint64_t(maximum)
        : value.get<int64_t>()<minimum || value.get<int64_t>()>maximum))
        throw std::runtime_error("Management integer outside native domain");
    return value.get<int32_t>();
}
inline bool boolean(const Json& value) {
    if (!value.is_boolean()) throw std::runtime_error("Management Boolean required");
    return value.get<bool>();
}
inline std::string text(const Json& value, size_t maximum) {
    auto result=value.get<std::string>();
    if(result.size()>maximum || result.find('\0')!=std::string::npos)
        throw std::runtime_error("Management text exceeds native extent or contains NUL");
    return result;
}
inline constexpr std::array<const char*,7> policy_names{
    "enabled","watchSeconds","checkLog","isBannable","banFavorites","banInfluencers","warnsBeforeBan"};
inline constexpr std::array<size_t,7> policy_offsets{0x18,0x1c,0x20,0x21,0x22,0x23,0x24};
inline bool policy_integer(size_t field){return field==1 || field==6;}
inline int32_t policy_max(size_t field){return field==1?3600:field==6?30:1;}
inline size_t policy_field(const std::string& name) {
    for(size_t i=0;i<policy_names.size();++i)if(name==policy_names[i])return i;
    throw std::runtime_error("Unknown GM policy field: "+name);
}
inline std::array<int32_t,3> next_version(std::array<int32_t,3> current,int32_t type) {
    for(auto value:current)if(value<0)throw std::runtime_error("Release current version is negative");
    if(type<1 || type>3)throw std::runtime_error("Release type must be 1 (hotfix), 2 (minor), or 3 (major)");
    const auto index=size_t(3-type);
    if(current[index]==INT32_MAX)throw std::runtime_error("Release version overflow");
    ++current[index];for(size_t n=index+1;n<3;++n)current[n]=0;
    return current;
}
inline std::array<int32_t,10> allocation(const Json& value) {
    if(!value.is_array() || value.size()!=10)throw std::runtime_error("Statistics allocation requires ten entries");
    std::array<int32_t,10> out{};int64_t sum=0;
    for(size_t n=0;n<10;++n){out[n]=integer(value[n]);sum+=out[n];}
    if(sum<=0 || sum>INT32_MAX)throw std::runtime_error("Statistics allocation total must be positive int32");
    return out;
}
}
