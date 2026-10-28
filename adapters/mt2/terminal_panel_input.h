#pragma once
#include "terminal_panel_draw.h"
namespace mt2fw::mt2::terminal_panel {
struct Point {float x=0,y=0;};
struct InputNative {
    float (*aspect)(void*)=nullptr;
    void (*scale)(void*,const Point*)=nullptr;
    uint64_t (*apply)(const void*,const Point*)=nullptr;
    void* (*hit)(void*,void*,const Point*)=nullptr;
};
inline InputNative input_calls(uintptr_t image){return {
    reinterpret_cast<float(*)(void*)>(image+0x9ca480),
    reinterpret_cast<void(*)(void*,const Point*)>(image+0x9fef60),
    reinterpret_cast<uint64_t(*)(const void*,const Point*)>(image+0x9ff030),
    reinterpret_cast<void*(*)(void*,void*,const Point*)>(image+0x81b1a0)};}
inline uintptr_t hit(const Native& n,const InputNative& f,const Owner& o,float x,float y,int logical_width,int logical_height){
    require(f.aspect&&f.scale&&f.apply&&f.hit,"Terminal native input entrypoints are incomplete");
    const auto s=inspect(n,o);
    require(logical_width>0&&logical_height>0&&get<int32_t>(n,o.screen+0x54)==logical_width&&
        get<int32_t>(n,o.screen+0x58)==logical_height,"Terminal SDL and native input dimensions differ");
    require(std::isfinite(x)&&std::isfinite(y)&&x>=0&&y>=0&&x<logical_width&&y<logical_height,"Terminal pointer lies outside its native window");
    const auto cursor=get<uintptr_t>(n,o.image+0x13df9d0);
    require(cursor&&get<uintptr_t>(n,cursor)==o.image+0x1786dd0,"Terminal Cursor receiver changed");
    alignas(16) std::array<unsigned char,0x64> transform{};n.read(s.camera+8,transform.data(),transform.size());
    const float half=get<float>(n,s.camera+0x68)*0.5f,aspect=f.aspect(reinterpret_cast<void*>(o.screen));
    require(std::isfinite(half)&&half>0&&std::isfinite(aspect)&&aspect>0,"Terminal native camera scale is invalid");
    const Point scale{half*aspect,half};f.scale(transform.data(),&scale);
    const Point normalized{2.f*x/logical_width-1.f,2.f*y/logical_height-1.f};
    const auto packed=f.apply(transform.data(),&normalized);Point local;std::memcpy(&local,&packed,sizeof(local));
    const auto origin=get<Point>(n,o.panel+0x110);local.x-=origin.x;local.y-=origin.y;
    require(std::isfinite(local.x)&&std::isfinite(local.y),"Terminal native pointer transform is non-finite");
    return reinterpret_cast<uintptr_t>(f.hit(reinterpret_cast<void*>(o.panel),reinterpret_cast<void*>(cursor),&local));
}
struct Click {
    uintptr_t panel=0,pane=0;
    uint64_t lease=0;
    void cancel(){panel=pane=0;lease=0;}
    void down(uint64_t selected,uintptr_t window,uintptr_t target){if(!pane){lease=selected;panel=window;pane=target;}}
    bool up(uint64_t selected,uintptr_t window,uintptr_t target){
        const bool accepted=target&&selected==lease&&window==panel&&target==pane;cancel();return accepted;
    }
};
}
