#include "native_ownership.h"
#include "crypto.h"
#include <windows.h>
#include <cstring>

namespace mt2fw::mt2 {
namespace {
constexpr uint32_t free_slot_rva=0x1c52a00,malloc_slot_rva=0x1c52a70;
DWORD copy_memory(uintptr_t address,void* output,size_t length){__try{std::memcpy(output,reinterpret_cast<void*>(address),length);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
void require(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
std::string win32(const char* operation){return std::string(operation)+" win32="+std::to_string(GetLastError());}
template<size_t N>void text(char(&output)[N],const std::string& value){require(value.size()<N,"Native ownership text exceeds receipt capacity");std::memcpy(output,value.c_str(),value.size()+1);}
template<class T>T read(uintptr_t address){T result{};auto code=copy_memory(address,&result,sizeof(result));require(!code,"Native ownership memory read SEH="+std::to_string(code));return result;}
MEMORY_BASIC_INFORMATION region(uintptr_t address,uintptr_t owner,bool executable){
    MEMORY_BASIC_INFORMATION value{};auto count=VirtualQuery(reinterpret_cast<void*>(address),&value,sizeof(value));if(!count)throw std::runtime_error(win32("VirtualQuery"));require(count==sizeof(value),"VirtualQuery returned unexpected structure size="+std::to_string(count));
    auto protection=value.Protect&0xff;bool kind=executable?(protection==PAGE_EXECUTE||protection==PAGE_EXECUTE_READ):(protection==PAGE_READONLY||protection==PAGE_READWRITE);
    require(value.State==MEM_COMMIT&&value.Type==MEM_IMAGE&&reinterpret_cast<uintptr_t>(value.AllocationBase)==owner&&!(value.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&kind,"Native ownership address is outside the expected immutable code or image data owner");return value;
}
IMAGE_NT_HEADERS64 headers(uintptr_t base){
    auto dos=read<IMAGE_DOS_HEADER>(base);require(dos.e_magic==IMAGE_DOS_SIGNATURE&&dos.e_lfanew>0&&dos.e_lfanew<0x100000,"Native ownership DOS header differs");
    auto nt=read<IMAGE_NT_HEADERS64>(base+dos.e_lfanew);require(nt.Signature==IMAGE_NT_SIGNATURE&&nt.FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64&&nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC,"Native ownership PE64 header differs");return nt;
}
std::string image_text(uintptr_t base,uint32_t rva,uint32_t image_size){
    std::string value;for(uint32_t i=0;i<1024;++i){require(uint64_t(rva)+i<image_size,"Native ownership PE string exceeds image");char next=read<char>(base+rva+i);if(!next)return value;value.push_back(next);}throw std::runtime_error("Native ownership PE string exceeds capacity");
}
void named_import(uintptr_t image,const char* symbol,uint32_t slot){
    auto nt=headers(image);auto directory=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    require(directory.VirtualAddress&&uint64_t(directory.VirtualAddress)+directory.Size<=nt.OptionalHeader.SizeOfImage,"Native ownership import directory differs");
    for(uint32_t offset=0;uint64_t(offset)+sizeof(IMAGE_IMPORT_DESCRIPTOR)<=directory.Size;offset+=sizeof(IMAGE_IMPORT_DESCRIPTOR)){
        auto descriptor=read<IMAGE_IMPORT_DESCRIPTOR>(image+directory.VirtualAddress+offset);if(!descriptor.Name)break;
        auto dll=image_text(image,descriptor.Name,nt.OptionalHeader.SizeOfImage);if(_stricmp(dll.c_str(),"msvcrt.dll"))continue;
        require(descriptor.OriginalFirstThunk!=0,"Named CRT import has no original symbol table");
        for(uint32_t index=0;index<4096;++index){auto lookup=uint64_t(descriptor.OriginalFirstThunk)+index*8ull;require(lookup+8<=nt.OptionalHeader.SizeOfImage,"Named CRT symbol table exceeds image");auto thunk=read<uint64_t>(image+lookup);if(!thunk)break;if(thunk&IMAGE_ORDINAL_FLAG64)continue;
            require(thunk<UINT32_MAX-2,"Named CRT symbol RVA overflows");if(image_text(image,uint32_t(thunk)+2,nt.OptionalHeader.SizeOfImage)!=symbol)continue;
            require(uint64_t(descriptor.FirstThunk)+index*8ull==slot,"Named CRT symbol does not own the exact required IAT slot");return;
        }throw std::runtime_error(std::string("Named CRT import absent: ")+symbol);
    }throw std::runtime_error("Executable has no expected msvcrt import descriptor");
}
struct HeapImports {
    uintptr_t image;
    fs::path executable;
    NativeOwnership::Snapshot proof;
    bool ready=false;
    // File handles deliberately remain open until process exit, including
    // partial preparation, so a published module receipt cannot change on disk.
    std::vector<HANDLE> files;
    HeapImports(uintptr_t address,fs::path path):image(address),executable(std::move(path)){}
    MT2FW_MT2_ModuleProof module(uintptr_t address){
        HMODULE handle=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(address),&handle))throw std::runtime_error(win32("Module pin"));
        wchar_t path[32768]{};auto count=GetModuleFileNameW(handle,path,DWORD(std::size(path)));require(count&&count<std::size(path),win32("Module path"));
        auto file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);require(file!=INVALID_HANDLE_VALUE,win32("Module file pin"));files.push_back(file);
        LARGE_INTEGER size{};if(!GetFileSizeEx(file,&size))throw std::runtime_error(win32("Module file size"));require(size.QuadPart>0,"Module file is empty");
        MT2FW_MT2_ModuleProof result{};result.base=reinterpret_cast<uintptr_t>(handle);result.pinned=1;result.bytes=uint64_t(size.QuadPart);text(result.path,path_text(fs::path(path)));text(result.sha256,file_digest(fs::path(path)));
        DWORD unused=0;SetLastError(0);auto version_bytes=GetFileVersionInfoSizeW(path,&unused);
        if(!version_bytes){result.version_error=GetLastError();require(result.version_error==ERROR_RESOURCE_DATA_NOT_FOUND||result.version_error==ERROR_RESOURCE_TYPE_NOT_FOUND||result.version_error==ERROR_RESOURCE_NAME_NOT_FOUND||result.version_error==ERROR_RESOURCE_LANG_NOT_FOUND,"Module version query failed win32="+std::to_string(result.version_error));return result;}
        require(version_bytes<=1024*1024,"Module version resource exceeds bound");std::vector<unsigned char> bytes(version_bytes);if(!GetFileVersionInfoW(path,0,version_bytes,bytes.data()))throw std::runtime_error(win32("Module version read"));VS_FIXEDFILEINFO* version=nullptr;UINT length=0;
        require(VerQueryValueW(bytes.data(),L"\\",reinterpret_cast<void**>(&version),&length)!=0&&version&&length>=sizeof(*version)&&version->dwSignature==0xfeef04bd,"Module fixed version resource differs");
        result.version_present=1;result.version[0]=HIWORD(version->dwFileVersionMS);result.version[1]=LOWORD(version->dwFileVersionMS);result.version[2]=HIWORD(version->dwFileVersionLS);result.version[3]=LOWORD(version->dwFileVersionLS);return result;
    }
    MT2FW_MT2_ExportProof exported(HMODULE imported,const char* symbol,MT2FW_MT2_ModuleProof& provider){
        auto function=GetProcAddress(imported,symbol);require(function!=nullptr,win32("CRT export"));auto resolved=reinterpret_cast<uintptr_t>(function);provider=module(resolved);region(resolved,provider.base,true);
        auto base=reinterpret_cast<uintptr_t>(imported);auto nt=headers(base);auto directory=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];require(directory.VirtualAddress&&uint64_t(directory.VirtualAddress)+directory.Size<=nt.OptionalHeader.SizeOfImage,"CRT export directory exceeds image");
        auto exports=read<IMAGE_EXPORT_DIRECTORY>(base+directory.VirtualAddress);require(exports.NumberOfNames<=100000&&exports.NumberOfFunctions<=100000,"CRT export table exceeds bound");
        for(uint32_t i=0;i<exports.NumberOfNames;++i){require(uint64_t(exports.AddressOfNames)+i*4ull+4<=nt.OptionalHeader.SizeOfImage&&uint64_t(exports.AddressOfNameOrdinals)+i*2ull+2<=nt.OptionalHeader.SizeOfImage,"CRT export symbol table exceeds image");
            auto name=read<uint32_t>(base+exports.AddressOfNames+i*4ull);if(image_text(base,name,nt.OptionalHeader.SizeOfImage)!=symbol)continue;auto ordinal=read<uint16_t>(base+exports.AddressOfNameOrdinals+i*2ull);require(ordinal<exports.NumberOfFunctions&&uint64_t(exports.AddressOfFunctions)+ordinal*4ull+4<=nt.OptionalHeader.SizeOfImage,"CRT export ordinal exceeds image");
            auto rva=read<uint32_t>(base+exports.AddressOfFunctions+ordinal*4ull);require(rva<nt.OptionalHeader.SizeOfImage,"CRT export address exceeds image");MT2FW_MT2_ExportProof result{};result.resolved=resolved;result.owner=provider.base;result.export_rva=rva;result.forwarded=rva>=directory.VirtualAddress&&uint64_t(rva)<uint64_t(directory.VirtualAddress)+directory.Size;
            if(result.forwarded)text(result.forwarder,image_text(base,rva,nt.OptionalHeader.SizeOfImage));else require(base+rva==resolved,"CRT export differs from resolved symbol address");return result;
        }throw std::runtime_error(std::string("CRT named export is absent: ")+symbol);
    }
    void prepare(){
        if(ready)return;require(file_digest(executable)=="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6","Free ownership executable SHA differs");
        named_import(image,"free",free_slot_rva);named_import(image,"malloc",malloc_slot_rva);auto imported=GetModuleHandleW(L"msvcrt.dll");require(imported!=nullptr,win32("Imported CRT module"));
        proof.imported_crt=module(reinterpret_cast<uintptr_t>(imported));proof.free_export=exported(imported,"free",proof.free_provider);proof.malloc_export=exported(imported,"malloc",proof.malloc_provider);proof.slot=image+free_slot_rva;proof.malloc_slot=image+malloc_slot_rva;ready=true;
    }
    void observe(NativeOwnership::Snapshot& output){
        require(ready,"Free ownership provider has not been prepared");auto imported=reinterpret_cast<HMODULE>(proof.imported_crt.base);
        require(reinterpret_cast<uintptr_t>(GetProcAddress(imported,"free"))==proof.free_export.resolved&&reinterpret_cast<uintptr_t>(GetProcAddress(imported,"malloc"))==proof.malloc_export.resolved,"Prepared CRT resolved exports changed");
        region(proof.free_export.resolved,proof.free_provider.base,true);region(proof.malloc_export.resolved,proof.malloc_provider.base,true);
        auto page=region(proof.slot,image,false);region(proof.malloc_slot,image,false);
        require(proof.slot%alignof(void*)==0&&proof.slot+sizeof(void*)<=reinterpret_cast<uintptr_t>(page.BaseAddress)+page.RegionSize,"Exact free IAT slot is not aligned image data");
        output=proof;output.current=read<uintptr_t>(proof.slot);output.malloc_current=read<uintptr_t>(proof.malloc_slot);output.protection=page.Protect;
    }
};
NativeOwnership::Exchange exchange_import(uintptr_t image,uintptr_t slot,uintptr_t expected,uintptr_t replacement,uint32_t protection,bool repair)noexcept{
    NativeOwnership::Exchange result;MEMORY_BASIC_INFORMATION page{};
    if(!expected||!replacement||slot!=image+free_slot_rva||slot%alignof(void*)||!protection){result.win32=ERROR_INVALID_PARAMETER;return result;}
    if(!VirtualQuery(reinterpret_cast<void*>(slot),&page,sizeof(page))||page.State!=MEM_COMMIT||page.Type!=MEM_IMAGE||reinterpret_cast<uintptr_t>(page.AllocationBase)!=image||(page.Protect&(PAGE_GUARD|PAGE_NOACCESS))||(!repair&&page.Protect!=protection)){result.win32=ERROR_INVALID_STATE;return result;}
    DWORD before=0,ignored=0;if(!VirtualProtect(reinterpret_cast<void*>(slot),sizeof(void*),PAGE_READWRITE,&before)){result.win32=GetLastError();return result;}
    result.before=before;result.attempted=true;
    __try{result.observed=reinterpret_cast<uintptr_t>(InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(slot),reinterpret_cast<void*>(replacement),reinterpret_cast<void*>(expected)));result.exchanged=result.observed==expected;}
    __except(EXCEPTION_EXECUTE_HANDLER){result.seh=GetExceptionCode();}
    result.protection_restored=VirtualProtect(reinterpret_cast<void*>(slot),sizeof(void*),protection,&ignored)!=0;
    if(!result.protection_restored)result.win32=GetLastError();else if(!result.exchanged&&!result.seh)result.win32=ERROR_INVALID_STATE;
    return result;
}
}
NativeOwnership::Backend windows_free_backend(uintptr_t image,const fs::path& executable){
    auto heap=std::make_shared<HeapImports>(image,executable);NativeOwnership::Backend result;
    result.prepare=[heap](NativeOwnership::Snapshot& output,std::string& cause){try{heap->prepare();heap->observe(output);return true;}catch(const std::exception& problem){cause=problem.what();return false;}};
    result.read=[heap](NativeOwnership::Snapshot& output,std::string& cause){try{heap->observe(output);return true;}catch(const std::exception& problem){cause=problem.what();return false;}};
    result.replacement=[](uintptr_t address,std::string& cause){try{HMODULE module=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(address),&module))throw std::runtime_error(win32("Replacement module pin"));region(address,reinterpret_cast<uintptr_t>(module),true);return true;}catch(const std::exception& problem){cause=problem.what();return false;}};
    result.exchange=[image](uintptr_t expected,uintptr_t replacement,uint32_t protection,bool repair){return exchange_import(image,image+free_slot_rva,expected,replacement,protection,repair);};return result;
}
}
