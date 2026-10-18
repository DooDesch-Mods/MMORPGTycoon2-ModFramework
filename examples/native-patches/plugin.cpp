// SPDX-License-Identifier: MIT
#include "mt2fw/plugin.h"
#include "native_patches.h"
#include <windows.h>
#include <atomic>
#include <cstdio>

static const MT2FW_Host* host;
static MT2FW_MT2_PatchesV1 patches{};
static std::atomic_uint64_t completed{0};
static uint64_t update_subscription;
static bool attempted = false, registered = false, retirement_failed = false;
static MT2FW_MT2_Result last_retirement = MT2FW_MT2_UNAVAILABLE;

static void report(const char* operation, MT2FW_MT2_Result result,
                   const char* cause) {
    char line[512];
    std::snprintf(line, sizeof(line), "%s: result=%u cause=%s\n",
                  operation, unsigned(result), cause);
    OutputDebugStringA(line);
}
static MT2FW_MT2_Result MT2FW_CALL before(
    void*, const MT2FW_MT2_PatchEvent* call, MT2FW_MT2_PatchEdit*) {
    return call && call->size == sizeof(*call)
        ? MT2FW_MT2_OK : MT2FW_MT2_INVALID;
}
static MT2FW_MT2_Result MT2FW_CALL after(
    void*, const MT2FW_MT2_PatchEvent* call) {
    if (!call || call->size != sizeof(*call)) return MT2FW_MT2_INVALID;
    completed.fetch_add(1, std::memory_order_relaxed);
    return MT2FW_MT2_OK;
}
static void MT2FW_CALL update(void*, const char*, size_t) {
    if (!attempted) {
        attempted = true;
        auto module = GetModuleHandleW(L"MT2Framework.dll");
        auto query = module ? reinterpret_cast<MT2FW_MT2_QueryPatchesFn>(
            GetProcAddress(module, "MT2FW_MT2_QueryPatches")) : nullptr;
        if (!query) {
            report("QueryPatches", MT2FW_MT2_UNAVAILABLE, "Export unavailable");
            return;
        }
        auto result = query(host, MT2FW_MT2_PATCHES_VERSION,
                            &patches, sizeof(patches));
        if (result != MT2FW_MT2_OK) {
            report("QueryPatches", result, "Extension query rejected");
            return;
        }
        MT2FW_MT2_PatchSpec spec{};
        spec.size = sizeof(spec);
        spec.target = MT2FW_MT2_PATCH_OBJECTS_CREATE;
        spec.mode = MT2FW_MT2_PATCH_OBSERVE;
        spec.id = "observe";
        spec.on_before = before;
        spec.on_after = after;
        MT2FW_MT2_PatchReceipt receipt{sizeof(receipt)};
        result = patches.register_patch(patches.context, &spec, &receipt);
        registered = result == MT2FW_MT2_OK;
        report("Register observe", result, receipt.cause);
    }
    if (!registered || retirement_failed ||
        completed.load(std::memory_order_relaxed) == 0) return;
    MT2FW_MT2_PatchReceipt receipt{sizeof(receipt)};
    auto result = patches.unregister_patch(patches.context, "observe", &receipt);
    if (result != last_retirement) report("Retire observe", result, receipt.cause);
    last_retirement = result;
    if (result == MT2FW_MT2_OK) registered = false;
    else if (result != MT2FW_MT2_BUSY) retirement_failed = true;
    // BUSY keeps the callbacks and their process-lifetime state valid.
}
extern "C" MT2FW_EXPORT int MT2FW_CALL MT2FW_PluginQuery(MT2FW_PluginInfo* info) {
    if (!info || info->size < sizeof(*info) ||
        info->abi_version != MT2FW_ABI_VERSION) return 0;
    info->id = "example.native-patches";
    info->version = "1.0.0";
    return 1;
}
extern "C" MT2FW_EXPORT int MT2FW_CALL MT2FW_PluginLoad(const MT2FW_Host* value) {
    if (!value || value->size < sizeof(*value) ||
        value->abi_version != MT2FW_ABI_VERSION) return 0;
    host = value;
    update_subscription = host->subscribe(host->context, "update", update, nullptr);
    return update_subscription != 0;
}
extern "C" MT2FW_EXPORT void MT2FW_CALL MT2FW_PluginShutdown() {
    host->unsubscribe(host->context, update_subscription);
    host = nullptr;
}
