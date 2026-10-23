#include "native_routes_windows.h"
#include "crypto.h"
#include <MinHook.h>
#include "minhook_batch.h"
#include <windows.h>
#include <fstream>
#include <cstring>

namespace mt2fw::mt2 {
namespace {
DWORD copy_memory(uintptr_t source,void* output,size_t length){
    __try{std::memcpy(output,reinterpret_cast<void*>(source),length);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool memory(uintptr_t source,void* output,size_t length,std::string& cause){
    if(!source||!output||!length||source>UINTPTR_MAX-length){cause="Native route memory bounds invalid";return false;}
    for(auto cursor=source;cursor<source+length;){MEMORY_BASIC_INFORMATION info{};
        if(!VirtualQuery(reinterpret_cast<void*>(cursor),&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS))){cause="Native route memory is inaccessible";return false;}
        auto end=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;if(end<=cursor){cause="Native route memory region does not advance";return false;}cursor=end;
    }
    auto exception=copy_memory(source,output,length);if(exception){cause="Native route memory exception="+std::to_string(exception);return false;}return true;
}
bool prove(void* address,void* detour,std::string& cause){
    auto cursor=reinterpret_cast<uintptr_t>(address),expected=reinterpret_cast<uintptr_t>(detour);
    for(int i=0;i<4;++i){
        if(cursor==expected)return true;unsigned char bytes[6]{};if(!memory(cursor,bytes,sizeof(bytes),cause))return false;
        if(bytes[0]==0xe9){int32_t displacement=0;std::memcpy(&displacement,bytes+1,4);cursor=cursor+5+displacement;continue;}
        if(bytes[0]==0xeb){int8_t displacement=0;std::memcpy(&displacement,bytes+1,1);cursor=cursor+2+displacement;continue;}
        if(bytes[0]==0xff&&bytes[1]==0x25){int32_t displacement=0;std::memcpy(&displacement,bytes+2,4);uintptr_t next=0;if(!memory(cursor+6+displacement,&next,sizeof(next),cause))return false;cursor=next;continue;}
        cause="Enabled native route is not the acknowledged detour";return false;
    }
    cause="Enabled native route exceeds the bounded jump proof";return false;
}
struct GameCode {
    struct Section {uint32_t rva,raw,size;};
    std::vector<unsigned char> bytes;
    std::vector<Section> sections;
    explicit GameCode(const fs::path& path){
        if(file_digest(path)!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")throw std::runtime_error("Native route executable hash differs");
        std::ifstream stream(path,std::ios::binary);bytes.assign(std::istreambuf_iterator<char>(stream),{});if(!stream.eof()&&stream.fail())throw std::runtime_error("Native route executable read failed");
        auto read=[&](size_t offset,void* result,size_t size){if(offset>bytes.size()||size>bytes.size()-offset)throw std::runtime_error("Native route PE bounds invalid");std::memcpy(result,bytes.data()+offset,size);};
        IMAGE_DOS_HEADER dos{};read(0,&dos,sizeof(dos));if(dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<=0)throw std::runtime_error("Native route DOS header differs");
        DWORD signature=0;read(dos.e_lfanew,&signature,sizeof(signature));if(signature!=IMAGE_NT_SIGNATURE)throw std::runtime_error("Native route PE signature differs");
        IMAGE_FILE_HEADER header{};read(dos.e_lfanew+4,&header,sizeof(header));if(header.Machine!=IMAGE_FILE_MACHINE_AMD64||header.NumberOfSections>96)throw std::runtime_error("Native route PE machine/sections differ");
        auto start=size_t(dos.e_lfanew)+4+sizeof(header)+header.SizeOfOptionalHeader;
        for(size_t i=0;i<header.NumberOfSections;++i){IMAGE_SECTION_HEADER section{};read(start+i*sizeof(section),&section,sizeof(section));if(section.Characteristics&IMAGE_SCN_MEM_EXECUTE)sections.push_back({section.VirtualAddress,section.PointerToRawData,section.SizeOfRawData});}
    }
    bool pristine(uint64_t rva,uint32_t offset,void* output,size_t length,std::string& cause)const{
        if(rva>UINT32_MAX-offset||!output||!length){cause="Pristine code bounds invalid";return false;}auto begin=rva+offset;
        for(auto& section:sections)if(begin>=section.rva&&begin-section.rva<section.size){auto relative=begin-section.rva;if(length>section.size-relative){cause="Pristine code crosses executable section";return false;}
            auto raw=uint64_t(section.raw)+relative;if(raw>bytes.size()||length>bytes.size()-raw){cause="Pristine code raw range invalid";return false;}std::memcpy(output,bytes.data()+raw,length);return true;}
        cause="Named native target is outside executable PE data";return false;
    }
};
}
NativeRoutes::Backend windows_route_backend(const fs::path& executable){
    auto game=std::make_shared<GameCode>(executable);NativeRoutes::Backend result;
    result.pristine=[game](uint64_t rva,uint32_t offset,void* output,size_t length,std::string& cause){return game->pristine(rva,offset,output,length,cause);};
    result.read=memory;result.prove=prove;
    result.create=[](void* address,void* detour,void** original,std::string& cause){auto status=MH_CreateHook(address,detour,original);if(status==MH_OK)return true;cause=MH_StatusToString(status);return false;};
    result.enable=[](void* address,void* detour,std::string& cause){auto status=MH_EnableHook(address);if(status!=MH_OK){cause=MH_StatusToString(status);return false;}return prove(address,detour,cause);};
    result.enable_batch=[](const std::vector<std::pair<void*,void*>>& hooks,std::string& cause){
        std::vector<void*> targets;targets.reserve(hooks.size());for(const auto& hook:hooks)targets.push_back(hook.first);
        UINT failed=0;const char* phase="admission";auto status=MT2FW_MH_EnableBatch(targets.data(),static_cast<UINT>(targets.size()),&failed,&phase);
        if(status!=MH_OK){cause=std::string("Native batch ")+phase+" index="+std::to_string(failed)+": "+MH_StatusToString(status);return false;}
        for(const auto& [address,detour]:hooks)if(!prove(address,detour,cause))return false;
        return true;
    };
    result.remove=[](void* address,std::string& cause){auto status=MH_DisableHook(address);if(status!=MH_OK&&status!=MH_ERROR_DISABLED){cause=MH_StatusToString(status);return false;}status=MH_RemoveHook(address);if(status!=MH_OK){cause=MH_StatusToString(status);return false;}return true;};
    result.disable=[](void* address,std::string& cause){auto status=MH_DisableHook(address);if(status==MH_OK||status==MH_ERROR_DISABLED)return true;cause=MH_StatusToString(status);return false;};
    return result;
}
void load_multiplayer_catalog(NativeRoutes& catalog){
#include "multiplayer_catalog.inc"
}
}
