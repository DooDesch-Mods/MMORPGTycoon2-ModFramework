#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace mt2fw::mt2::gameplay::paths::detail {
inline const char* history_transition_cause(const std::vector<uintptr_t>& source_before,const std::vector<uintptr_t>& other_before,
                                          const std::vector<uintptr_t>& source_after,const std::vector<uintptr_t>& other_after,
                                          uintptr_t expected_top,uint8_t locked,int32_t pending_delete,uintptr_t active) {
    if(source_before.empty() || !expected_top || source_before.back()!=expected_top)return "Path history source is not the exact owned stack top";
    if(source_before.size()>20 || other_before.size()>20)return "Path history native stack exceeds its bound";
    auto source=source_before;source.pop_back();if(source_after!=source)return "Path history source stack did not pop the exact owned graph";
    if(other_after.empty())return "Path history inverse stack is empty";const auto inverse=other_after.back();
    if(!inverse || std::find(other_before.begin(),other_before.end(),inverse)!=other_before.end())return "Path history inverse graph is absent or reused";
    if(locked!=1 || pending_delete || active)return "Path history inverse graph is not drained and locked";
    auto other=other_before;other.push_back(inverse);if(other.size()>20)other.erase(other.begin());if(other_after!=other)return "Path history inverse stack did not append its original graph";return nullptr;
}
}
