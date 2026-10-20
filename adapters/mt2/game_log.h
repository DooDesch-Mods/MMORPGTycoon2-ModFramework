#pragma once
#include <string>
#include <string_view>

namespace mt2fw::mt2 {
// The caller must first accept the adapter's exact executable build.
bool game_log(std::string_view message, std::string& error);
}
