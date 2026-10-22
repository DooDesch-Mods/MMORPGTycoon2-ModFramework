#pragma once
#include "region_type.h"
#include "../modular/common.h"
#include <algorithm>
#include <set>
namespace mt2fw::mt2::gameplay::region_type::detail {
using modular::detail::need;
inline int32_t integer(const Json& j,int32_t low,int32_t high){need(j.is_number_integer()&&(j.is_number_unsigned()?j.get<uint64_t>()<=uint64_t(high):j.get<int64_t>()>=low&&j.get<int64_t>()<=high),"Region type integer is outside its native domain");return j.get<int32_t>();}
inline int32_t requested_type(const Json& j){return integer(j,0,1);}
inline std::set<uint64_t> union_members(const std::map<std::string,std::set<uint64_t>>& families){std::set<uint64_t> out;for(const auto& [name,ids]:families){(void)name;for(auto uid:ids)need(uid&&out.insert(uid).second,"Region clear member UID is zero or belongs to multiple manager families");}return out;}
inline void cleared(const std::map<std::string,std::set<uint64_t>>& before,const std::map<std::string,std::set<uint64_t>>& after){need(before.size()==after.size(),"Region clear manager family set changed");for(const auto& [family,ids]:before){(void)ids;need(after.contains(family)&&after.at(family).empty(),"Region native ClearContents retained manager members: "+family);}}
}
