#pragma once
#include <cstdint>
#include <stdexcept>

namespace mt2fw::mt2::step_pins {
inline constexpr uint64_t go_rva=0x956730,timer_rva=0xa36300,timer_return_rva=0x95675f;
inline constexpr uint32_t go_bytes=336,timer_bytes=0x340;
inline constexpr const char* executable_sha256="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
inline constexpr const char* go_sha256="dafb71c7578812f1264c7121494d5ceff01d38931467babd20942bbec69eb994";
inline constexpr const char* timer_sha256="2442e3fc5884b09357a378bdeed30fa487466e4d69c36c864254857dc030c334";
// Call before creating either physical hook. Installation and live ownership
// readback remain the Framework adapter driver's responsibility.
inline bool select_step_owner(bool lab_requested,bool step_requested){
    if(lab_requested&&step_requested)throw std::runtime_error("Private lab and Step service cannot own Go/Timer together");
    return step_requested;
}
}
