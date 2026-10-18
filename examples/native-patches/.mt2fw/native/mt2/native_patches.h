#pragma once
/* Experimental typed patch extension. Stable plugin ABI 1 is unchanged. */
#include "native_services.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MT2FW_MT2_PATCHES_VERSION 1u
#define MT2FW_MT2_PATCH_ID_BYTES 160u
#define MT2FW_MT2_PATCH_MAX_ORDER 16u

typedef enum MT2FW_MT2_PatchTarget {
    MT2FW_MT2_PATCH_GAME_UPDATE = 1,
    MT2FW_MT2_PATCH_OBJECTS_CREATE = 2,
    /* Framework request boundary only, not a native clock detour. */
    MT2FW_MT2_PATCH_API_TIME_SET = 3
} MT2FW_MT2_PatchTarget;
typedef enum MT2FW_MT2_PatchPhase {
    MT2FW_MT2_PATCH_BEFORE = 1,
    MT2FW_MT2_PATCH_AFTER = 2
} MT2FW_MT2_PatchPhase;
typedef enum MT2FW_MT2_PatchMode {
    MT2FW_MT2_PATCH_OBSERVE = 0,
    MT2FW_MT2_PATCH_MODIFY = 1
} MT2FW_MT2_PatchMode;

/* Copied scalar data only. No pointer is a game address or object handle.
   Payload storage is borrowed for the callback. Native code is not sandboxed. */
typedef struct MT2FW_MT2_PatchEvent {
    uint32_t size;
    uint32_t target;
    uint32_t phase;
    uint32_t original_returned;
    uint64_t invocation;
    uint64_t generation;
    uint64_t epoch;
    uint64_t thread;
    float dt;
    uint32_t create_flag;
    uint32_t result_present;
    uint32_t paused;
    float speed;
    uint32_t reserved;
} MT2FW_MT2_PatchEvent;

/* Only api.time.set MODIFY before may replace these request arguments.
   Initialize size and set replace_arguments=1 to submit both values. */
typedef struct MT2FW_MT2_PatchEdit {
    uint32_t size;
    uint32_t replace_arguments;
    uint32_t paused;
    float speed;
} MT2FW_MT2_PatchEdit;
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_PatchBefore)(
    void* user,const MT2FW_MT2_PatchEvent*,MT2FW_MT2_PatchEdit*);
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_PatchAfter)(
    void* user,const MT2FW_MT2_PatchEvent*);

typedef struct MT2FW_MT2_PatchSpec {
    uint32_t size;
    uint32_t target;
    uint32_t mode;
    int32_t priority;
    const char* id;
    /* References are owner_manifest_id:patch_id. Arrays and strings are copied
       during registration; counts may not exceed PATCH_MAX_ORDER. */
    const char* const* before;
    uint32_t before_count;
    const char* const* after;
    uint32_t after_count;
    MT2FW_MT2_PatchBefore on_before;
    MT2FW_MT2_PatchAfter on_after;
    void* user;
} MT2FW_MT2_PatchSpec;

typedef enum MT2FW_MT2_SessionRole {
    MT2FW_MT2_SESSION_UNKNOWN = 0,
    MT2FW_MT2_SESSION_STANDALONE = 1,
    MT2FW_MT2_SESSION_HOST = 2,
    MT2FW_MT2_SESSION_CLIENT = 3
} MT2FW_MT2_SessionRole;
typedef struct MT2FW_MT2_SessionQuery {
    uint32_t size;
    uint32_t reserved;
    uint64_t epoch;
    uint64_t thread;
} MT2FW_MT2_SessionQuery;
typedef struct MT2FW_MT2_SessionReply {
    uint32_t size;
    uint32_t role;
    /* Must echo query epoch. A stale acknowledgement is unknown authority. */
    uint64_t epoch;
} MT2FW_MT2_SessionReply;
/* The supported active native Multiplayer bridge owns this callback. It is
   queried synchronously for each mutation request. Return errors remain errors.
   Do not throw, block or retain query/reply storage. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_SessionProvider)(
    void* user,const MT2FW_MT2_SessionQuery*,MT2FW_MT2_SessionReply*);

typedef struct MT2FW_MT2_PatchReceipt {
    uint32_t size;
    uint32_t result;
    char cause[256];
} MT2FW_MT2_PatchReceipt;
typedef struct MT2FW_MT2_PatchesV1 {
    uint32_t size;
    uint32_t version;
    void* context;
    MT2FW_MT2_Result (MT2FW_CALL *register_patch)(void*,const MT2FW_MT2_PatchSpec*,MT2FW_MT2_PatchReceipt*);
    MT2FW_MT2_Result (MT2FW_CALL *unregister_patch)(void*,const char* id,MT2FW_MT2_PatchReceipt*);
    MT2FW_MT2_Result (MT2FW_CALL *bind_session_provider)(void*,MT2FW_MT2_SessionProvider,void* user,uint64_t* token,MT2FW_MT2_PatchReceipt*);
    MT2FW_MT2_Result (MT2FW_CALL *unbind_session_provider)(void*,uint64_t token,MT2FW_MT2_PatchReceipt*);
} MT2FW_MT2_PatchesV1;

/* Query in an active Framework callback using the exact borrowed Host pointer
   from PluginLoad. All calls belong on the bound game thread. Registrations are
   owner-scoped; native and Lua patches share ordering. BUSY retirement requires
   retaining callbacks/user data until confirmed quiescence. No exception or STL
   object crosses this boundary. Resolve this export from loaded MT2Framework. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_QueryPatchesFn)(
    const MT2FW_Host*,uint32_t version,MT2FW_MT2_PatchesV1*,uint32_t capacity);

#ifdef __cplusplus
}
#endif
