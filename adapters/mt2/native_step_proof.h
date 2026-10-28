#pragma once
#include "native_step_driver.h"

namespace mt2fw::mt2 {
// The adapter replaces the callback with its owner-pinned thunk before this read.
StepNativeObservation read_step_proof(const MT2FW_Host*,const MT2FW_MT2_StepSpec&,uint64_t thread);
}
