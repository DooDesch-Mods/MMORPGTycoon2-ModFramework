#pragma once
#include "mt2fw/core.h"
#include <stdexcept>
#include <utility>

namespace mt2fw::mt2::gameplay::dungeons::detail {
template<class Execute>bool outcome(Json& result,std::string& cause,Execute&& execute) {
    try {
        const bool confirmed=std::forward<Execute>(execute)();
        if(!confirmed){if(cause.empty())cause="Dungeon operation returned without confirmation";result=nullptr;return false;}
        cause.clear();return true;
    }catch(const std::exception& error){cause=error.what();result=nullptr;return false;}
}
}
