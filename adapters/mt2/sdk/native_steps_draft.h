#pragma once
/* Private Step draft. Query its exact version before using any sized struct. */
#include "native_patches.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MT2FW_MT2_STEP_DRAFT_VERSION 9u
#define MT2FW_MT2_STEP_FIXED_DT_BITS 0x3c800000u
#define MT2FW_MT2_STEP_UUID_BYTES 65u
#define MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS 1u

typedef enum MT2FW_MT2_StepPhase {
    MT2FW_MT2_STEP_PASSIVE=0,
    MT2FW_MT2_STEP_ARMED=1,
    MT2FW_MT2_STEP_LOADING=2,
    MT2FW_MT2_STEP_HELD=3,
    MT2FW_MT2_STEP_ENTERED=4,
    MT2FW_MT2_STEP_FAILED=5,
    MT2FW_MT2_STEP_RETIRING=6
} MT2FW_MT2_StepPhase;

typedef enum MT2FW_MT2_StepOutcome {
    MT2FW_MT2_STEP_NOT_ATTEMPTED=0,
    MT2FW_MT2_STEP_CONFIRMED=1,
    MT2FW_MT2_STEP_UNKNOWN=2
} MT2FW_MT2_StepOutcome;

typedef struct MT2FW_MT2_StepReceipt {
    uint32_t size;
    uint32_t phase;
    uint32_t outcome;
    uint32_t actual_role;
    uint64_t lease;
    uint64_t session;
    uint64_t world_epoch;
    uint64_t tick;
    uint64_t invocation;
    uint64_t completed_tick;
    uint64_t thread;
    uint32_t original_attempted;
    uint32_t original_returned;
    uint32_t timer_calls;
    uint32_t dt_bits;
    uint32_t workers_held;
    uint32_t callbacks_inflight;
    char cause[256];
    /* Retained Framework-owned lifecycle ACK. No pointer enters peer agreement. */
    uint32_t seed_attempted;
    uint32_t seed_returned;
    uint32_t bootstrap_held;
    /* Known coverage of the retained canonical bootstrap SHA. Zero before bootstrap. */
    uint32_t bootstrap_coverage;
    uint64_t generation;
    unsigned char initial_witness_sha256[32];
} MT2FW_MT2_StepReceipt;

typedef struct MT2FW_MT2_StepBoundary {
    uint32_t size;
    uint32_t actual_role;
    uint64_t lease;
    uint64_t session;
    uint64_t world_epoch;
    uint64_t tick;
    uint64_t invocation;
    uint64_t thread;
    uint32_t dt_bits;
    uint32_t reserved;
} MT2FW_MT2_StepBoundary;

/* Borrowed only inside this boundary callback. Exactly one attempt, on its
   bound thread and lease/epoch/tick. Never call again after an error/timeout. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_StepNext)(
    void* continuation,uint64_t invocation,MT2FW_MT2_StepReceipt*);

typedef enum MT2FW_MT2_StepDisposition {
    /* No command or original may have been attempted when returning HELD. */
    MT2FW_MT2_STEP_KEEP_HELD=0,
    MT2FW_MT2_STEP_COMPLETED=1,
    MT2FW_MT2_STEP_STOP_UNKNOWN=2
} MT2FW_MT2_StepDisposition;

typedef MT2FW_MT2_StepDisposition (MT2FW_CALL *MT2FW_MT2_StepBoundaryFn)(
    void* user,const MT2FW_MT2_StepBoundary*,MT2FW_MT2_StepNext,void* continuation);

/* Nonblocking transport/admission/status only. No legacy action-runtime pump,
   game commands, Lua/world updates or native scene draw from this callback.
   v3 ARMED service may perform its one guarded selected-load continuation only
   after the retained seed ACK; this runs outside an original Go.
   HELD service may perform one queued user Leave through the guarded original
   Quit call after real Controller-idle checks, before Next. Its native ACK
   returns RETIRING/NOT_ATTEMPTED with no new original; no other commands are
   permitted except the v6 single-use prepared snapshot capture callback below.
   v4 FAILED service also permits that local Quit after explicit
   preflight_exit and real Controller-idle proof; it never consumes a command or
   retries Next. Preserve the terminal cause through retirement. Defer Step
   unbind until teardown callbacks are no longer in flight. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_StepServiceFn)(
    void* user,const MT2FW_MT2_StepReceipt*);

typedef enum MT2FW_MT2_StepProofPhase {
    MT2FW_MT2_STEP_PROOF_UNAVAILABLE=0,
    MT2FW_MT2_STEP_PROOF_PRELOAD=1,
    MT2FW_MT2_STEP_PROOF_LOADING=2,
    MT2FW_MT2_STEP_PROOF_LOADED=3,
    MT2FW_MT2_STEP_PROOF_EXITED=4
} MT2FW_MT2_StepProofPhase;
typedef struct MT2FW_MT2_StepProofQuery {
    uint32_t size;
    uint32_t version;
    uint64_t session;
    uint64_t session_provider_token;
    uint64_t thread;
    /* UINT32_MAX reads metadata; another index reads one immutable Region row. */
    uint32_t region_rng_index;
    uint32_t reserved;
} MT2FW_MT2_StepProofQuery;
/* This is a copied, process-local proof from the retained native MP owner.
   It is not authority by itself: Framework rechecks owner, registered provider,
   native pointers and scheduler. Addresses are experimental native data only.
   UUID/digest/bytes/generation come from verified NativeOpen/normal_load receipts;
   session/membership/seed come from the actual agreed pre-load session owner.
   Never turn Config values, request echoes or fixture constants into a proof.
   No string pointer, STL object, exception or ownership crosses this boundary. */
