#pragma once
#include "../context.h"
#include <optional>

namespace mt2fw::mt2::gameplay::dungeons {
std::optional<Json> query(Context&, const std::string& operation, const Json& args);
bool action(Context&, const std::string& operation, const Json& args,
            bool& attempted, Json& result, std::string& cause);
}
