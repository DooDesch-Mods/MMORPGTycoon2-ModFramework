#include <windows.h>
#include <iostream>
#include <stdexcept>
#include <string>

void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
int wmain(int argc,wchar_t** argv){try{
    require(argc==2,"expected exact Framework DLL path");
    auto module=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_AS_DATAFILE|LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    require(module!=nullptr,"Framework resource-only mapping failed");
    struct Close{HMODULE module;~Close(){FreeLibrary(module);}}close{module};
    auto resource=FindResourceW(module,MAKEINTRESOURCEW(VS_VERSION_INFO),RT_VERSION);
    require(resource!=nullptr,"Framework VERSIONINFO absent");
    auto handle=LoadResource(module,resource);require(handle!=nullptr,"Framework VERSIONINFO load failed");
    const auto* bytes=LockResource(handle);require(bytes!=nullptr,"Framework VERSIONINFO lock failed");
    VS_FIXEDFILEINFO* fixed=nullptr;UINT length=0;
    require(VerQueryValueW(bytes,L"\\",reinterpret_cast<void**>(&fixed),&length)&&fixed&&length>=sizeof(*fixed)&&
        fixed->dwSignature==0xfeef04bd,"Framework fixed VERSIONINFO differs");
    const DWORD high=MAKELONG(MT2FW_VERSION_MINOR,MT2FW_VERSION_MAJOR),low=MAKELONG(0,MT2FW_VERSION_PATCH);
    require(fixed->dwFileVersionMS==high&&fixed->dwFileVersionLS==low&&fixed->dwProductVersionMS==high&&
        fixed->dwProductVersionLS==low&&fixed->dwFileType==VFT_DLL,"Framework fixed DLL version differs from CMake");
    const auto expected=std::to_wstring(MT2FW_VERSION_MAJOR)+L"."+std::to_wstring(MT2FW_VERSION_MINOR)+L"."+
        std::to_wstring(MT2FW_VERSION_PATCH);
    for(const auto* name:{L"FileVersion",L"ProductVersion",L"OriginalFilename"}){
        const auto key=std::wstring(L"\\StringFileInfo\\040904b0\\")+name;
        wchar_t* text=nullptr;length=0;
        require(VerQueryValueW(bytes,key.c_str(),reinterpret_cast<void**>(&text),&length)&&text&&length,
            "Framework string VERSIONINFO absent");
        require(std::wstring(text)==(std::wstring(name)==L"OriginalFilename"?L"MT2Framework.dll":expected),
            "Framework string version or filename differs");
    }
    for(unsigned id=201;id<=209;++id)require(FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA)!=nullptr,
        "Framework UI resource lost while adding version information");
    std::wcout<<L"PASS loaded DLL resource version "<<expected<<L".0 and all 9 UI resources\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
