#include "mt2fw/plugin.h"
#include <string.h>
static const MT2FW_Host* api;
static void MT2FW_CALL update(void*, const char*, size_t) {}
extern "C" MT2FW_EXPORT int MT2FW_CALL MT2FW_PluginQuery(MT2FW_PluginInfo* info) {
    if (!info || info->size < sizeof(*info) || info->abi_version != MT2FW_ABI_VERSION) return 0;
    info->id = "example.native"; info->version = "1.0.0"; return 1;
}
extern "C" MT2FW_EXPORT int MT2FW_CALL MT2FW_PluginLoad(const MT2FW_Host* host) {
    if (!host || host->size < sizeof(*host) || host->abi_version != MT2FW_ABI_VERSION) return 0;
    api = host;
    return api->subscribe(api->context, "update", update, nullptr) != 0;
}
extern "C" MT2FW_EXPORT void MT2FW_CALL MT2FW_PluginShutdown() { api = nullptr; }