typedef struct MT2FW_MT2_StepProofReply {
    uint32_t size;
    uint32_t phase;
    uint64_t session;
    uint64_t session_provider_token;
    uint64_t generation;
    uint64_t snapshot_bytes;
    uint64_t agreed_seed;
    uint64_t thread;
    uint64_t world_epoch;
    uint64_t game;
    uint64_t mode;
    uint64_t state;
    uint64_t world;
    uint64_t map;
    uint32_t seed_present;
    uint32_t frozen_peer_mask;
    /* v3: MP leaves both resume fields and initial_witness_sha256 zero.
       Framework owns Pause and the retained initial bootstrap certificate. */
    uint32_t resume_workers_known;
    uint32_t resume_workers;
    char snapshot_uuid[MT2FW_MT2_STEP_UUID_BYTES];
    unsigned char snapshot_sha256[32];
    unsigned char initial_witness_sha256[32];
    char cause[256];
    unsigned char reserved[7];
    /* v5 snapshot-bound root Region RNG input, copied before native load. */
    uint32_t region_rng_mode; /* 1 first import, 2 preserved snapshot state */
    uint32_t region_rng_count;
    uint32_t region_rng_index;
    int32_t region_rng_id;
    uint64_t region_rng_words[4];
    int32_t region_rng_extra;
    uint32_t region_rng_reserved;
    unsigned char region_rng_sha256[32];
} MT2FW_MT2_StepProofReply;
/* Synchronous, nonblocking and read-only. Fill exact sizes and return the
   original failure cause. Do not run loads, ticks, commands or retire callbacks
   from here. Callback/user storage must survive BUSY retirement. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_StepProofFn)(
    void* user,const MT2FW_MT2_StepProofQuery*,MT2FW_MT2_StepProofReply*);

enum {MT2FW_MT2_SAVE_ASYNC=1,MT2FW_MT2_SAVE_BLOCKING=2,MT2FW_MT2_SAVE_PERIODIC=3,MT2FW_MT2_SAVE_QUIT=4};
/* QUIT retains _BeginQuit intent before native side effects. option is the
   pinned native autosave condition; execution of the Save uses option=false. */
enum { MT2FW_MT2_EXIT_SAVE_BEGIN=1,MT2FW_MT2_EXIT_SAVE_COMPLETE=2,
       MT2FW_MT2_EXIT_SAVE_TERMINAL=3,MT2FW_MT2_EXIT_SAVE_RELOAD=4 };
typedef struct MT2FW_MT2_SaveRequest {
    uint32_t size,kind,option,reserved;
    uint64_t lease,session,world_epoch,tick,invocation,thread,mode,caller_rva;
    const char* filename;
    uint32_t filename_bytes,reserved1;
} MT2FW_MT2_SaveRequest;
/* v7: before original save preparation. Copy immutable request bytes before
   returning OK. This callback only retains an intent; it cannot save, tick,
   issue commands, or retire its owner. Async option is absolute-path mode;
   blocking option is hide_logo. The original filename is not normalized. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_SaveRequestFn)(
    void* user,const MT2FW_MT2_SaveRequest*,char* cause,uint32_t cause_capacity);

typedef struct MT2FW_MT2_StepSpec {
    uint32_t size;
    uint32_t dt_bits;
    uint64_t session;
    uint64_t session_provider_token;
    uint8_t frozen_peer_mask;
    uint8_t reserved[7];
    char snapshot_uuid[MT2FW_MT2_STEP_UUID_BYTES];
    MT2FW_MT2_StepBoundaryFn boundary;
    MT2FW_MT2_StepServiceFn service;
    void* user;
    MT2FW_MT2_StepProofFn proof;
    void* proof_user;
    MT2FW_MT2_SaveRequestFn save_request;
} MT2FW_MT2_StepSpec;

typedef struct MT2FW_MT2_StepActivation {
    uint32_t size;
    uint32_t reserved;
    uint64_t lease;
    uint64_t session;
    uint64_t world_epoch;
    uint64_t first_tick;
    char snapshot_uuid[MT2FW_MT2_STEP_UUID_BYTES];
    /* Exact local bootstrap witness from Framework StepReceipt.
       Peer agreement is verified by the existing Multiplayer controller. */
    unsigned char initial_witness_sha256[32];
} MT2FW_MT2_StepActivation;

