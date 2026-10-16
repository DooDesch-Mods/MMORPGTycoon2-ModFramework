#include "mt2fw/core.h"
#include "crypto.h"
#include "bootstrap.h"
#include <atomic>
#include <fstream>
#include <mutex>
#include <array>
#include <cstring>

namespace {
using namespace mt2fw;
HMODULE self_module=nullptr, system_module=nullptr;
std::once_flag system_once, profile_once;
std::atomic<bool> enabled=false, stopped=false;
std::atomic<unsigned> isolation=0, state=0;
fs::path profile, game, data;
std::string data_utf8;
using Void=void(*)();
using Pref=char*(*)(const char*,const char*);
using Duplicate=char*(*)(const char*);
using WriteDir=int(*)(const char*);
Void original_callbacks=nullptr,original_shutdown=nullptr,runtime_shutdown=nullptr;
Pref original_pref=nullptr;Duplicate duplicate_string=nullptr;WriteDir original_write=nullptr;
void* targets[75]{};
struct Export {WORD ordinal;const char* name;};
constexpr Export exports[]={
#include "wsock32_exports.inc"
};
fs::path module_path(HMODULE module) {
    std::array<wchar_t,32768> buffer{};auto count=GetModuleFileNameW(module,buffer.data(),DWORD(buffer.size()));
    if(!count||count>=buffer.size())throw std::runtime_error("Module path failed Win32="+std::to_string(GetLastError()));
    return fs::path(buffer.data());
}
void record(const char* level,const std::string& message) {
    auto bytes=Json({{"level",level},{"message",message.substr(0,8192)}}).dump()+"\n";OutputDebugStringA(bytes.c_str());
    if(!profile.empty()) {std::ofstream stream(profile/"bootstrap.jsonl",std::ios::app);stream<<bytes;if(!stream)OutputDebugStringA("error: framework bootstrap log write failed\n");}
}
void ensure_system() {
    std::call_once(system_once,[]{
        wchar_t directory[MAX_PATH]{};auto length=GetSystemDirectoryW(directory,MAX_PATH);
        if(!length||length>=MAX_PATH)throw std::runtime_error("Cannot resolve System32");
        system_module=LoadLibraryExW((fs::path(directory)/L"wsock32.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!system_module||system_module==self_module)throw std::runtime_error("Cannot load original System32 WSOCK32");
        for(size_t i=0;i<std::size(exports);++i) {
            auto fn=GetProcAddress(system_module,exports[i].name);
            if(!fn||fn!=GetProcAddress(system_module,MAKEINTRESOURCEA(exports[i].ordinal)))throw std::runtime_error("System socket export differs: "+std::string(exports[i].name));
            targets[i]=reinterpret_cast<void*>(fn);
        }
    });
}
void prepare() {
    std::call_once(profile_once,[]{
        try {
            game=module_path(nullptr).parent_path();
            std::ifstream stream(game/"mt2fw.json");if(!stream){record("info","No framework configuration; original game continues");return;}
            Json config;stream>>config;if(!config.value("enabled",true)){record("info","Framework disabled; original game continues");return;}
            if(file_digest(game/"MT2.exe")!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6") {record("error","Unsupported executable; adapter and mods disabled");return;}
            if(GetModuleHandleW(L"MT2_Multiplayer.dll")){record("error","Another runtime already owns this process; framework disabled");return;}
            auto selected=fs::u8path(config.at("profile").get<std::string>());
            if(!selected.is_absolute())throw std::runtime_error("Framework profile must be absolute");
            auto canonical=fs::weakly_canonical(selected);
            if(canonical!=selected.lexically_normal())throw std::runtime_error("Framework profile path is an alias");
            for(auto current=canonical;!current.empty()&&current!=current.root_path();current=current.parent_path()) {
                auto attributes=GetFileAttributesW(current.c_str());
                if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Framework profile contains a reparse point");
            }
            fs::create_directories(canonical/"game-data");profile=canonical;data=profile/"game-data";
            auto encoded=data.generic_u8string();data_utf8.assign(reinterpret_cast<const char*>(encoded.data()),encoded.size());data_utf8+='/';
            enabled=true;record("info","Exact game build accepted; isolated framework profile prepared");
        }catch(const std::exception& error){enabled=false;record("error",error.what());}
    });
}
char* pref_path(const char* organization,const char* application) {
    prepare();if(!enabled)return original_pref(organization,application);
    char* value=duplicate_string(data_utf8.c_str());if(!value){record("error","SDL profile allocation failed");return nullptr;}
    isolation.fetch_or(1);return value;
}
void import_copies(fs::path account) {
    static std::mutex imports_gate;std::lock_guard lock(imports_gate);
    if(account.filename().empty())account=account.parent_path();
    if(account.parent_path()!=data||account.filename().string().find_first_not_of("0123456789")!=std::string::npos)return;
    auto imports=profile/"imports";if(!fs::exists(imports))return;
    for(auto& entry:fs::directory_iterator(imports)) {
        if(!entry.is_directory())continue;auto job_path=confined(imports,entry.path().filename()/"job.json");if(!fs::exists(job_path))continue;
        auto job=read_json(job_path);if(job.value("state","")!="pending")continue;
        try {
            auto world=job.at("world").get<std::string>();
            if(world.find_first_of("/\\:")!=std::string::npos||world=="."||world=="..")throw std::runtime_error("Invalid copied world name");
            for(auto& [name,hash]:job.at("files").items()){
                if(fs::path(name).filename()!=fs::path(name))throw std::runtime_error("Imported save filename contains a directory");
                auto source=confined(entry.path(),name),target=confined(account,fs::path("save")/fs::u8path(world)/fs::u8path(name));
                if(file_digest(source)!=hash.get<std::string>())throw std::runtime_error("Imported copy hash differs: "+name);
                if(fs::exists(target))throw std::runtime_error("Copied world destination already exists: "+name);
            }
            for(auto& [name,hash]:job.at("files").items()){
                auto target=confined(account,fs::path("save")/fs::u8path(world)/fs::u8path(name));fs::create_directories(target.parent_path());
                fs::copy_file(confined(entry.path(),name),target,fs::copy_options::none);
                if(file_digest(target)!=hash.get<std::string>())throw std::runtime_error("Copied world verification failed: "+name);
            }
            job["state"]="complete";job["account"]=account.filename().string();record("info","Copied save imported into isolated Steam profile");
        }catch(const std::exception& error){job["state"]="failed";job["error"]=error.what();record("error",std::string("Save copy import failed: ")+error.what());}
        atomic_json(job_path,job);
    }
}
int write_dir(const char* directory) {
    prepare();if(!enabled||!directory)return original_write(directory);
    try {
        auto requested=fs::weakly_canonical(fs::u8path(directory));
        auto mismatch=std::mismatch(data.begin(),data.end(),requested.begin(),requested.end());
        if(mismatch.first!=data.end())throw std::runtime_error("PHYSFS write outside private framework profile refused");
        int result=original_write(directory);if(result){isolation.fetch_or(2);import_copies(requested);}else record("error","Original PHYSFS_setWriteDir rejected private profile");return result;
    }catch(const std::exception& error){record("error",error.what());return 0;}
}
void callbacks() {
    original_callbacks();if(stopped)return;prepare();if(!enabled||isolation.load()!=3)return;
    unsigned expected=0;if(!state.compare_exchange_strong(expected,1))return;
    try {
        auto path=game/"MT2Framework.dll";
        auto module=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module||module_path(module)!=path)throw std::runtime_error("MT2Framework.dll exact-path load failed Win32="+std::to_string(GetLastError()));
        auto initialize=reinterpret_cast<bool(*)(const MT2FW_Bootstrap*)>(GetProcAddress(module,"MT2FW_InitInstalled"));
        runtime_shutdown=reinterpret_cast<Void>(GetProcAddress(module,"MT2FW_Shutdown"));
        if(!initialize||!runtime_shutdown)throw std::runtime_error("Framework bootstrap exports missing");
        MT2FW_Bootstrap receipt{sizeof(MT2FW_Bootstrap),1,profile.c_str(),game.c_str(),isolation.load(),GetCurrentProcessId()};
        if(!initialize(&receipt))throw std::runtime_error("Framework initialization rejected; private profile retained");
        state=2;record("info","Framework initialized after original Steam callback");
    }catch(const std::exception& error){state=3;record("error",error.what());}
}
void shutdown() {
    if(!stopped.exchange(true)&&state.exchange(3)==2&&runtime_shutdown)runtime_shutdown();
    original_shutdown();
}
struct Patch {uintptr_t* slot;uintptr_t original;uintptr_t replacement;};
std::array<Patch,4> patches{};size_t patched=0;
bool patch(const char* library,const char* symbol,void* replacement,void** original) {
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>1048576)return false;
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE)return false;
    auto descriptor=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for(;descriptor->Name;++descriptor)if(_stricmp(reinterpret_cast<char*>(base+descriptor->Name),library)==0) {
        if(!descriptor->OriginalFirstThunk)return false;
        auto names=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+descriptor->OriginalFirstThunk),slots=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+descriptor->FirstThunk);
        for(;names->u1.AddressOfData;++names,++slots)if(!IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal)) {
            auto name=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
            if(std::strcmp(reinterpret_cast<char*>(name->Name),symbol)!=0)continue;
            auto slot=reinterpret_cast<uintptr_t*>(&slots->u1.Function);DWORD protection=0;
            if(patched>=patches.size()||!VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&protection))return false;
            *original=reinterpret_cast<void*>(*slot);patches[patched++]={slot,*slot,reinterpret_cast<uintptr_t>(replacement)};*slot=reinterpret_cast<uintptr_t>(replacement);
            DWORD ignored=0;return VirtualProtect(slot,sizeof(*slot),protection,&ignored)!=0;
        }
    }
    return false;
}
void rollback() {
    while(patched) {auto entry=patches[--patched];DWORD protection=0;
        if(*entry.slot!=entry.replacement||!VirtualProtect(entry.slot,sizeof(*entry.slot),PAGE_READWRITE,&protection)){OutputDebugStringA("error: framework IAT rollback guard failed\n");continue;}
        *entry.slot=entry.original;DWORD ignored=0;if(!VirtualProtect(entry.slot,sizeof(*entry.slot),protection,&ignored))OutputDebugStringA("error: framework IAT protection restore failed\n");
    }
}
bool bridges() {
    auto sdl=GetModuleHandleW(L"SDL3.dll");if(sdl)duplicate_string=reinterpret_cast<Duplicate>(GetProcAddress(sdl,"SDL_strdup"));
    if(!duplicate_string||!patch("SDL3.dll","SDL_GetPrefPath",reinterpret_cast<void*>(pref_path),reinterpret_cast<void**>(&original_pref))||
       !patch("libphysfs.dll","PHYSFS_setWriteDir",reinterpret_cast<void*>(write_dir),reinterpret_cast<void**>(&original_write))||
       !patch("steam_api64.dll","SteamAPI_RunCallbacks",reinterpret_cast<void*>(callbacks),reinterpret_cast<void**>(&original_callbacks))||
       !patch("steam_api64.dll","SteamAPI_Shutdown",reinterpret_cast<void*>(shutdown),reinterpret_cast<void**>(&original_shutdown))) {
        OutputDebugStringA("error: framework bootstrap import installation failed; rollback\n");rollback();return false;
    }
    return true;
}
}
extern "C" void** mt2mp_wsock_resolve(unsigned index) {
    DWORD error=GetLastError();
    try {if(index>=std::size(exports))throw std::runtime_error("Invalid WSOCK32 export index");ensure_system();SetLastError(error);return &targets[index];}
    catch(const std::exception& cause){record("error",cause.what());RaiseFailFastException(nullptr,nullptr,0);return nullptr;}
}
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){self_module=module;if(!DisableThreadLibraryCalls(module))OutputDebugStringA("warn: framework loader thread notifications remain enabled\n");return bridges()?TRUE:FALSE;}
    if(reason==DLL_PROCESS_DETACH)stopped=true;return TRUE;
}
