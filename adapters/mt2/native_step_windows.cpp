#include "native_step_windows.h"
#include "native_step_pins.h"
#include "crypto.h"
#include <windows.h>
#include <tuple>

namespace mt2fw::mt2 {
namespace {
void require(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
}
NativeStepDriver::Backend make_native_step_backend(uintptr_t image,const fs::path& executable,NativeRoutes& routes,
    StepNativeHooks hooks,std::function<void(const MT2FW_MT2_StepBoundary&)> before_step){
    require(image&&image==reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),"Step native image is not the loaded executable");
    require(file_digest(executable)==step_pins::executable_sha256,"Step executable SHA256 differs");
    require(hooks.original_go&&hooks.original_timer&&hooks.go_detour&&hooks.timer_detour&&before_step,
        "Step Framework Go/Timer/bootstrap hooks are incomplete");
    auto prove=[image,hooks,&routes]{
        require(image==reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),"Step loaded executable base changed");
        std::string cause;
        if(!routes.prove_framework_hook("framework.step.go",hooks.go_detour,reinterpret_cast<void*>(hooks.original_go),cause))
            throw std::runtime_error("Step Go owner proof failed: "+cause);
        if(!routes.prove_framework_hook("framework.step.timer",hooks.timer_detour,reinterpret_cast<void*>(hooks.original_timer),cause))
            throw std::runtime_error("Step Timer owner proof failed: "+cause);
    };
    prove();
    // Full native body admission is an initialization check. Per-step checks
    // read the existing acknowledged hook windows, without hashing files.
    for(const auto& [rva,bytes,sha]:std::array<std::tuple<uint64_t,size_t,const char*>,6>{{
            {step_pins::go_rva,step_pins::go_bytes,step_pins::go_sha256},
            {step_pins::timer_rva,step_pins::timer_bytes,step_pins::timer_sha256},
            {0x526240,272,"63648e01ead82806b5d976287890d6653f1e5745387b356aaaeb629d1ea09f87"},
            {0x526350,160,"0acc0efdcbc9ed4445426d1bd10320d9b5827bf3781840eef1b2e467e2bc2c1a"},
            {0x526d50,480,"666f4cac19d8d04df16bdff53afbcc87899ad708f157aeae5674cf284a4540fa"},
            {0x525ae0,112,"ffb5c39d83679b65d6d17eed0f5e6031cd7cd967efd3bff784713138a5940046"}}}){
        std::string cause;
        if(!routes.verify_body(rva,bytes,sha,cause))
            throw std::runtime_error("Step native body proof failed rva="+std::to_string(rva)+": "+cause);
    }
    NativeStepDriver::Backend out;out.image=image;
    out.thread=[]{return uint64_t(GetCurrentThreadId());};out.prove_routes=std::move(prove);
    out.read=[](uintptr_t at,void* data,size_t bytes){SIZE_T copied=0;
        if(!at||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(at),data,bytes,&copied)||copied!=bytes)
            throw std::runtime_error("Step native read failed address="+std::to_string(at)+" required="+std::to_string(bytes)+" actual="+std::to_string(copied)+" win32="+std::to_string(GetLastError()));
    };
    out.write=[](uintptr_t at,const void* data,size_t bytes){SIZE_T copied=0;
        if(!at||!WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),data,bytes,&copied)||copied!=bytes)
            throw std::runtime_error("Step native write failed address="+std::to_string(at)+" required="+std::to_string(bytes)+" actual="+std::to_string(copied)+" win32="+std::to_string(GetLastError()));
    };
    out.original_go=hooks.original_go;out.original_timer=hooks.original_timer;
    out.pause=reinterpret_cast<StepNativeHooks::Go>(image+0x526240);
    out.resume=reinterpret_cast<StepNativeHooks::Go>(image+0x526350);
    out.finish_all=reinterpret_cast<StepNativeHooks::Go>(image+0x526d50);
    out.all_done=reinterpret_cast<bool(*)(void*)>(image+0x525ae0);
    out.before_step=std::move(before_step);return out;
}
}
