#pragma once
#include <algorithm>
#include <string>
namespace mt2fw::mt2 {
inline bool initial_save_path(const std::string& name,const std::string& folder) {
    if(folder.empty() || folder=="." || folder==".." || folder.find_first_of("/\\:")!=std::string::npos)return false;
    const auto target="save/"+folder+"/initial_state.vrz";
    if(name==target)return true;
    const auto temporary="tmp/"+target+".";
    if(!name.starts_with(temporary) || name.size()==temporary.size())return false;
    const auto suffix=name.substr(temporary.size());
    return std::all_of(suffix.begin(),suffix.end(),[](char value){return value>='0'&&value<='9';});
}
}
