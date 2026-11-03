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

/* Fixed Character preview selection. V1 and the stable plugin ABI are unchanged. */
#define MT2FW_MT2_PREVIEW_SERVICE_VERSION 2u
#define MT2FW_MT2_PREVIEW_RANDOM_BYTES 40u
#define MT2FW_MT2_CHARACTER_PREVIEW_ABORT 0xE04D5432u

typedef struct MT2FW_MT2_CharacterPreviewIntCall {
    uint32_t size;
    uint32_t reserved;
    uint64_t return_rva;
    uint64_t thread;
    void* receiver;
    int32_t limit;
    uint32_t reserved2;
} MT2FW_MT2_CharacterPreviewIntCall;

typedef struct MT2FW_MT2_CharacterPreviewIntSelection {
    uint32_t size;
    uint32_t source_bytes;
    void* source;
    char cause[160];
} MT2FW_MT2_CharacterPreviewIntSelection;

/* UNAVAILABLE alone means no matching scope. OK borrows an initialized, writable
   private 40-byte source until the original GetInt and its trace finish. Other
   results, exceptions and invalid OK output abort before any original call with
   MT2FW_MT2_CHARACTER_PREVIEW_ABORT. The consumer must latch this ingress failure
   and refuse further connected preview originals until proper session teardown.
   A callback must not throw, reenter selection or retain the borrowed call. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_CharacterPreviewIntSelector)(
    void* user, const MT2FW_MT2_CharacterPreviewIntCall* call,
    MT2FW_MT2_CharacterPreviewIntSelection* selection);

typedef struct MT2FW_MT2_ServiceV2 {
    MT2FW_MT2_ServiceV1 v1;
    /* One supported Multiplayer registration, bound game thread only. A second
       bind is BUSY. Unbind prevents new selection; BUSY retains any in-flight
       lease until original return/unwind. Plugin retirement unregisters it. */
    MT2FW_MT2_Result (MT2FW_CALL *character_preview_int_bind)(
        void* context, MT2FW_MT2_CharacterPreviewIntSelector selector,
        void* user, uint64_t* token);
    MT2FW_MT2_Result (MT2FW_CALL *character_preview_int_unbind)(void* context, uint64_t token);
} MT2FW_MT2_ServiceV2;

/* Resolve MT2FW_MT2_QueryService on the already loaded MT2Framework.dll. Query
   from an active framework callback, with the exact borrowed Host pointer from
   PluginLoad. An absent export means unsupported, never start another loader.
   Query V2 with &service.v1 and sizeof(service); the returned prefix contains
   version 2 and the full V2 size. Public Runtime 0.5.5 does not provide V2.
   All calls except capture_read/section_read and scope emission are restricted
   to framework callbacks. No exception may cross this boundary. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_QueryServiceFn)(
    const MT2FW_Host* owner, uint32_t version, MT2FW_MT2_ServiceV1* output, uint32_t capacity);

#ifdef __cplusplus
}
#endif
