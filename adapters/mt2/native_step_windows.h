#pragma once
#include "native_step_driver.h"
#include "native_routes.h"

namespace mt2fw::mt2 {
struct StepNativeHooks {
    using Go=void(*)(void*);
    using Timer=void(*)(void*,float);
    Go original_go=nullptr;
    Timer original_timer=nullptr;
    void* go_detour=nullptr;
    void* timer_detour=nullptr;
};
// Call only after the existing Framework route owner has published both hooks.
// This function neither installs a hook nor changes the public service contract.
NativeStepDriver::Backend make_native_step_backend(uintptr_t image,const fs::path& executable,
    NativeRoutes&,StepNativeHooks,std::function<void(const MT2FW_MT2_StepBoundary&)> before_step);
}
