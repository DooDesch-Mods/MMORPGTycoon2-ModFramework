#pragma once
#include "native_routes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MT2FW_MT2_OWNERSHIP_VERSION 1u
#define MT2FW_MT2_ROUTE_DISABLED_RETAINED 5u
#define MT2FW_MT2_EVENT_COPY_BYTES 256u
typedef void (MT2FW_CALL *MT2FW_MT2_FreeFn)(void* allocation);
typedef struct MT2FW_MT2_CommandCall {
    uint32_t size;
    uint32_t readable;
    uint32_t read_error;
    uint32_t reserved;
    uint64_t invocation;
    uint64_t thread;
    void* receiver;
    const char* text;
    size_t length;
} MT2FW_MT2_CommandCall;
/* Passive observation of the original gameplay Mode.Command entry. Text is
   borrowed only for this callback. It cannot replace arguments or suppress it. */
typedef void (MT2FW_CALL *MT2FW_MT2_CommandObserver)(void*,const MT2FW_MT2_CommandCall*);
typedef struct MT2FW_MT2_PolledEvent {
    uint32_t size;
    uint32_t event_bytes;
    uint32_t sdl_version;
    uint32_t reserved;
    uint64_t invocation;
    uint64_t thread;
    const void* event;
} MT2FW_MT2_PolledEvent;
/* A borrowed copy of the accepted SDL_Event, after provider consumption. It
   cannot filter or alter the event returned to the game. Pointer payloads are
   borrowed only for this callback; validate byte size/version before decoding. */
typedef void (MT2FW_CALL *MT2FW_MT2_InputObserver)(void*,const MT2FW_MT2_PolledEvent*);
typedef struct MT2FW_MT2_ModuleProof {
    uintptr_t base;
    uint64_t bytes;
    uint32_t pinned;
    uint32_t version_present;
    uint32_t version[4];
    uint32_t version_error;
    char sha256[65];
    char path[1024];
} MT2FW_MT2_ModuleProof;
typedef struct MT2FW_MT2_ExportProof {
    uintptr_t resolved;
    uintptr_t owner;
    uint32_t export_rva;
    uint32_t forwarded;
    char forwarder[256];
} MT2FW_MT2_ExportProof;
typedef struct MT2FW_MT2_FreeReceipt {
    uint32_t size;
    uint32_t state;
    uint64_t generation;
    uintptr_t slot;
    uintptr_t current;
    uintptr_t original;
    uintptr_t replacement;
    uintptr_t malloc_slot;
    uintptr_t malloc_original;
    uint32_t exchange_attempted;
    uint32_t exchanged;
    uint32_t protection_restored;
    uint32_t initial_protection;
    uint32_t current_protection;
    uint32_t live_confirmed;
    uint32_t win32_error;
    uint32_t seh_code;
    MT2FW_MT2_ModuleProof imported_crt;
    MT2FW_MT2_ModuleProof free_provider;
    MT2FW_MT2_ModuleProof malloc_provider;
    MT2FW_MT2_ExportProof free_export;
    MT2FW_MT2_ExportProof malloc_export;
    char owner[MT2FW_MT2_ID_BYTES];
    char game_sha256[65];
    char cause[256];
} MT2FW_MT2_FreeReceipt;

/* Independent private extension. Neither previous service header is enlarged.
   All controls run in Framework callbacks. The mod publishes its existing
   disabled observer with receipt.original before enabling its physical routes.
   There is no additional per-free dispatcher: the IAT points to that observer.
   Originals, provider modules and retained owner state live until process exit. */
typedef struct MT2FW_MT2_OwnershipV1 {
    uint32_t size;
    uint32_t version;
    void* context;
    MT2FW_MT2_Result (MT2FW_CALL *hook_disable)(void*,const char* name,MT2FW_MT2_RouteReceipt*);
    MT2FW_MT2_Result (MT2FW_CALL *hook_resume)(void*,const char* name,MT2FW_MT2_RouteReceipt*);
    MT2FW_MT2_Result (MT2FW_CALL *free_prepare)(void*,MT2FW_MT2_FreeFn replacement,MT2FW_MT2_FreeReceipt*);
    MT2FW_MT2_Result (MT2FW_CALL *free_enable)(void*,MT2FW_MT2_FreeReceipt*);
    MT2FW_MT2_Result (MT2FW_CALL *free_disable)(void*,MT2FW_MT2_FreeReceipt*);
    MT2FW_MT2_Result (MT2FW_CALL *free_read)(void*,MT2FW_MT2_FreeReceipt*);
    /* Remove is metadata retirement after a prepared or confirmed disabled
       import. It cannot free pinned modules or an enabled/unknown original. */
    MT2FW_MT2_Result (MT2FW_CALL *free_remove)(void*,MT2FW_MT2_FreeReceipt*);
    uint64_t (MT2FW_CALL *command_subscribe)(void*,MT2FW_MT2_CommandObserver,void* user);
    MT2FW_MT2_Result (MT2FW_CALL *command_unsubscribe)(void*,uint64_t token);
    uint64_t (MT2FW_CALL *input_subscribe)(void*,MT2FW_MT2_InputObserver,void* user);
    MT2FW_MT2_Result (MT2FW_CALL *input_unsubscribe)(void*,uint64_t token);
} MT2FW_MT2_OwnershipV1;
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_QueryOwnershipFn)(
    const MT2FW_Host*,uint32_t version,MT2FW_MT2_OwnershipV1*,uint32_t capacity);

#ifdef __cplusplus
}
#endif
