#include <windows.h>
#include <MinHook.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
int checks=0;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
constexpr size_t gate=0x100;
constexpr unsigned char absolute[]={0xff,0x25,0,0,0,0};
void jump(unsigned char* at,void* destination){std::memcpy(at,absolute,6);std::memcpy(at+6,&destination,8);}
}

void exercise(bool jne){
    const size_t skip=gate+(jne?0xb3e:0x609);
    const unsigned char branch[]={0x0f,static_cast<unsigned char>(jne?0x85:0x84),
        static_cast<unsigned char>(jne?0x38:0x03),static_cast<unsigned char>(jne?0x0b:0x06),0,0};
    auto page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    check(page!=nullptr,"allocate isolated rotation fixture");std::memset(page,0x90,4096);
    // CMP publishes live condition flags. Both continuations capture them before
    // returning, so the test observes relocation rather than a C++ comparison.
    const unsigned char entry[]={0x83,0xf9,0,0xe9};std::memcpy(page,entry,sizeof(entry));
    const int32_t displacement=static_cast<int32_t>(gate-8);std::memcpy(page+4,&displacement,4);
    std::memcpy(page+gate,branch,sizeof(branch));
    const unsigned char rotate[]={0x9c,0x8f,0x02,0xb8,1,0,0,0,0xc3};
    const unsigned char hidden[]={0x9c,0x8f,0x02,0xb8,0,0,0,0,0xc3};
    std::memcpy(page+gate+6,rotate,sizeof(rotate));std::memcpy(page+skip,hidden,sizeof(hidden));
    check(FlushInstructionCache(GetCurrentProcess(),page,4096)!=0,"flush rotation fixture");
    using Function=int(*)(unsigned int,uint64_t*);auto call=reinterpret_cast<Function>(page);
    std::array<uint64_t,2> flags{};
    auto expected=[&](unsigned int value){return jne?1-static_cast<int>(value):static_cast<int>(value);};
    for(unsigned int value:{0u,1u})check(call(value,&flags[value])==expected(value),"original conditional branch uses incoming flags");
    auto verify=[&]{for(unsigned int value:{0u,1u}){
        uint64_t observed=0;
        check(call(value,&observed)==expected(value),"both original conditional destinations preserved");
        check((observed&0x8d5)==(flags[value]&0x8d5),"arithmetic flags survive relocated conditional jump and continuation");
    }};
    check(MH_Initialize()==MH_OK,"initialize real MinHook");
    void* original=nullptr;void* outer=nullptr;
    check(MH_CreateHook(page+gate,page+0x900,&original)==MH_OK,"prepare six-byte internal conditional branch");
    auto relocated=static_cast<unsigned char*>(original);
    check(relocated[0]==(jne?0x74:0x75)&&relocated[1]==14,"relocated Jcc uses inverse condition over absolute target jump");
    check(std::memcmp(relocated+2,absolute,6)==0,"conditional trampoline uses a flag-preserving absolute branch");
    void* target=nullptr;std::memcpy(&target,relocated+8,8);
    check(target==page+skip,"relocated taken destination equals original skip");
    check(std::memcmp(relocated+16,absolute,6)==0,"fallthrough uses flag-preserving jumpback");
    std::memcpy(&target,relocated+22,8);check(target==page+gate+6,"fallthrough resumes immediately after original Jcc");
    jump(page+0x900,original);
    check(MH_CreateHook(page,page+0xa00,&outer)==MH_OK,"prepare simultaneous outer entry hook");jump(page+0xa00,outer);
    check(FlushInstructionCache(GetCurrentProcess(),page,4096)!=0,"flush rotation forwarding shims");
    check(MH_EnableHook(page+gate)==MH_OK,"enable internal rotation branch");verify();
    check(MH_EnableHook(page)==MH_OK,"enable outer entry with internal rotation branch");verify();
    check(MH_DisableHook(page+gate)==MH_OK&&MH_RemoveHook(page+gate)==MH_OK,"retire internal branch while entry remains enabled");verify();
    check(std::memcmp(page+gate,branch,sizeof(branch))==0,"original six-byte conditional branch restored");
    check(MH_DisableHook(page)==MH_OK&&MH_RemoveHook(page)==MH_OK,"retire outer entry");verify();
    check(MH_Uninitialize()==MH_OK,"retire hook registry");
    check(VirtualFree(page,0,MEM_RELEASE)!=0,"release isolated executable fixture");
}
int main(){try{exercise(false);exercise(true);std::cout<<checks<<" JZ/JNE branch and live flags checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<"Conditional branch check failed after "<<checks<<": "<<error.what()<<'\n';return 1;}}
