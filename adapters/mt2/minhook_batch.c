/* Compile the pinned, unmodified MinHook implementation in this translation
   unit so the private batch operation shares its lock and slot registry. */
#include "../../third_party/vendor/minhook/src/hook.c"
#include "minhook_batch.h"

MH_STATUS WINAPI MT2FW_MH_EnableBatch(LPVOID const* targets,UINT count,UINT* failed_index,const char** phase)
{
    MH_STATUS status=MH_OK;
    UINT positions[128],i,j;
    FROZEN_THREADS threads;
    *failed_index=count;
    *phase="admission";
    if(!targets||!count||count>128)return MH_ERROR_NOT_CREATED;
    EnterSpinLock();
    if(g_hHeap==NULL){status=MH_ERROR_NOT_INITIALIZED;goto done;}
    for(i=0;i<g_hooks.size;++i){
        if(g_hooks.pItems[i].queueEnable!=g_hooks.pItems[i].isEnabled){
            *phase="pending foreign queue";status=MH_ERROR_UNSUPPORTED_FUNCTION;goto done;
        }
    }
    for(i=0;i<count;++i){
        positions[i]=FindHookEntry(targets[i]);
        *failed_index=i;
        if(positions[i]==INVALID_HOOK_POS){status=MH_ERROR_NOT_CREATED;goto done;}
        if(g_hooks.pItems[positions[i]].isEnabled){status=MH_ERROR_ENABLED;goto done;}
        for(j=0;j<i;++j)if(positions[j]==positions[i]){status=MH_ERROR_ALREADY_CREATED;goto done;}
    }
    for(i=0;i<count;++i)g_hooks.pItems[positions[i]].queueEnable=TRUE;
    *phase="freeze";*failed_index=count;
    status=Freeze(&threads,ALL_HOOKS_POS,ACTION_APPLY_QUEUED);
    if(status==MH_OK){
        *phase="activation";
        for(i=0;i<count;++i){
            *failed_index=i;status=EnableHookLL(positions[i],TRUE);
            if(status!=MH_OK)break;
        }
        /* Keep every remaining queue bit equal to its actual MinHook state.
           A later batch cannot accidentally apply an incomplete prior batch. */
        for(i=0;i<count;++i)g_hooks.pItems[positions[i]].queueEnable=g_hooks.pItems[positions[i]].isEnabled;
        Unfreeze(&threads);
    }else{
        for(i=0;i<count;++i)g_hooks.pItems[positions[i]].queueEnable=g_hooks.pItems[positions[i]].isEnabled;
    }
done:
    LeaveSpinLock();
    return status;
}
