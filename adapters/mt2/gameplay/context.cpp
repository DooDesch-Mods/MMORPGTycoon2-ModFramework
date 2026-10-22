#include "context.h"
#include "mt2mp/game/native_engine.h"
#include "crypto.h"
#include <windows.h>
#include <charconv>
#include <limits>

namespace mt2fw::mt2::gameplay {
namespace {
CodeVerifier verify_code;
constexpr const char* supported_game = "bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
DWORD lookup(uintptr_t entry,uint64_t uid,uintptr_t& result) {
    __try {result=reinterpret_cast<uintptr_t(*)(uint64_t)>(entry)(uid);}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    return 0;
}
}
void configure_code_verifier(CodeVerifier verifier){verify_code=std::move(verifier);}
Context context(uint64_t epoch) {
    Context result;result.epoch=epoch;std::string sha,error;
    if(!mt2mp::game::native::read_resolver_receipt(result.image,result.thread,sha,error))
        throw std::runtime_error("GAMEPLAY_RESOLVER: "+error);
    if(sha!=supported_game)throw std::runtime_error("GAMEPLAY_VERSION: unsupported game executable");
    result.require_world();return result;
}
void Context::require_world() const {
    if(!image||!thread||thread!=GetCurrentThreadId())throw std::runtime_error("GAMEPLAY_THREAD: requires the bound game thread");
    if(!epoch||!mt2mp::game::native::in_game())throw std::runtime_error("GAMEPLAY_WORLD: requires a loaded world");
}
void Context::require_mutation() const {
    require_world();mt2mp::game::native::WorldStatus state;std::string cause;
    if(!mt2mp::game::native::read_world_status(state,cause))throw std::runtime_error("GAMEPLAY_WORLD: "+cause);
    if(state.quitting||state.title_transition_pending||state.saves_in_progress!=0||state.edit_operations!=0||state.map_task_pending)
        throw std::runtime_error("GAMEPLAY_BUSY: native world transition/edit/save must be idle; edits="+std::to_string(state.edit_operations)+
            " saves="+std::to_string(state.saves_in_progress)+" map_task="+std::to_string(state.map_task_pending));
}
void Context::read_bytes(uintptr_t address,void* output,size_t bytes) const {
    if(!address||!output||!bytes||bytes>1024*1024||address>std::numeric_limits<uintptr_t>::max()-bytes)
        throw std::runtime_error("GAMEPLAY_READ: invalid bounded native memory range");
    SIZE_T actual=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),output,bytes,&actual)||actual!=bytes)
        throw std::runtime_error("GAMEPLAY_READ: native memory is unreadable, Win32="+std::to_string(GetLastError()));
}
std::string Context::text(uintptr_t address,size_t maximum) const {
    if(maximum>65536)throw std::runtime_error("GAMEPLAY_STRING: text budget exceeds 65536 bytes");
    auto data=read<uintptr_t>(address);auto length=read<uint64_t>(address+8);
    if(length>maximum)throw std::runtime_error("GAMEPLAY_STRING: native text exceeds the requested budget");
    std::string result(size_t(length),'\0');if(length)read_bytes(data,result.data(),size_t(length));
    if(result.find('\0')!=std::string::npos)throw std::runtime_error("GAMEPLAY_STRING: embedded NUL in native text");
    return result;
}
uintptr_t Context::singleton(uint64_t rva) const {
    if(rva>std::numeric_limits<uintptr_t>::max()-image)throw std::runtime_error("GAMEPLAY_SINGLETON: invalid RVA");
    auto value=read<uintptr_t>(image+rva);
    if(!value)throw std::runtime_error("GAMEPLAY_SINGLETON: required native owner is absent");
    return value;
}
void Context::verify_hash(uint64_t rva,size_t bytes,const std::string& sha) const {
    require_world();std::string cause;
    if(!verify_code)throw std::runtime_error("GAMEPLAY_CODE: trusted native code verifier unavailable");
    if(!verify_code(rva,bytes,sha,cause))throw std::runtime_error("GAMEPLAY_CODE: "+cause);
}
void Context::verify(uint64_t rva,std::initializer_list<unsigned char> bytes) const {
    verify_hash(rva,bytes.size(),digest(std::string(reinterpret_cast<const char*>(bytes.begin()),bytes.size())));
}
void Context::verify_original(uint64_t rva,size_t bytes) const {verify_hash(rva,bytes,"");}
uintptr_t Context::resolve(const Json& handle,uint64_t vtable) const {
    require_world();
    if(!handle.is_object()||!handle.contains("epoch")||!handle["epoch"].is_number_integer()||handle["epoch"].get<int64_t>()<0||handle["epoch"].get<uint64_t>()!=epoch||
       !handle.contains("uid")||!handle["uid"].is_string())throw std::runtime_error("GAMEPLAY_HANDLE: handle belongs to another world or has invalid identity");
    auto text=handle["uid"].get<std::string>();uint64_t uid=0;
    auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),uid);
    if(error!=std::errc{}||end!=text.data()+text.size()||!uid)throw std::runtime_error("GAMEPLAY_HANDLE: invalid exact object UID");
    verify(0x47ce20,{0x53,0x45,0x31,0xc0,0x48,0x85,0xc9});
    uintptr_t object=0;auto exception=lookup(image+0x47ce20,uid,object);
    if(exception)throw std::runtime_error("GAMEPLAY_HANDLE: UID registry raised SEH="+std::to_string(exception));
    if(!object)throw std::runtime_error("GAMEPLAY_HANDLE: object was deleted");
    if(vtable&&read<uintptr_t>(object)!=image+vtable)throw std::runtime_error("GAMEPLAY_HANDLE: native object type differs");
    return object;
}
}
