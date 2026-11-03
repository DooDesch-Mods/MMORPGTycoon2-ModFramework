#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>

namespace mt2fw::mt2::poll_delay {
struct Context { uint64_t thread=0; };
inline thread_local const Context* active=nullptr;
class Scope {
    Context context_;
    const Context* previous_;
public:
    Scope(bool enabled,uint64_t thread) noexcept:context_{thread},previous_(active){active=enabled?&context_:nullptr;}
    ~Scope() noexcept{active=previous_;}
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
struct Counters {
    std::atomic<uint64_t> entered{0},returned{0},failed{0},requested_ms{0},selected_ms{0},elapsed_ns{0};
};
inline Counters selected,forwarded;
inline std::atomic<bool> failure_claimed{false},failure_ready{false};
inline char first_failure[256]{};
inline void failure(const char* cause) noexcept{
    if(!failure_claimed.exchange(true,std::memory_order_acq_rel)){
        std::snprintf(first_failure,sizeof(first_failure),"%s",cause);
        failure_ready.store(true,std::memory_order_release);
    }
}
inline bool qualifies(uint32_t requested,uintptr_t caller,uint64_t thread,uintptr_t image) noexcept{
    return active&&active->thread&&thread==active->thread&&image&&caller==image+0x526ded&&requested==100;
}
inline uint64_t now() noexcept{
    return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
template<class F> void invoke(uint32_t requested,uintptr_t caller,uint64_t thread,uintptr_t image,F&& original,uint64_t(*clock)()=now){
    const bool chosen=qualifies(requested,caller,thread,image);
    const uint32_t duration=chosen?10:requested;
    auto& counter=chosen?selected:forwarded;
    counter.entered.fetch_add(1,std::memory_order_relaxed);
    counter.requested_ms.fetch_add(requested,std::memory_order_relaxed);
    counter.selected_ms.fetch_add(duration,std::memory_order_relaxed);
    const auto start=clock();
    try{original(duration);counter.returned.fetch_add(1,std::memory_order_relaxed);}
    catch(const std::exception& error){counter.elapsed_ns.fetch_add(clock()-start,std::memory_order_relaxed);counter.failed.fetch_add(1,std::memory_order_relaxed);failure(error.what());throw;}
    catch(...){counter.elapsed_ns.fetch_add(clock()-start,std::memory_order_relaxed);counter.failed.fetch_add(1,std::memory_order_relaxed);failure("unknown original SDL_Delay exception");throw;}
    counter.elapsed_ns.fetch_add(clock()-start,std::memory_order_relaxed);
}
}
