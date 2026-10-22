#pragma once
#include "../context.h"
#include <optional>

namespace mt2fw::mt2::gameplay::management {
std::optional<Json> query(Context&,const std::string& operation,const Json& arguments);
bool action(Context&,const std::string& operation,const Json& arguments,bool& attempted,Json& result,std::string& cause);
}
