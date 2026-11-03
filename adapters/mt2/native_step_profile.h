#pragma once
#include "sdk/native_services.h"
#include <array>
#include <cstdio>
#include <exception>
#include <type_traits>

namespace mt2fw::mt2::step_profile {
enum class Part : size_t {
    kernel_preproof, kernel_postproof, driver_preproof, before_step, resume,
    original_go, after_go, pause_restore, driver_postproof, finish_all, consumer_proof, count
};
inline constexpr std::array<const char*, size_t(Part::count)> names{
    "step.kernel_preproof", "step.kernel_postproof", "step.driver_preproof", "step.before_step", "step.resume",
    "step.original_go", "step.after_go", "step.pause_restore", "step.driver_postproof", "step.finish_all", "step.consumer_proof"
};
struct Counts { uint64_t entered=0, returned=0, failed=0; };
struct State {
    MT2FW_MT2_ServiceV1 api{};
    std::array<uint32_t, names.size()> sections{};
    std::array<Counts, names.size()> counts{};
    uint64_t epoch=0;
    bool attempted=false, configured=false;
    char availability_cause[160]{}, first_failure[256]{};
};
inline thread_local State state;

inline bool configure(const MT2FW_MT2_ServiceV1& api) {
    if(state.attempted) return state.configured && state.api.context==api.context;
    state.attempted=true;
    if(!api.context || !api.section_register || !api.scope_begin || !api.scope_end) {
        std::snprintf(state.availability_cause,sizeof(state.availability_cause),"Step profiling requires the existing native trace service");
        return false;
    }
    state.api=api;
    for(size_t i=0;i<names.size();++i) {
        state.sections[i]=api.section_register(api.context,names[i]);
        if(!state.sections[i]) {
            std::snprintf(state.availability_cause,sizeof(state.availability_cause),"Step trace section registration failed: %s",names[i]);
            return false;
        }
    }
    state.configured=true;
    return true;
}

class Scope {
    Part part_;
    MT2FW_MT2_Scope token_{};
    uint32_t section_=0;
    bool failed_=false;
public:
    explicit Scope(Part part) noexcept :part_(part) {
        if(!state.configured) return;
        section_=state.sections[size_t(part)];
        token_=state.api.scope_begin(state.api.context,section_);
        if(!token_.token) return;
        if(state.epoch!=token_.epoch) {
            state.epoch=token_.epoch;
            state.counts={};
            state.first_failure[0]=0;
        }
        ++state.counts[size_t(part_)].entered;
    }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
    ~Scope() noexcept {
        if(token_.token) state.api.scope_end(state.api.context,section_,token_);
    }
    void returned() noexcept {
        if(token_.token && state.epoch==token_.epoch) ++state.counts[size_t(part_)].returned;
    }
    void failed(const char* cause) noexcept {
        if(!token_.token || state.epoch!=token_.epoch || failed_) return;
        failed_=true;
        ++state.counts[size_t(part_)].failed;
        if(!state.first_failure[0])
            std::snprintf(state.first_failure,sizeof(state.first_failure),"%s: %s",names[size_t(part_)],cause);
    }
};

template<class F> auto call(Part part,F&& fn) {
    Scope scope(part);
    try {
        if constexpr(std::is_void_v<std::invoke_result_t<F>>) {
            fn(); scope.returned();
        } else {
            auto result=fn(); scope.returned(); return result;
        }
    } catch(const std::exception& error) { scope.failed(error.what()); throw; }
    catch(...) { scope.failed("unknown native exception"); throw; }
}
template<class F> MT2FW_MT2_Result consumer_proof(F&& fn) {
    Scope scope(Part::consumer_proof);
    try {
        const auto result=fn(); scope.returned();
        if(result!=MT2FW_MT2_OK) {
            char cause[80]{};
            std::snprintf(cause,sizeof(cause),"callback returned status %u",unsigned(result));
            scope.failed(cause);
        }
        return result;
    } catch(const std::exception& error) { scope.failed(error.what()); throw; }
    catch(...) { scope.failed("unknown native exception"); throw; }
}
}
