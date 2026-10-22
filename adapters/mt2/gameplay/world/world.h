#pragma once
#include "../context.h"
namespace mt2fw::mt2::gameplay::world {
std::optional<Json> query(Context&,const std::string&,const Json&);
}
