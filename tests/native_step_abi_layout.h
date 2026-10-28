#pragma once
#include <stddef.h>
#include "native_steps_draft.h"
#ifdef __cplusplus
#define MT2FW_ABI_ASSERT static_assert
#else
#define MT2FW_ABI_ASSERT _Static_assert
#endif
MT2FW_ABI_ASSERT(MT2FW_MT2_STEP_DRAFT_VERSION == 9u, "Step draft must match the consumer");
MT2FW_ABI_ASSERT(MT2FW_MT2_PATCHES_VERSION == 1u, "Patch protocol must remain compatible");
MT2FW_ABI_ASSERT(sizeof(void*) == 8, "Windows x64 is required");
MT2FW_ABI_ASSERT(sizeof(MT2FW_MT2_StepServiceDraftV1) == 72, "Step service draft 9 size differs");
MT2FW_ABI_ASSERT(offsetof(MT2FW_MT2_StepServiceDraftV1, exit_save) == 64, "Step exit-save slot differs");
MT2FW_ABI_ASSERT(sizeof(MT2FW_MT2_StepSpec) == 152, "Step specification layout differs");
MT2FW_ABI_ASSERT(sizeof(MT2FW_MT2_QueryStepServiceDraftFn) == sizeof(void*), "Step query function pointer differs");
