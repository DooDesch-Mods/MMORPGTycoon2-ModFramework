#pragma once
#include "native_routes.h"
namespace mt2fw::mt2 {
NativeRoutes::Backend windows_route_backend(const fs::path& executable);
void load_multiplayer_catalog(NativeRoutes& catalog);
}
