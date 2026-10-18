#pragma once
/* Experimental MT2-only service. This is separate from the stable plugin ABI. */
#include "mt2fw/plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MT2FW_MT2_SERVICE_VERSION 1u
#define MT2FW_MT2_NAME_BYTES 96u
#define MT2FW_MT2_ID_BYTES 160u

typedef enum MT2FW_MT2_Result {
    MT2FW_MT2_OK = 0,
    MT2FW_MT2_INVALID = 1,
    MT2FW_MT2_UNAVAILABLE = 2,
    MT2FW_MT2_BUSY = 3,
    MT2FW_MT2_FAILED = 4
} MT2FW_MT2_Result;

typedef struct MT2FW_MT2_Frame {
    uint32_t size;
    uint32_t original_returned;
    uint64_t id;
    void* game;
    float dt;
    uint32_t reserved;
} MT2FW_MT2_Frame;
/* Return 1 to enter the pair, 0 to skip it. A successful begin receives exactly
   one end, including when its owner retires during original Update. End runs in
   reverse begin order. Callbacks must not throw, block or retain the frame. */
typedef int (MT2FW_CALL *MT2FW_MT2_FrameBegin)(void* user, const MT2FW_MT2_Frame* frame);
typedef void (MT2FW_CALL *MT2FW_MT2_FrameEnd)(void* user, const MT2FW_MT2_Frame* frame);

typedef enum MT2FW_MT2_TraceKind {
    MT2FW_MT2_SCOPE_BEGIN = 1,
    MT2FW_MT2_SCOPE_END = 2,
    MT2FW_MT2_FRAME_BEGIN = 3,
    MT2FW_MT2_FRAME_END = 4
} MT2FW_MT2_TraceKind;
typedef struct MT2FW_MT2_Scope {
    uint64_t epoch;
    uint64_t token;
} MT2FW_MT2_Scope;
typedef struct MT2FW_MT2_TraceEvent {
    uint64_t epoch;
    uint64_t sequence;
    uint64_t nanoseconds;
    uint64_t thread;
    uint64_t token;
    uint64_t frame;
    uint32_t section;
    uint32_t kind;
} MT2FW_MT2_TraceEvent;
typedef struct MT2FW_MT2_TraceStatus {
    uint32_t size;
    uint32_t running;
    uint64_t epoch;
    uint64_t dropped;
    uint64_t queued;
    uint32_t written;
    uint32_t thread_cpu_available;
} MT2FW_MT2_TraceStatus;
typedef struct MT2FW_MT2_Section {
    uint32_t size;
    uint32_t id;
    char owner[MT2FW_MT2_ID_BYTES];
    char name[MT2FW_MT2_NAME_BYTES];
} MT2FW_MT2_Section;

typedef struct MT2FW_MT2_ServiceV1 {
    uint32_t size;
    uint32_t version;
    void* context;
    /* Borrowed UTF-8 paths, valid until PluginShutdown returns. Game data is the
       loader's isolated PhysFS root. Mod data belongs to this plugin only. */
    const char* game_directory;
    const char* profile_directory;
    const char* game_data_directory;
    const char* mod_directory;
    const char* mod_data_directory;
    uint64_t (MT2FW_CALL *frame_subscribe)(void*, MT2FW_MT2_FrameBegin, MT2FW_MT2_FrameEnd, void* user);
    MT2FW_MT2_Result (MT2FW_CALL *frame_unsubscribe)(void*, uint64_t token);
    /* Names resolve through the verified adapter catalog. Reserved/shared or
       unknown targets reject. No RVA or executable address comes from a mod.
       Native detour storage must live for the session. No live DLL unloading. */
    MT2FW_MT2_Result (MT2FW_CALL *hook_install)(void*, const char* target, void* detour, void** original, uint64_t* token);
    MT2FW_MT2_Result (MT2FW_CALL *hook_remove)(void*, uint64_t token);
    uint32_t (MT2FW_CALL *section_register)(void*, const char* name);
    MT2FW_MT2_Scope (MT2FW_CALL *scope_begin)(void*, uint32_t section);
    void (MT2FW_CALL *scope_end)(void*, uint32_t section, MT2FW_MT2_Scope scope);
    /* One capture owner at a time. Capacity is fixed at start; no per-event
       allocation. Only that owner may drain/stop. Data is wall time, not CPU.
       A changed dropped counter invalidates exclusive/complete interval claims. */
    MT2FW_MT2_Result (MT2FW_CALL *capture_start)(void*, uint32_t capacity, uint64_t* epoch);
    MT2FW_MT2_Result (MT2FW_CALL *capture_read)(void*, MT2FW_MT2_TraceEvent* events, uint32_t capacity, MT2FW_MT2_TraceStatus* status);
    MT2FW_MT2_Result (MT2FW_CALL *capture_stop)(void*);
    MT2FW_MT2_Result (MT2FW_CALL *section_read)(void*, uint32_t section, MT2FW_MT2_Section* result);
} MT2FW_MT2_ServiceV1;

/* Resolve MT2FW_MT2_QueryService on the already loaded MT2Framework.dll. Query
   from an active framework callback, with the exact borrowed Host pointer from
   PluginLoad. An absent export means unsupported, never start another loader.
   All calls except capture_read/section_read and scope emission are restricted
   to framework callbacks. No exception may cross this boundary. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_QueryServiceFn)(
    const MT2FW_Host* owner, uint32_t version, MT2FW_MT2_ServiceV1* output, uint32_t capacity);

#ifdef __cplusplus
}
#endif
