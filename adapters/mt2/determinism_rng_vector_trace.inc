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
    const auto ticket=begin(0x9fc540,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_vector2_max(source,limit);
    finish(ticket,result);return result;
}
using Vector2Bounds=uint64_t(*)(void*,const void*,const void*);
Vector2Bounds original_vector2_bounds=nullptr;
__declspec(noinline) uint64_t get_vector2_bounds(void* source,const void* low,const void* high){
    const auto ticket=begin(0x9fc7f0,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_vector2_bounds(source,low,high);
    finish(ticket,result);return result;
}
using Vector3Range=void*(*)(void*,void*,float,float);
Vector3Range original_vector3_range=nullptr;
__declspec(noinline) void* get_vector3_range(void* output,void* source,float low,float high){
    const auto ticket=begin(0x9fc8e0,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_vector3_range(output,source,low,high);
    finish_buffer(ticket,output,12);return result;
}
using Vector3Max=void*(*)(void*,void*,float);
Vector3Max original_vector3_max=nullptr;
__declspec(noinline) void* get_vector3_max(void* output,void* source,float limit){
    const auto ticket=begin(0x9fcb00,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_vector3_max(output,source,limit);
    finish_buffer(ticket,output,12);return result;
}
using Vector3Bounds=void*(*)(void*,void*,const void*,const void*);
Vector3Bounds original_vector3_bounds=nullptr;
__declspec(noinline) void* get_vector3_bounds(void* output,void* source,const void* low,const void* high){
    const auto ticket=begin(0x9fccc0,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_vector3_bounds(output,source,low,high);
    finish_buffer(ticket,output,12);return result;
}
using Vector2Box=uint64_t(*)(void*,const void*);
Vector2Box original_vector2_box=nullptr;
__declspec(noinline) uint64_t get_vector2_box(void* source,const void* box){
    const auto ticket=begin(0x9fce30,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_vector2_box(source,box);
    finish(ticket,result);return result;
}
using Vector3Box=void*(*)(void*,void*,const void*);
Vector3Box original_vector3_box=nullptr;
__declspec(noinline) void* get_vector3_box(void* output,void* source,const void* box){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto selected=select_source?select_source(0x9fcf20,caller,source):Selection{source};
    const auto ticket=begin(0x9fcf20,caller,selected.source);
    const auto result=original_vector3_box(output,selected.source,box);
    finish_buffer(ticket,output,12);if(selected.count)++selected.count->completed;return result;
}
using Color=void*(*)(void*,void*,float,float);
Color original_color=nullptr;
__declspec(noinline) void* get_color(void* output,void* source,float low,float high){
    const auto ticket=begin(0x9fd090,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_color(output,source,low,high);
    finish_buffer(ticket,output,16);return result;
}
using Vector3Cone=void*(*)(void*,void*,const void*,float);
Vector3Cone original_vector3_cone=nullptr;
__declspec(noinline) void* get_vector3_cone(void* output,void* source,const void* direction,float angle){
    const auto ticket=begin(0x9fd2f0,reinterpret_cast<uintptr_t>(_ReturnAddress()),source);
    const auto result=original_vector3_cone(output,source,direction,angle);
    finish_buffer(ticket,output,12);return result;
}
bool vector_consumers_installed=false;
void install_vector_consumers(){
    for(const auto& [offset,size,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,9>{{
        {0x9fc540,336,"eecbd3cb2b6c7142e5d21463585b165cbcaaa0c2499f485175c7015091673194"},
        {0x9fc7f0,240,"54e095e5bb5f6613c1bb91786e258c59b7142996db49e3099e144bc37ba40ec8"},
        {0x9fc8e0,544,"8783a9091768ffc5f55e2b39d5e4a83c6ed6f98d201ea628e16a4beb9fcbafeb"},
        {0x9fcb00,448,"424988fd3f3ec857c612f4392e0ec8a60b8c64454db1e1793ad3fe75ffd1b92e"},
        {0x9fccc0,368,"10228e9b1fea081fe076af6f307e8500b58400ff4e81ecac662ef21edbf27ad7"},
        {0x9fce30,240,"910b468e1fbfd0eba64ff063f8752af242ea09ac364597aa41310eee8b2d202c"},
        {0x9fcf20,368,"6a619b096d2c022f22f50d9bdda3254ae130b167dd2781eedf2d50cd2e821b65"},
        {0x9fd090,608,"c5cc6107c774310bd29707bf28d123600c396db1a7b943fe464c2e137c709c8e"},
        {0x9fd2f0,432,"05f261b26a492f81e1c2fbaaed9a2f4f84d92e35b8e37942cdb71be71468354a"}}}){
        std::string bytes(size,'\0');SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+offset),bytes.data(),size,&copied)||copied!=size||digest(bytes)!=sha)
            throw std::runtime_error("Default RNG vector consumer original bytes differ rva="+std::to_string(offset));
    }
    native_routes->catalog("framework.rng.vector2-max",0x9fc540,true);native_services->reserve("framework.rng.vector2-max");
    hook(reinterpret_cast<void*>(image+0x9fc540),reinterpret_cast<void*>(&get_vector2_max),reinterpret_cast<void**>(&original_vector2_max));
    native_routes->catalog("framework.rng.vector2-bounds",0x9fc7f0,true);native_services->reserve("framework.rng.vector2-bounds");
    hook(reinterpret_cast<void*>(image+0x9fc7f0),reinterpret_cast<void*>(&get_vector2_bounds),reinterpret_cast<void**>(&original_vector2_bounds));
    native_routes->catalog("framework.rng.vector3-range",0x9fc8e0,true);native_services->reserve("framework.rng.vector3-range");
    hook(reinterpret_cast<void*>(image+0x9fc8e0),reinterpret_cast<void*>(&get_vector3_range),reinterpret_cast<void**>(&original_vector3_range));
    native_routes->catalog("framework.rng.vector3-max",0x9fcb00,true);native_services->reserve("framework.rng.vector3-max");
    hook(reinterpret_cast<void*>(image+0x9fcb00),reinterpret_cast<void*>(&get_vector3_max),reinterpret_cast<void**>(&original_vector3_max));
    native_routes->catalog("framework.rng.vector3-bounds",0x9fccc0,true);native_services->reserve("framework.rng.vector3-bounds");
    hook(reinterpret_cast<void*>(image+0x9fccc0),reinterpret_cast<void*>(&get_vector3_bounds),reinterpret_cast<void**>(&original_vector3_bounds));
    native_routes->catalog("framework.rng.vector2-box",0x9fce30,true);native_services->reserve("framework.rng.vector2-box");
    hook(reinterpret_cast<void*>(image+0x9fce30),reinterpret_cast<void*>(&get_vector2_box),reinterpret_cast<void**>(&original_vector2_box));
    native_routes->catalog("framework.rng.vector3-box",0x9fcf20,true);native_services->reserve("framework.rng.vector3-box");
    hook(reinterpret_cast<void*>(image+0x9fcf20),reinterpret_cast<void*>(&get_vector3_box),reinterpret_cast<void**>(&original_vector3_box));
    native_routes->catalog("framework.rng.color",0x9fd090,true);native_services->reserve("framework.rng.color");
    hook(reinterpret_cast<void*>(image+0x9fd090),reinterpret_cast<void*>(&get_color),reinterpret_cast<void**>(&original_color));
    native_routes->catalog("framework.rng.vector3-cone",0x9fd2f0,true);native_services->reserve("framework.rng.vector3-cone");
    hook(reinterpret_cast<void*>(image+0x9fd2f0),reinterpret_cast<void*>(&get_vector3_cone),reinterpret_cast<void**>(&original_vector3_cone));
    vector_consumers_installed=true;
}
