// Private observation only: count original Default scalar-helper entries/returns.
namespace rng_trace {
constexpr size_t capacity=512,none=capacity;
struct Row {uintptr_t helper=0,caller=0;DWORD thread=0;uint64_t attempted=0,completed=0,result_hash=14695981039346656037ull;bool stack_attempted=false;uint16_t stack_count=0;std::array<void*,16> stack{};};
std::array<Row,capacity> rows{};
size_t used=0;
std::mutex count_mutex;
std::atomic<bool> recording=false,counter_failed=false;
std::atomic<uint64_t> dropped=0;
bool installed=false,started=false;
DWORD main_thread=0;
using Selection=mt2::determinism::presentation::Selection;
Selection(*select_source)(uintptr_t,uintptr_t,void*)=nullptr;
using Next=uint64_t(*)(void*);
using Int=int(*)(void*,int);
using IntRange=int(*)(void*,int,int);
using FloatRange=float(*)(void*,float,float);
Next original_next=nullptr;
Int original_int=nullptr;
IntRange original_int_range=nullptr;
FloatRange original_float_range=nullptr;
// Exact native ABI: this in RCX, floats in XMM1/XMM2, packed XY in RAX.
using Vector2Range=uint64_t(*)(void*,float,float);
Vector2Range original_vector2_range=nullptr;

size_t begin(uintptr_t helper,uintptr_t caller,void* source) noexcept {
    if(!recording.load(std::memory_order_acquire)||reinterpret_cast<uintptr_t>(source)!=image+0x1c557a0)return none;
    try{
        const auto current_thread=GetCurrentThreadId();std::lock_guard lock(count_mutex);
        if(!recording.load(std::memory_order_acquire))return none;
        for(size_t i=0;i<used;++i)if(rows[i].helper==helper&&rows[i].caller==caller&&rows[i].thread==current_thread){++rows[i].attempted;return i;}
        if(used==capacity){dropped.fetch_add(1,std::memory_order_relaxed);return none;}
        const auto index=used++;rows[index]={helper,caller,current_thread,1,0};
        // One bounded diagnostic unwind for each known main-thread gameplay
        // caller. Zero frames or a full buffer do not prove a complete stack.
        if(current_thread==main_thread&&helper==0xa06a60&&(caller==image+0x38e878||caller==image+0x3cf984)){
            auto& row=rows[index];row.stack_attempted=true;
            row.stack_count=CaptureStackBackTrace(0,16,row.stack.data(),nullptr);
        }
        return index;
    }catch(...){counter_failed.store(true,std::memory_order_release);return none;}
}
template<class T>void finish(size_t ticket,T result) noexcept {
    if(ticket==none)return;
    uint64_t bits=0;
    if constexpr(std::is_same_v<T,float>)bits=std::bit_cast<uint32_t>(result);
    else if constexpr(std::is_same_v<T,int>)bits=static_cast<uint32_t>(result);
    else {static_assert(std::is_same_v<T,uint64_t>);bits=result;}
    try{std::lock_guard lock(count_mutex);auto& row=rows[ticket];++row.completed;
        for(size_t i=0;i<sizeof(T);++i){row.result_hash^=(bits>>(8*i))&255;row.result_hash*=1099511628211ull;}}
    catch(...){counter_failed.store(true,std::memory_order_release);}
}
template<class Function,class... Args>auto invoke(uintptr_t helper,uintptr_t caller,void* source,Function original,Args... args){
    const auto selected=select_source?select_source(helper,caller,source):Selection{source};
    const auto ticket=begin(helper,caller,selected.source);
    auto result=original(selected.source,args...);
    finish(ticket,result);if(selected.count)++selected.count->completed;return result;
}
__declspec(noinline) uint64_t next(void* source){return invoke(0xa06600,reinterpret_cast<uintptr_t>(_ReturnAddress()),source,original_next);}
__declspec(noinline) int get_int(void* source,int limit){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto selected=select_source?select_source(0xa068a0,caller,source):Selection{source};
    auto* service=native_services.get();
    mt2::NativeServices::CharacterPreviewLease lease{nullptr,0,selected.source};
    int result;
    // POD SEH cleanup protects the borrowed source through native execution and
    // trace completion, including exceptions that do not unwind /EHsc objects.
    __try{
        if(service)service->character_preview_select(caller,selected.source,limit,&lease);
        const auto ticket=begin(0xa068a0,caller,lease.source);
        result=original_int(lease.source,limit);
        finish(ticket,result);if(selected.count)++selected.count->completed;
    }__finally{if(service)service->character_preview_release(&lease);}
    return result;
}
__declspec(noinline) int get_int_range(void* source,int low,int high){return invoke(0xa06920,reinterpret_cast<uintptr_t>(_ReturnAddress()),source,original_int_range,low,high);}
__declspec(noinline) float get_float_range(void* source,float low,float high){return invoke(0xa06a60,reinterpret_cast<uintptr_t>(_ReturnAddress()),source,original_float_range,low,high);}
__declspec(noinline) uint64_t get_vector2_range(void* source,float low,float high){return invoke(0xa06cb0,reinterpret_cast<uintptr_t>(_ReturnAddress()),source,original_vector2_range,low,high);}
#include "determinism_rng_vector_trace.inc"
void start(){
    if(!installed)return;
    if(started)throw std::runtime_error("Default RNG loading trace already started; no reset/replay");
    main_thread=GetCurrentThreadId();started=true;recording.store(true,std::memory_order_release);
}
void stop(){std::lock_guard lock(count_mutex);recording.store(false,std::memory_order_release);}
Json status(){
    Json entries=Json::array();uint64_t attempted=0,completed=0;
    {std::lock_guard lock(count_mutex);for(size_t i=0;i<used;++i){const auto& row=rows[i];attempted+=row.attempted;completed+=row.completed;
        Json entry={{"helper_rva",row.helper},{"return_rva",row.caller>=image&&row.caller-image<0x2000000?Json(row.caller-image):Json(nullptr)},
            {"return_address",row.caller},{"thread_id",row.thread},{"main_thread",row.thread==main_thread},{"attempted",row.attempted},{"completed",row.completed},{"result_hash",row.result_hash}};
        if(row.stack_attempted){
            Json frames=Json::array();
            for(size_t j=0;j<row.stack_count&&j<row.stack.size();++j){const auto address=reinterpret_cast<uintptr_t>(row.stack[j]);
                frames.push_back({{"address",address},{"game_rva",address>=image&&address-image<0x2000000?Json(address-image):Json(nullptr)}});}
            entry["first_call_stack"]={{"attempted",true},{"count",row.stack_count},{"capacity",row.stack.size()},
                {"unavailable",row.stack_count==0},{"capacity_reached",row.stack_count==row.stack.size()},
                {"complete",false},{"coverage","Windows unwind diagnostic; zero is unavailable; capacity reached may truncate; omitted/unwind-unavailable frames are not proven absent"},{"frames",frames}};
        }
        entries.push_back(std::move(entry));}}
    return {{"installed",installed},{"started",started},{"recording",recording.load()},{"attempted",attempted},{"completed",completed},
        {"dropped",dropped.load()},{"counter_failed",counter_failed.load()},{"complete_helper_receipts",installed&&started&&!recording.load()&&attempted==completed&&!dropped.load()&&!counter_failed.load()},
        {"result_hash_method","FNV1a64 over raw little-endian result bytes in per-thread row completion order; diagnostic only, not a global order or collision-free proof"},{"vector_consumers_installed",vector_consumers_installed},{"vector2_range_installed",original_vector2_range!=nullptr},{"coverage","Default Next and scalar helpers; Vector2/Vector3/Color/Cone overloads when vector_consumers_installed, with Vector2(float,float) separately reported by vector2_range_installed; caller-inlined transitions and direct state writes outside these bodies remain uncovered; helper counts are not total RNG advances"},{"entries",entries}};
}
void install(const Json& config){
    if(!config.value("rng_trace",false))return;
    if(!config.value("presentation_rng",false))throw std::runtime_error("Default RNG trace requires existing GetFloat(max) presentation hook");
    for(const auto& [offset,size,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,4>{{
        {0xa06600,112,"8716222dfb80c046bcf5700b173550d5be11e771f671ae9c8a59c7a5df54c7b2"},
        {0xa068a0,128,"a645122dbd7b9160fab9fccaa2461f116379b2119afe6846e37ebffb552e7b18"},
        {0xa06920,144,"44a19e7249e82770e22d836100f85541c508f06f67153876107fdb0f568e7ae4"},
        {0xa06a60,208,"9f311cc5ad27b95c15e4c78e0baae84f3bf68e988f0ac10e682b01d6399f29d9"}}}){
        std::string bytes(size,'\0');SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+offset),bytes.data(),size,&copied)||copied!=size||digest(bytes)!=sha)
            throw std::runtime_error("Default RNG trace original helper bytes differ rva="+std::to_string(offset));
    }
    hook(reinterpret_cast<void*>(image+0xa06600),reinterpret_cast<void*>(&next),reinterpret_cast<void**>(&original_next));
    hook(reinterpret_cast<void*>(image+0xa068a0),reinterpret_cast<void*>(&get_int),reinterpret_cast<void**>(&original_int));
    hook(reinterpret_cast<void*>(image+0xa06920),reinterpret_cast<void*>(&get_int_range),reinterpret_cast<void**>(&original_int_range));
    hook(reinterpret_cast<void*>(image+0xa06a60),reinterpret_cast<void*>(&get_float_range),reinterpret_cast<void**>(&original_float_range));
    if(config.value("digital_rng",false)){
        std::string bytes(416,'\0');SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+0xa06cb0),bytes.data(),bytes.size(),&copied)||copied!=bytes.size()||digest(bytes)!="2ae9be85725eab4521a2a0c1973d003ee5c3a59b30c77ae4561d8e89d5ac37f1")
            throw std::runtime_error("Digital RNG vector helper original bytes differ");
        hook(reinterpret_cast<void*>(image+0xa06cb0),reinterpret_cast<void*>(&get_vector2_range),reinterpret_cast<void**>(&original_vector2_range));
    }
    install_vector_consumers();
    installed=true;
}
}
