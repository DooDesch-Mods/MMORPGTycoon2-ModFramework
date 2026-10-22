// Copyright (c) 2026 DooDesch. Original standalone adapter under the MIT license.
// Native ABI evidence: Multiplayer/src/game/scenery_stroke_calls.h.
struct StrokePoint {int64_t x,z;};
struct StrokeView {uintptr_t begin=0,end=0,capacity=0;};
struct StrokeTransfer {uintptr_t image=0,data=0;int32_t count=0;bool transferred=false;uint32_t cause=0;};
struct StrokePattern {const uintptr_t* ap;StrokeTransfer* transfer;};
struct StrokeArray {uintptr_t ap,data;int32_t count,capacity;};
void stroke_pattern_destroy(StrokePattern* p){p->transfer->cause=0xe0030003;}
void stroke_pattern_generate(StrokePattern* p,StrokeArray* out,const void*){
    auto& t=*p->transfer;__try{
        if(t.transferred || out->ap!=t.image+0x1746400 || out->count<0 || out->capacity<out->count || out->capacity>1000000 || (!out->data && out->capacity)){t.cause=0xe0030004;return;}
        if(out->data)reinterpret_cast<void(*)(uintptr_t)>(t.image+0xa01280)(out->data);
        out->data=t.data;out->count=t.count;out->capacity=t.count;t.data=0;t.transferred=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){t.cause=GetExceptionCode();}
}
const std::array<uintptr_t,3> stroke_pattern_methods{reinterpret_cast<uintptr_t>(&stroke_pattern_destroy),reinterpret_cast<uintptr_t>(&stroke_pattern_destroy),reinterpret_cast<uintptr_t>(&stroke_pattern_generate)};
struct StrokeStamp {std::vector<std::vector<StrokePoint>> paths;std::vector<std::array<float,10>> dots;alignas(16) unsigned char shape[0x70]{};bool made=false;StrokeTransfer transfer;};
struct StrokeDraft {uintptr_t image=0,receiver=0,options=0;bool brush=false,ready=false,options_ready=false,finished=false;};
struct StrokeText {const char* data;size_t size;unsigned char rest[16]{};};
uint32_t stroke_catalog(uintptr_t image,uintptr_t manager,const StrokeText* file,int32_t& category,int32_t& variant,uintptr_t& directory,uintptr_t& filename,uint64_t& packed){
    __try{packed=reinterpret_cast<uint64_t(*)(uintptr_t,const void*)>(image+0x2cedb0)(manager,file);category=int32_t(uint32_t(packed));variant=int32_t(uint32_t(packed>>32));if(category<0 || category>=47)return ERROR_NOT_FOUND;
        directory=reinterpret_cast<uintptr_t(*)(int32_t)>(image+0x448660)(category);if(!reinterpret_cast<bool(*)(uintptr_t,uintptr_t,int32_t)>(image+0x2cead0)(manager,directory,variant))return ERROR_NOT_FOUND;
        filename=reinterpret_cast<uintptr_t(*)(uintptr_t,uintptr_t,int32_t)>(image+0x2ceb40)(manager,directory,variant);if(reinterpret_cast<uint64_t(*)(uintptr_t,uintptr_t)>(image+0x2cedb0)(manager,filename)!=packed)return ERROR_INVALID_DATA;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t stroke_directory(uintptr_t image,int32_t category,uintptr_t& out){__try{out=reinterpret_cast<uintptr_t(*)(int32_t)>(image+0x448660)(category);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t stroke_construct(StrokeDraft& d,const StrokeText* type,uintptr_t region,int32_t mode,int32_t category,int32_t variant,bool replace,const float* bounds){
    __try{d.receiver=reinterpret_cast<uintptr_t(*)(size_t)>(d.image+0xa02670)(d.brush?0xc58:0x5c8);if(!d.receiver)return ERROR_NOT_ENOUGH_MEMORY;
        reinterpret_cast<void(*)(uintptr_t,const void*)>(d.image+(d.brush?0x5d0770:0x5d63b0))(d.receiver,type);d.ready=true;
        if(d.brush){d.options=reinterpret_cast<uintptr_t(*)(size_t)>(d.image+0xa02670)(0xd0);if(!d.options)return ERROR_NOT_ENOUGH_MEMORY;reinterpret_cast<void(*)(uintptr_t)>(d.image+0x5fcb00)(d.options);d.options_ready=true;
            *reinterpret_cast<uint8_t*>(d.options+0x60)=uint8_t(replace);*reinterpret_cast<uintptr_t*>(d.receiver+0xb68)=d.options;*reinterpret_cast<uintptr_t*>(d.receiver+0xb60)=region;
            *reinterpret_cast<int32_t*>(d.receiver+0x174)=mode;*reinterpret_cast<int32_t*>(d.receiver+0x178)=category;*reinterpret_cast<int32_t*>(d.receiver+0x17c)=variant;std::memcpy(reinterpret_cast<void*>(d.receiver+0x894),bounds,24);
        }else *reinterpret_cast<uintptr_t*>(d.receiver+0x588)=region;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t stroke_shape(uintptr_t image,StrokeStamp& s,const StrokeView* paths){
    __try{reinterpret_cast<void(*)(void*)>(image+0xf1810)(s.shape);s.made=true;reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0xf7ed0)(*reinterpret_cast<uintptr_t*>(s.shape+0x18),paths);reinterpret_cast<void(*)(void*)>(image+0xf3b60)(s.shape);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t stroke_dots(StrokeStamp& s,uintptr_t image){
    __try{s.transfer.image=image;s.transfer.count=int32_t(s.dots.size());if(s.dots.empty())return 0;s.transfer.data=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa026c0)(s.dots.size()*0x6c);if(!s.transfer.data)return ERROR_NOT_ENOUGH_MEMORY;
        for(size_t n=0;n<s.dots.size();++n)reinterpret_cast<void(*)(uintptr_t,const void*,const void*,const void*)>(image+0x9ff370)(s.transfer.data+n*0x6c,s.dots[n].data(),s.dots[n].data()+4,s.dots[n].data()+7);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t stroke_paint(StrokeDraft& d,StrokeStamp& s,bool inject){
    uintptr_t previous=0;StrokePattern pattern{stroke_pattern_methods.data(),&s.transfer};
    __try{auto* slot=reinterpret_cast<uintptr_t*>(d.receiver+0xbc0);if(inject){previous=*slot;*slot=reinterpret_cast<uintptr_t>(&pattern);}
        __try{reinterpret_cast<void(*)(uintptr_t,const void*)>(d.image+(d.brush?0x5d5070:0x5d7550))(d.receiver,s.shape);}__finally{if(inject)*slot=previous;}
        return inject?s.transfer.cause:0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t stroke_begin(StrokeDraft& d,uint32_t& cleanup){
    alignas(16) unsigned char label[0x20]{};bool ready=false;uint32_t code=0;__try{if(d.brush && reinterpret_cast<bool(*)()>(d.image+0x264c30)()){
        auto scope=reinterpret_cast<uintptr_t(*)(size_t)>(d.image+0xa02670)(1);if(!scope)return ERROR_NOT_ENOUGH_MEMORY;*reinterpret_cast<uint8_t*>(scope)=0;*reinterpret_cast<uintptr_t*>(d.receiver+0x360)=scope;
        reinterpret_cast<void(*)(void*,const char*)>(d.image+0x5c04b0)(label,"Painting scenery");ready=true;reinterpret_cast<void(*)(const void*)>(d.image+0x265300)(label);*reinterpret_cast<uint8_t*>(scope)=1;}}
    __except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}if(ready){__try{reinterpret_cast<void(*)(void*)>(d.image+0x11bd1e0)(label);}__except(EXCEPTION_EXECUTE_HANDLER){cleanup=GetExceptionCode();}}return code;
}
uint32_t stroke_finish(StrokeDraft& d){__try{reinterpret_cast<void(*)(uintptr_t)>(d.image+(d.brush?0x5cfdd0:0x5d68f0))(d.receiver);d.finished=true;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t stroke_position(uintptr_t cursor,const float* position){__try{std::memcpy(reinterpret_cast<void*>(cursor+0x1e4),position,12);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t stroke_shape_destroy(uintptr_t image,StrokeStamp& s){__try{reinterpret_cast<void(*)(void*)>(image+0xf15a0)(s.shape);s.made=false;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t stroke_transfer_destroy(StrokeTransfer& t){__try{reinterpret_cast<void(*)(uintptr_t)>(t.image+0xa01280)(t.data);t.data=0;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t stroke_cursor_destroy(StrokeDraft& d){__try{reinterpret_cast<void(*)(uintptr_t)>(d.image+(d.brush?0x5d0fc0:0x5d72d0))(d.receiver);d.ready=false;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t stroke_options_destroy(StrokeDraft& d){__try{reinterpret_cast<void(*)(uintptr_t)>(d.image+0xbefc50)(d.options);d.options_ready=false;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
