#pragma once
#include "native_services.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MT2FW_MT2_ROUTES_VERSION 1u
typedef enum MT2FW_MT2_RouteState {
    MT2FW_MT2_ROUTE_PRISTINE=0,
    MT2FW_MT2_ROUTE_PREPARED=1,
    MT2FW_MT2_ROUTE_ENABLED=2,
    MT2FW_MT2_ROUTE_UNKNOWN=3,
    MT2FW_MT2_ROUTE_REMOVED=4
} MT2FW_MT2_RouteState;
typedef struct MT2FW_MT2_RouteReceipt {
    uint32_t size;
    uint32_t state;
    uint64_t generation;
    uint64_t rva;
    uintptr_t entry;
    uintptr_t trampoline;
    uintptr_t detour;
    unsigned char pristine[32];
    unsigned char current[32];
    char owner[MT2FW_MT2_ID_BYTES];
    char game_sha256[65];
    char cause[256];
} MT2FW_MT2_RouteReceipt;

typedef struct MT2FW_MT2_CreateCall {
    uint32_t size;
    uint32_t flag;
    uint64_t invocation;
    uint64_t thread;
    void* manager;
    const void* type;
    const void* variant;
    const void* pose;
    void* a;
    void* b;
    void* c;
} MT2FW_MT2_CreateCall;
typedef void (MT2FW_CALL *MT2FW_MT2_CreateBefore)(void* user,const MT2FW_MT2_CreateCall* call);
typedef void (MT2FW_CALL *MT2FW_MT2_CreateAfter)(void* user,const MT2FW_MT2_CreateCall* call,void* result,uint32_t original_returned);

typedef enum MT2FW_MT2_RemoveDisposition {
    MT2FW_MT2_REMOVE_SUPPRESSED=0,
    MT2FW_MT2_REMOVE_FORWARDED=1,
    MT2FW_MT2_REMOVE_UNKNOWN=2
} MT2FW_MT2_RemoveDisposition;
typedef struct MT2FW_MT2_RemoveCall {
    uint32_t size;
    uint32_t reserved;
    uint64_t invocation;
    uint64_t thread;
    void* manager;
    void* array;
    /* Borrowed original native stack slot, captured at the noinline detour
       entry. It is not a stack slot from inside the service dispatcher. */
    uintptr_t native_return_slot;
    uintptr_t native_return_address;
} MT2FW_MT2_RemoveCall;
/* The continuation retains the original arguments and may be attempted once.
   It catches original C++ exceptions, returns FAILED and leaves the outcome
   unknown. The adapter rethrows the original after the authority returns. */
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_RemoveNext)(void* invocation);
typedef MT2FW_MT2_RemoveDisposition (MT2FW_CALL *MT2FW_MT2_RemoveAuthority)(
    void* user,const MT2FW_MT2_RemoveCall* call,MT2FW_MT2_RemoveNext next,void* invocation);

typedef struct MT2FW_MT2_RoutesV1 {
    uint32_t size;
    uint32_t version;
    void* context;
    uint64_t (MT2FW_CALL *create_subscribe)(void*,MT2FW_MT2_CreateBefore,MT2FW_MT2_CreateAfter,void* user);
    MT2FW_MT2_Result (MT2FW_CALL *create_unsubscribe)(void*,uint64_t);
    MT2FW_MT2_Result (MT2FW_CALL *remove_bind)(void*,MT2FW_MT2_RemoveAuthority,void* user,uint64_t* token);
    MT2FW_MT2_Result (MT2FW_CALL *remove_unbind)(void*,uint64_t);
    /* Prepare creates a disabled hook and publishes its actual trampoline.
       The plugin then publishes its own disabled receiver before enable.
       Each operation returns a current receipt; UNKNOWN must not be replayed. */
    MT2FW_MT2_Result (MT2FW_CALL *hook_prepare)(void*,const char* name,void* detour,MT2FW_MT2_RouteReceipt*);
    MT2FW_MT2_Result (MT2FW_CALL *hook_enable)(void*,const char* name,MT2FW_MT2_RouteReceipt*);
    MT2FW_MT2_Result (MT2FW_CALL *hook_remove)(void*,const char* name,MT2FW_MT2_RouteReceipt*);
    MT2FW_MT2_Result (MT2FW_CALL *route_read)(void*,const char* name,MT2FW_MT2_RouteReceipt*);
    /* Verify expected pristine bytes AND the live route. Only a catalogued
       target-relative window is accepted. Call its entry through the adapter,
       never bypass the shared observer using a receipt's trampoline. */
    MT2FW_MT2_Result (MT2FW_CALL *code_verify)(void*,const char* name,uint32_t offset,const void* expected,uint32_t length,MT2FW_MT2_RouteReceipt*);
} MT2FW_MT2_RoutesV1;
typedef MT2FW_MT2_Result (MT2FW_CALL *MT2FW_MT2_QueryRoutesFn)(const MT2FW_Host*,uint32_t,MT2FW_MT2_RoutesV1*,uint32_t);

#ifdef __cplusplus
}
#endif
