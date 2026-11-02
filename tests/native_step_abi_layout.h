#pragma once
#include <stddef.h>
#include "native_steps_draft.h"
#include "native_step_recovery_draft.h"
#ifdef __cplusplus
#define MT2FW_ABI_ASSERT static_assert
#else
#define MT2FW_ABI_ASSERT _Static_assert
#endif
MT2FW_ABI_ASSERT(MT2FW_MT2_STEP_DRAFT_VERSION == 9u, "Step draft must match the consumer");
MT2FW_ABI_ASSERT(MT2FW_ABI_VERSION == 1u, "Stable native plugin ABI differs");
MT2FW_ABI_ASSERT(MT2FW_MT2_PATCHES_VERSION == 1u, "Patch protocol must remain compatible");
MT2FW_ABI_ASSERT(sizeof(void*) == 8, "Windows x64 is required");
MT2FW_ABI_ASSERT(sizeof(MT2FW_MT2_StepServiceDraftV1) == 72, "Step service draft 9 size differs");
MT2FW_ABI_ASSERT(offsetof(MT2FW_MT2_StepServiceDraftV1, exit_save) == 64, "Step exit-save slot differs");
MT2FW_ABI_ASSERT(sizeof(MT2FW_MT2_StepSpec) == 152, "Step specification layout differs");
MT2FW_ABI_ASSERT(sizeof(MT2FW_MT2_SnapshotRequest) == 128, "Snapshot request layout differs");
MT2FW_ABI_ASSERT(offsetof(MT2FW_MT2_SnapshotRequest, nonce) == 56, "Snapshot nonce offset differs");
MT2FW_ABI_ASSERT(offsetof(MT2FW_MT2_SnapshotRequest, witness_sha256) == 96, "Snapshot witness offset differs");
MT2FW_ABI_ASSERT(MT2FW_MT2_SNAPSHOT_ARM == 1 && MT2FW_MT2_SNAPSHOT_CLIENT_CACHE == 6 && MT2FW_MT2_SNAPSHOT_ARM_PERIODIC == 7,
                 "Snapshot operation values differ");
MT2FW_ABI_ASSERT(sizeof(MT2FW_MT2_QueryStepServiceDraftFn) == sizeof(void*), "Step query function pointer differs");
MT2FW_ABI_ASSERT(sizeof(MT2FW_MT2_RecoveryReceipt) == 720, "Recovery receipt layout differs");
MT2FW_ABI_ASSERT(sizeof(MT2FW_MT2_RecoverySpec) == 136, "Recovery spec layout differs");
MT2FW_ABI_ASSERT(sizeof(MT2FW_MT2_RecoveryRequest) == 216, "Recovery request layout differs");
MT2FW_ABI_ASSERT(sizeof(MT2FW_MT2_RecoveryPublication) == 88, "Recovery publication layout differs");
MT2FW_ABI_ASSERT(sizeof(MT2FW_MT2_StepRecoveryDraftV1) == 40, "Recovery query table layout differs");
