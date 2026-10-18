#pragma once
#include "native_routes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MT2FW_MT2_RELEASE_VERSION 1u
#define MT2FW_MT2_RELEASE_FORWARD 1u
#define MT2FW_MT2_RELEASE_SUPPRESS 2u
#define MT2FW_MT2_RELEASE_FAILED 3u

/* Private multiplayer extension for final release LevelUp capture. The immediate
   caller is the shared MessageTo Mode dispatcher, not celebration provenance.
   The consumer must prove its exact retained active celebration/action/current
   owners or return FORWARD for a known unrelated invocation. Uncertain release
   provenance requires FAILED. All pointers are borrowed only for the callback.
   native_command is the actual GNU string object, not a reconstructed value.
   native_return is captured in the physical Mode.Command detour itself. */
typedef struct MT2FW_MT2_ReleaseCall {
    uint32_t size;
    uint32_t readable;
    uint32_t read_error;
    uint32_t reserved;
    uint64_t invocation;
    uint64_t registration;
    uint64_t thread;
    void* receiver;
    const void* native_command;
    uintptr_t native_return;
    const char* text;
    size_t length;
} MT2FW_MT2_ReleaseCall;

typedef struct MT2FW_MT2_ReleaseDecision {
    uint32_t size;
    uint32_t disposition;
    char cause[256];
} MT2FW_MT2_ReleaseDecision;

/* Return OK and an explicit disposition. Only FORWARD permits the existing
   owner to continue; SUPPRESS skips the original. FAILED, malformed output or
   a thrown exception blocks this matching invocation and retains its cause.
   A failed registration stays blocked until explicitly unbound and rebound.
   The delegate must not call the original and then request FORWARD. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_ReleaseDelegate)(
    void* user,const MT2FW_MT2_ReleaseCall*,MT2FW_MT2_ReleaseDecision*);

typedef struct MT2FW_MT2_ReleaseReceipt {
    uint32_t size;
    uint32_t bound;
    uint32_t failed;
    uint32_t inflight;
    uint64_t registration;
    uint64_t last_invocation;
    uint32_t last_disposition;
    uint32_t reserved;
    char owner[MT2FW_MT2_ID_BYTES];
    char cause[256];
} MT2FW_MT2_ReleaseReceipt;

/* Does not add a hook or change OwnershipV1. One active exact native
   doodesch.multiplayer host owns the registration. Unbind is metadata-only;
   BUSY means its in-flight callback has not finished. A receipt records the
   delegate decision, never confirmation of the original game's effects. */
typedef struct MT2FW_MT2_ReleaseV1 {
    uint32_t size;
    uint32_t version;
    void* context;
    MT2FW_MT2_Result (MT2FW_CALL *bind)(void*,MT2FW_MT2_ReleaseDelegate,void* user,MT2FW_MT2_ReleaseReceipt*);
    MT2FW_MT2_Result (MT2FW_CALL *unbind)(void*,uint64_t registration,MT2FW_MT2_ReleaseReceipt*);
    MT2FW_MT2_Result (MT2FW_CALL *read)(void*,MT2FW_MT2_ReleaseReceipt*);
} MT2FW_MT2_ReleaseV1;

typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_QueryReleaseFn)(
    const MT2FW_Host*,uint32_t version,MT2FW_MT2_ReleaseV1*,uint32_t capacity);

#ifdef __cplusplus
}
#endif
