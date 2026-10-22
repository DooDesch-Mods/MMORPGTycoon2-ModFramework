#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

namespace mt2fw::mt2::gameplay::dungeons::detail {
inline bool committed_history(const std::vector<uintptr_t>& before,const std::vector<uintptr_t>& after,
                              bool redo_empty,uint8_t locked,int32_t pending_delete,uintptr_t active_graph) {
    if(after.empty() || before.size()>20 || locked!=1 || pending_delete || active_graph || !redo_empty)return false;
    const auto graph=after.back();if(!graph || std::find(before.begin(),before.end(),graph)!=before.end())return false;
    auto expected=before;expected.push_back(graph);if(expected.size()>20)expected.erase(expected.begin());return after==expected;
}
}
