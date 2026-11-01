#pragma once
#include "native_steps_draft.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MT2FW_MT2_STEP_RECOVERY_DRAFT_VERSION 1u
enum { MT2FW_MT2_RECOVERY_WATCHING=1, MT2FW_MT2_RECOVERY_INTERRUPTED=2,
       MT2FW_MT2_RECOVERY_PREPARING=3, MT2FW_MT2_RECOVERY_PREPARED=4,
       MT2FW_MT2_RECOVERY_SEALED=5, MT2FW_MT2_RECOVERY_CONSUMED=6,
       MT2FW_MT2_RECOVERY_CAPTURED=7, MT2FW_MT2_RECOVERY_PUBLISHED=8,
       MT2FW_MT2_RECOVERY_RETIRE_READY=9, MT2FW_MT2_RECOVERY_FAILED=10 };
enum { MT2FW_MT2_COHORT_HEALTHY=1, MT2FW_MT2_COHORT_INTERRUPTED=2 };
/* Physical account IDs and process nonces are separate. Slot i is bit 1<<i.
   Zero both fields for unused slots. Freeze these after the real handshake. */
typedef struct MT2FW_MT2_RecoveryCohort { uint64_t physical_ids[4],process_nonces[4]; } MT2FW_MT2_RecoveryCohort;
typedef struct MT2FW_MT2_RecoveryReceipt {
    uint32_t size,phase;
    uint64_t lease,session,world_epoch,generation,thread;
    uint64_t interruption,completed_tick,next_tick,local_process;
    uint32_t local_slot,original_mask,survivor_mask,workers_held;
    uint32_t prepare_attempted,prepare_returned,capture_attempted,capture_returned;
    uint32_t selected_clock_bits,reserved;
    MT2FW_MT2_RecoveryCohort original,observed,survivors;
    char recovery_nonce[37];
    unsigned char witness_sha256[32];
    uint64_t file_bytes,companion_bytes;
    unsigned char file_sha256[32],companion_sha256[32];
    char cause[256];
} MT2FW_MT2_RecoveryReceipt;
typedef struct MT2FW_MT2_RecoveryObservation {
    uint32_t size,disposition;
    MT2FW_MT2_RecoveryCohort observed;
    char cause[256];
} MT2FW_MT2_RecoveryObservation;
/* Transport only. No native mutation, ordinary controller tick, Save or Next.
   Called before normal Step membership proof. Return actual observed nonces. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_RecoveryObserveFn)(void*,const MT2FW_MT2_RecoveryReceipt*,MT2FW_MT2_RecoveryObservation*);
/* Separate read scope: retained NativeOpen/selected file identity and provider,
   bypassing only live membership liveness. Never use for normal Step proof. */
typedef MT2FW_MT2_StepProofFn MT2FW_MT2_RecoveryLocalProofFn;
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_RecoveryServiceFn)(void*,const MT2FW_MT2_RecoveryReceipt*);
/* Full covered Host observation after preparation and before/after capture.
   Read only; preserve the original cause and do not echo request digests. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_RecoveryWitnessFn)(void*,const MT2FW_MT2_RecoveryReceipt*,unsigned char sha256[32],char*,uint32_t);
typedef struct MT2FW_MT2_RecoveryPublication {
    uint32_t size,reserved;
    uint64_t file_bytes,companion_bytes;
    unsigned char file_sha256[32],companion_sha256[32];
} MT2FW_MT2_RecoveryPublication;
/* Re-read the completed full file and companion. OK means exact bytes/hashes
   and durable publication, not an old-session peer acknowledgement. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_RecoveryPublicationFn)(void*,const MT2FW_MT2_RecoveryReceipt*,const MT2FW_MT2_RecoveryPublication*,char*,uint32_t);
typedef struct MT2FW_MT2_RecoverySpec {
    uint32_t size,local_slot;
    uint64_t lease,session_provider_token;
    MT2FW_MT2_RecoveryCohort original;
    MT2FW_MT2_RecoveryObserveFn observe;
    MT2FW_MT2_RecoveryLocalProofFn local_proof;
    MT2FW_MT2_RecoveryServiceFn service;
    MT2FW_MT2_RecoveryWitnessFn witness;
    MT2FW_MT2_RecoveryPublicationFn publication;
    void* user;
} MT2FW_MT2_RecoverySpec;
enum { MT2FW_MT2_RECOVERY_BEGIN=1, MT2FW_MT2_RECOVERY_PREPARE=2,
       MT2FW_MT2_RECOVERY_SEAL=3, MT2FW_MT2_RECOVERY_CAPTURE=4,
       MT2FW_MT2_RECOVERY_PUMP=5, MT2FW_MT2_RECOVERY_PUBLISH=6,
       MT2FW_MT2_RECOVERY_PREFLIGHT_RETIRE=7 };
typedef struct MT2FW_MT2_RecoveryRequest {
    uint32_t size,operation;
    uint64_t lease,interruption;
    char recovery_nonce[37];
    uint8_t survivor_mask,reserved[2];
    MT2FW_MT2_RecoveryCohort survivors;
    MT2FW_MT2_RecoveryPublication publication;
} MT2FW_MT2_RecoveryRequest;
typedef struct MT2FW_MT2_StepRecoveryDraftV1 {
    uint32_t size,version;
    void* context;
    MT2FW_MT2_Result (MT2FW_CALL *bind)(void*,const MT2FW_MT2_RecoverySpec*,MT2FW_MT2_RecoveryReceipt*);
    MT2FW_MT2_Result (MT2FW_CALL *read)(void*,uint64_t lease,MT2FW_MT2_RecoveryReceipt*);
    /* Called only from the retained recovery service. CAPTURE alone accepts a
       callback. Failure consumes native attempts; never replay an unknown Save.
       PREFLIGHT_RETIRE grants only original Quit admission, not a retirement ACK.
       Observe actual Title destruction through the existing Step exit path. */
    MT2FW_MT2_Result (MT2FW_CALL *operation)(void*,const MT2FW_MT2_RecoveryRequest*,MT2FW_MT2_SnapshotCaptureFn,void*,MT2FW_MT2_RecoveryReceipt*);
} MT2FW_MT2_StepRecoveryDraftV1;
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_QueryStepRecoveryDraftFn)(const MT2FW_Host*,uint32_t,MT2FW_MT2_StepRecoveryDraftV1*,uint32_t);
#ifdef __cplusplus
}
#endif