/* A snapshot is prepared by an ordered command, then held until peer agreement.
   ARM borrows the current boundary before Next. SEAL, CAPTURE and RELEASE
   require the held service at exactly the prepared completed tick. */
enum { MT2FW_MT2_SNAPSHOT_ARM=1, MT2FW_MT2_SNAPSHOT_SEAL=2,
       MT2FW_MT2_SNAPSHOT_CAPTURE=3, MT2FW_MT2_SNAPSHOT_RELEASE=4, MT2FW_MT2_SNAPSHOT_PUMP=5, MT2FW_MT2_SNAPSHOT_CLIENT_CACHE=6 };
typedef struct MT2FW_MT2_SnapshotRequest {
    uint32_t size,operation;
    uint64_t lease,session,world_epoch,tick,invocation,order;
    char nonce[37];
    uint8_t peer_mask,ack_mask,reserved;
    unsigned char witness_sha256[32];
} MT2FW_MT2_SnapshotRequest;
/* Owner-pinned callback. Preserve the original failure cause.
   Blocking Save completes synchronously. Original async Save may retain its
   native tasks; PUMP advances only those tasks from the exact held service.
   Failed sessions may PUMP to safe native retirement without clearing failure.
   No second Save or Next is permitted. Verify covered state after actual async
   completion and before file acknowledgment or RELEASE.
   The certificate is consumed before entry, including a failed callback.
   Blocking capture verifies same-call covered witness neutrality. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_SnapshotCaptureFn)(void*,char*,uint32_t);

typedef struct MT2FW_MT2_StepServiceDraftV1 {
    uint32_t size;
    uint32_t version;
    void* context;
    /* Bind arms initialization at the next outer-frame entry, after the current
       Go returns. Framework initializes the real agreed seed once. Start the guarded native load
       only after seed_returned=1 and matching generation. Never replay unknown
       initialization. Bind never runs a game load or reseeds a loaded world. */
    MT2FW_MT2_Result (MT2FW_CALL *bind)(void*,const MT2FW_MT2_StepSpec*,MT2FW_MT2_StepReceipt*);
    /* Require current native bootstrap proof and matching immutable context. */
    MT2FW_MT2_Result (MT2FW_CALL *activate)(void*,const MT2FW_MT2_StepActivation*,MT2FW_MT2_StepReceipt*);
    /* Retained local receipt; no native calls, tick replay or state repair. */
    MT2FW_MT2_Result (MT2FW_CALL *read)(void*,uint64_t lease,MT2FW_MT2_StepReceipt*);
    /* BUSY requires callback storage to remain alive. No catch-up or silent
       return to free-running simulation after failed/unknown execution. */
    MT2FW_MT2_Result (MT2FW_CALL *unbind)(void*,uint64_t lease,MT2FW_MT2_StepReceipt*);
    /* Read-only admission for local Quit from the current held/failed service.
       Exact lease, owner and thread; no task draining or failure reset. A fresh
       successful check is required before MP retirement/save-companion changes.
       Original Quit repeats native admission. The returned receipt stays local. */
    MT2FW_MT2_Result (MT2FW_CALL *preflight_exit)(void*,uint64_t lease,MT2FW_MT2_StepReceipt*);
    /* v6: only the bound owner can arm preparation or consume its capture.
       RELEASE requires complete file and companion acknowledgment on Host. */
    MT2FW_MT2_Result (MT2FW_CALL *snapshot)(void*,const MT2FW_MT2_SnapshotRequest*,
        MT2FW_MT2_SnapshotCaptureFn,void*,MT2FW_MT2_StepReceipt*);
    /* v9: retain healthy Quit, complete its matching local Save publication, or
       explicitly retain the existing terminal/reload native teardown scope.
       This never executes original Quit or changes native flags. */
    MT2FW_MT2_Result (MT2FW_CALL *exit_save)(void*,uint64_t lease,uint32_t operation,
        const char* nonce,MT2FW_MT2_StepReceipt*);
} MT2FW_MT2_StepServiceDraftV1;

/* Proposed separate export; query using the exact borrowed owner Host from an
   active Framework callback. Do not probe another loader when it is absent. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_QueryStepServiceDraftFn)(
    const MT2FW_Host*,uint32_t,MT2FW_MT2_StepServiceDraftV1*,uint32_t capacity);

#ifdef __cplusplus
}
#endif
