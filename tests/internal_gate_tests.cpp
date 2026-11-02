#include <windows.h>
#include <MinHook.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
constexpr size_t gate=0xc2,deferred=gate+0x380,full=gate+30;
constexpr unsigned char original_gate[]={0x80,0xbb,0xc0,0x01,0,0,0,0x0f,0x84,0x73,0x03,0,0,
    0x80,0xbb,0x08,0x08,0,0,0,0x0f,0x84,0x66,0x03,0,0,0x0f,0x1f,0x40,0};
void absolute_jump(unsigned char* at,void* destination){
    const unsigned char prefix[]={0xff,0x25,0,0,0,0};
    std::memcpy(at,prefix,sizeof(prefix));std::memcpy(at+6,&destination,sizeof(destination));
}
}

int main(){try{
    auto page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    check(page!=nullptr,"allocate isolated branch fixture");std::memset(page,0x90,4096);
    // Preserve RBX as a native caller must, then enter the exact internal block.
    const unsigned char entry[]={0x53,0x48,0x89,0xcb,0xe9};std::memcpy(page,entry,sizeof(entry));
    const int32_t displacement=static_cast<int32_t>(gate-9);std::memcpy(page+5,&displacement,4);
    std::memcpy(page+gate,original_gate,sizeof(original_gate));
    const unsigned char full_result[]={0xb8,1,0,0,0,0x5b,0xc3};std::memcpy(page+full,full_result,sizeof(full_result));
    const unsigned char deferred_result[]={0x31,0xc0,0x5b,0xc3};std::memcpy(page+deferred,deferred_result,sizeof(deferred_result));
    check(FlushInstructionCache(GetCurrentProcess(),page,4096)!=0,"flush isolated branch fixture");
    using Function=int(*)(void*);auto call=reinterpret_cast<Function>(page);
    std::array<unsigned char,0x820> actor{};
    constexpr std::array<unsigned char,2> switches{0,1};
    auto truth_table=[&]{for(auto visible:switches)for(auto camera_near:switches){
        actor[0x1c0]=visible;actor[0x808]=camera_near;const auto before=actor;
        check(call(actor.data())==(visible&&camera_near?1:0),"original conditional branches and fallthrough preserved");
        check(actor==before,"internal branch forwarding never writes actor fields");
    }};
    truth_table();check(MH_Initialize()==MH_OK,"initialize real MinHook");
    void* gate_original=nullptr;void* entry_original=nullptr;
    check(MH_CreateHook(page+gate,page+0x600,&gate_original)==MH_OK,"prepare exact internal compare site");
    check(std::memcmp(gate_original,original_gate,7)==0,"trampoline preserves full seven-byte displaced compare");
    auto relocated=static_cast<unsigned char*>(gate_original);
    const unsigned char jump_prefix[]={0xff,0x25,0,0,0,0};
    check(std::memcmp(relocated+7,jump_prefix,sizeof(jump_prefix))==0,"trampoline uses flag-preserving absolute jump");
    void* resume=nullptr;std::memcpy(&resume,relocated+13,sizeof(resume));
    check(resume==page+gate+7,"trampoline resumes at original first conditional branch");
    absolute_jump(page+0x600,gate_original);
    check(MH_CreateHook(page,page+0x680,&entry_original)==MH_OK,"prepare independent outer entry hook");
    absolute_jump(page+0x680,entry_original);
    check(FlushInstructionCache(GetCurrentProcess(),page,4096)!=0,"flush published forwarding shims");
    check(MH_EnableHook(page+gate)==MH_OK,"enable owned internal site");truth_table();
    check(std::memcmp(page+gate+5,original_gate+5,sizeof(original_gate)-5)==0,"internal patch leaves all original conditional branches intact");
    check(MH_EnableHook(page)==MH_OK,"enable outer entry beside internal site");truth_table();
    check(MH_DisableHook(page+gate)==MH_OK&&MH_RemoveHook(page+gate)==MH_OK,"retire internal site with outer entry active");
    truth_table();check(std::memcmp(page+gate,original_gate,sizeof(original_gate))==0,"internal pristine block fully restored");
    check(MH_DisableHook(page)==MH_OK&&MH_RemoveHook(page)==MH_OK,"retire outer entry");truth_table();
    check(MH_Uninitialize()==MH_OK,"retire isolated MinHook registry");
    check(VirtualFree(page,0,MEM_RELEASE)!=0,"release isolated executable fixture");
    std::cout<<checks<<" internal branch relocation checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"Internal branch check failed after "<<checks<<": "<<error.what()<<'\n';return 1;}}
