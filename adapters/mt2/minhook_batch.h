#pragma once
#include <MinHook.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Private MinHook extension. Uses one freeze for exactly these prepared slots.
   Does not consume another caller's queue. Cancels residual queued intent on
   failure without undoing or replaying an applied code patch. */
MH_STATUS WINAPI MT2FW_MH_EnableBatch(LPVOID const* targets,UINT count,UINT* failed_index,const char** phase);
#ifdef __cplusplus
}
#endif
