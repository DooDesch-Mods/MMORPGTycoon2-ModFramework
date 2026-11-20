#pragma once
#include <cstdint>
#include <stdexcept>

namespace mt2fw::mt2::step_pins {
inline constexpr uint64_t go_rva=0x95f890,timer_rva=0xa413b0,timer_return_rva=0x95f8bf;
inline constexpr uint32_t go_bytes=336,timer_bytes=0x340;
inline constexpr const char* go_sha256="b4829af24a4528aac26889a49cf6bafb7353dabacba0ac1b8ef5b521ab4bf298";
inline constexpr const char* timer_sha256="8cdf76ec96c73b9c4745d27f7329f80f90cc33e7ebd2a3a862105d2994ae02f5";
// Call before creating either physical hook. Installation and live ownership
// readback remain the Framework adapter driver's responsibility.
inline bool select_step_owner(bool lab_requested,bool step_requested){
    if(lab_requested&&step_requested)throw std::runtime_error("Private lab and Step service cannot own Go/Timer together");
    return step_requested;
}
}
