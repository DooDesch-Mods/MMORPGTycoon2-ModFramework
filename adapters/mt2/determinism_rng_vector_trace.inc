// These native consumers inline xoshiro writes instead of calling Next.
// Packed Vector2 returns in RAX. Vector3/Color use RCX output, RDX source,
// shifted remaining arguments and return the original output pointer in RAX.
// Observation does not select a different source or replace any native body.
void finish_buffer(size_t ticket,const void* output,size_t bytes) noexcept {
    if(ticket==none)return;
    try{std::lock_guard lock(count_mutex);auto& row=rows[ticket];++row.completed;
        const auto* data=static_cast<const uint8_t*>(output);
        for(size_t i=0;i<bytes;++i){row.result_hash^=data[i];row.result_hash*=1099511628211ull;}}
    catch(...){counter_failed.store(true,std::memory_order_release);}
}
using Vector2Max=uint64_t(*)(void*,float);
Vector2Max original_vector2_max=nullptr;
__declspec(noinline) uint64_t get_vector2_max(void* source,float limit){
    const auto ticket=begin(0xa06b30,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_vector2_max(source,limit);
    finish(ticket,result);return result;
}
using Vector2Bounds=uint64_t(*)(void*,const void*,const void*);
Vector2Bounds original_vector2_bounds=nullptr;
__declspec(noinline) uint64_t get_vector2_bounds(void* source,const void* low,const void* high){
    const auto ticket=begin(0xa06e50,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_vector2_bounds(source,low,high);
    finish(ticket,result);return result;
}
using Vector3Range=void*(*)(void*,void*,float,float);
Vector3Range original_vector3_range=nullptr;
__declspec(noinline) void* get_vector3_range(void* output,void* source,float low,float high){
    const auto ticket=begin(0xa06fe0,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_vector3_range(output,source,low,high);
    finish_buffer(ticket,output,12);return result;
}
using Vector3Max=void*(*)(void*,void*,float);
Vector3Max original_vector3_max=nullptr;
__declspec(noinline) void* get_vector3_max(void* output,void* source,float limit){
    const auto ticket=begin(0xa072c0,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_vector3_max(output,source,limit);
    finish_buffer(ticket,output,12);return result;
}
using Vector3Bounds=void*(*)(void*,void*,const void*,const void*);
Vector3Bounds original_vector3_bounds=nullptr;
__declspec(noinline) void* get_vector3_bounds(void* output,void* source,const void* low,const void* high){
    const auto ticket=begin(0xa07530,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_vector3_bounds(output,source,low,high);
    finish_buffer(ticket,output,12);return result;
}
using Vector2Box=uint64_t(*)(void*,const void*);
Vector2Box original_vector2_box=nullptr;
__declspec(noinline) uint64_t get_vector2_box(void* source,const void* box){
    const auto ticket=begin(0xa07780,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_vector2_box(source,box);
    finish(ticket,result);return result;
}
using Vector3Box=void*(*)(void*,void*,const void*);
Vector3Box original_vector3_box=nullptr;
__declspec(noinline) void* get_vector3_box(void* output,void* source,const void* box){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto selected=select_source?select_source(0xa078e0,caller,source):Selection{source};
    const auto ticket=begin(0xa078e0,caller,selected.source);
    const auto result=original_vector3_box(output,selected.source,box);
    finish_buffer(ticket,output,12);if(selected.count)++selected.count->completed;return result;
}
using Color=void*(*)(void*,void*,float,float);
Color original_color=nullptr;
__declspec(noinline) void* get_color(void* output,void* source,float low,float high){
    const auto ticket=begin(0xa07b60,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_color(output,source,low,high);
    finish_buffer(ticket,output,16);return result;
}
using Vector3Cone=void*(*)(void*,void*,const void*,float);
Vector3Cone original_vector3_cone=nullptr;
__declspec(noinline) void* get_vector3_cone(void* output,void* source,const void* direction,float angle){
    const auto ticket=begin(0xa07e60,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_vector3_cone(output,source,direction,angle);
    finish_buffer(ticket,output,12);return result;
}
bool vector_consumers_installed=false;
void install_vector_consumers(){
    for(const auto& [offset,size,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,9>{{
        {0xa06b30,384,"af8022ace9b9d186975edadb9b4fec491360b5f71b14b6526ce8c4574c67d486"},
        {0xa06e50,400,"154613813003df2864b74088907346110af31dd6661cc9d9d5a184cd236a3d78"},
        {0xa06fe0,736,"85de43e5c49a072b4577a2166d8629aa66b41f3d9ba8269a7f17d9a2347a5115"},
        {0xa072c0,624,"1dcc9ee3bb9a29c1cb2f772809a7dc959363d66e9882dca12655d6fc37881df5"},
        {0xa07530,592,"ee59c3c14529bb0de6caf5f900043f13f8824c12ceb461e3c8e03cf408f40974"},
        {0xa07780,352,"46dd461e0938f38861160d8f0b44abd68620d46e6fa3b6ba0abd1120aaf45277"},
        {0xa078e0,640,"69d91d7416bbd9414853da8700b4b1b7f82a329a3e1753b0d881fb3c401446f0"},
        {0xa07b60,768,"efad788a044c2cd9685ae43425160ebc9b3a274f6bd842cfe5157fc4a25852f4"},
        {0xa07e60,512,"ed1cad49cc75bbfac58f74e84e81f8b97f22114cdac7fe30d74e653498d9ec8c"}}}){
        std::string bytes(size,'\0');SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+offset),bytes.data(),size,&copied)||copied!=size||digest(bytes)!=sha)
            throw std::runtime_error("Default RNG vector consumer original bytes differ rva="+std::to_string(offset));
    }
    native_routes->catalog("framework.rng.vector2-max",0xa06b30,true);native_services->reserve("framework.rng.vector2-max");
    hook(reinterpret_cast<void*>(image+0xa06b30),reinterpret_cast<void*>(&get_vector2_max),reinterpret_cast<void**>(&original_vector2_max));
    native_routes->catalog("framework.rng.vector2-bounds",0xa06e50,true);native_services->reserve("framework.rng.vector2-bounds");
    hook(reinterpret_cast<void*>(image+0xa06e50),reinterpret_cast<void*>(&get_vector2_bounds),reinterpret_cast<void**>(&original_vector2_bounds));
    native_routes->catalog("framework.rng.vector3-range",0xa06fe0,true);native_services->reserve("framework.rng.vector3-range");
    hook(reinterpret_cast<void*>(image+0xa06fe0),reinterpret_cast<void*>(&get_vector3_range),reinterpret_cast<void**>(&original_vector3_range));
    native_routes->catalog("framework.rng.vector3-max",0xa072c0,true);native_services->reserve("framework.rng.vector3-max");
    hook(reinterpret_cast<void*>(image+0xa072c0),reinterpret_cast<void*>(&get_vector3_max),reinterpret_cast<void**>(&original_vector3_max));
    native_routes->catalog("framework.rng.vector3-bounds",0xa07530,true);native_services->reserve("framework.rng.vector3-bounds");
    hook(reinterpret_cast<void*>(image+0xa07530),reinterpret_cast<void*>(&get_vector3_bounds),reinterpret_cast<void**>(&original_vector3_bounds));
    native_routes->catalog("framework.rng.vector2-box",0xa07780,true);native_services->reserve("framework.rng.vector2-box");
    hook(reinterpret_cast<void*>(image+0xa07780),reinterpret_cast<void*>(&get_vector2_box),reinterpret_cast<void**>(&original_vector2_box));
    native_routes->catalog("framework.rng.vector3-box",0xa078e0,true);native_services->reserve("framework.rng.vector3-box");
    hook(reinterpret_cast<void*>(image+0xa078e0),reinterpret_cast<void*>(&get_vector3_box),reinterpret_cast<void**>(&original_vector3_box));
    native_routes->catalog("framework.rng.color",0xa07b60,true);native_services->reserve("framework.rng.color");
    hook(reinterpret_cast<void*>(image+0xa07b60),reinterpret_cast<void*>(&get_color),reinterpret_cast<void**>(&original_color));
    native_routes->catalog("framework.rng.vector3-cone",0xa07e60,true);native_services->reserve("framework.rng.vector3-cone");
    hook(reinterpret_cast<void*>(image+0xa07e60),reinterpret_cast<void*>(&get_vector3_cone),reinterpret_cast<void**>(&original_vector3_cone));
    vector_consumers_installed=true;
}
