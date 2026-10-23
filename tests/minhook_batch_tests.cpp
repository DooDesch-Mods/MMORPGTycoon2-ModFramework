#include "minhook_batch.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
using Function=int(*)();
__declspec(noinline) int first_detour(){return 41;}
__declspec(noinline) int second_detour(){return 42;}
__declspec(noinline) int third_detour(){return 43;}
struct Page {
    void* code=nullptr;
    explicit Page(int result){
        code=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);check(code!=nullptr,"allocate isolated executable fixture");
        std::memset(code,0x90,64);auto bytes=static_cast<unsigned char*>(code);bytes[0]=0xb8;std::memcpy(bytes+1,&result,4);bytes[5]=0xc3;
        check(FlushInstructionCache(GetCurrentProcess(),code,64)!=0,"flush fixture instructions");
    }
    void release(){check(VirtualFree(code,0,MEM_RELEASE)!=0,"release isolated fixture page");code=nullptr;}
    int call(){return reinterpret_cast<Function>(code)();}
};
void remove(void* target){auto disabled=MH_DisableHook(target);check(disabled==MH_OK||disabled==MH_ERROR_DISABLED,"disable isolated fixture hook");check(MH_RemoveHook(target)==MH_OK,"remove isolated fixture hook");}
}
int main(){try{
    check(MH_Initialize()==MH_OK,"initialize process-local MinHook registry");
    Page a(1),b(2),foreign(3);void* original_a=nullptr;void* original_b=nullptr;
    check(MH_CreateHook(a.code,reinterpret_cast<void*>(&first_detour),&original_a)==MH_OK,"prepare first disabled hook");
    check(MH_CreateHook(b.code,reinterpret_cast<void*>(&second_detour),&original_b)==MH_OK,"prepare second disabled hook");
    check(MH_CreateHook(foreign.code,reinterpret_cast<void*>(&third_detour),nullptr)==MH_OK,"prepare foreign disabled hook");
    void* targets[]={a.code,b.code};UINT failed=0;const char* phase=nullptr;
    check(a.call()==1&&b.call()==2&&foreign.call()==3,"prepared hooks have no visible effect");
    check(MH_QueueEnableHook(foreign.code)==MH_OK,"queue unrelated owner operation");
    check(MT2FW_MH_EnableBatch(targets,2,&failed,&phase)==MH_ERROR_UNSUPPORTED_FUNCTION&&std::strcmp(phase,"pending foreign queue")==0,"batch rejects another pending queue");
    check(a.call()==1&&b.call()==2&&foreign.call()==3,"queue rejection has no native effects");
    check(MH_ApplyQueued()==MH_OK&&foreign.call()==43,"foreign queue remains available to its original caller");
    check(MH_DisableHook(foreign.code)==MH_OK,"clear unrelated active hook");
    check(MT2FW_MH_EnableBatch(targets,2,&failed,&phase)==MH_OK,"activate explicit pair with private batch");
    check(a.call()==41&&b.call()==42&&foreign.call()==3,"only named hooks are active");
    check(reinterpret_cast<Function>(original_a)()==1&&reinterpret_cast<Function>(original_b)()==2,"both published trampolines retain original behavior");
    check(MT2FW_MH_EnableBatch(targets,2,&failed,&phase)==MH_ERROR_ENABLED,"already active batch is rejected");
    check(MH_DisableHook(a.code)==MH_OK&&MH_DisableHook(b.code)==MH_OK,"restore pair for partial-failure test");
    void* duplicates[]={a.code,a.code};check(MT2FW_MH_EnableBatch(duplicates,2,&failed,&phase)==MH_ERROR_ALREADY_CREATED,"duplicate slot is rejected before patching");
    check(a.call()==1,"duplicate admission leaves first slot unchanged");
    // The second fixture page belongs only to this process. Unmapping it makes
    // VirtualProtect fail after the first patch, without touching a game.
    auto removed_b=b.code;b.release();
    check(MT2FW_MH_EnableBatch(targets,2,&failed,&phase)==MH_ERROR_MEMORY_PROTECT&&failed==1,"native partial activation retains failing index and status");
    check(a.call()==41&&foreign.call()==3,"partial failure reports the actual first applied hook");
    check(MH_QueueEnableHook(foreign.code)==MH_OK&&MH_ApplyQueued()==MH_OK,"next unrelated queue is not contaminated by a failed batch");
    check(foreign.call()==43,"next unrelated operation applies correctly");
    check(MH_RemoveHook(removed_b)==MH_OK,"unapplied failed slot can be removed without accessing freed code");
    remove(a.code);remove(foreign.code);a.release();foreign.release();
    check(MH_Uninitialize()==MH_OK,"uninitialize isolated hook registry");
    std::cout<<checks<<" real MinHook batch checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"MinHook batch check failed after "<<checks<<": "<<error.what()<<'\n';return 1;}}
