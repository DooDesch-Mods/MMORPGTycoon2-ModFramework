#pragma once
#include "../context.h"
namespace mt2fw::mt2::gameplay::actors {
std::optional<Json> query(Context&, const std::string&, const Json&);
bool action(Context&, const std::string&, const Json&, bool&, Json&, std::string&);
}
