#pragma once
#include "../context.h"
#include <optional>
namespace mt2fw::mt2::gameplay::world_tools {
std::optional<Json> query(Context&,const std::string&,const Json&);
bool action(Context&,const std::string&,const Json&,bool& attempted,Json& result,std::string& cause);
}
