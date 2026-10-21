#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

namespace mt2fw::mt2 {
// Exact supported MT2 layout. This diagnostic reads fields without calling game code.
struct ClickNativeReadback {
    bool input_read=false,cursor_read=false,screen_read=false;
    std::array<float,2> input_xy{},cursor_ui{},cursor_world_screen{};
    std::array<float,3> cursor_world_pick{};
    std::array<int32_t,2> screen_size{};
    std::array<uint8_t,3> restrictions{};
    std::array<uint8_t,2> input_edges{},cursor_down_drag{};
    uint8_t cursor_enabled=0,input_gate=0,trap_gate=0,mouse_on_screen=0;
    bool trap=false,pressed_window=false,focus_window=false,capture_window=false;
    uint32_t pressed_window_class=0,focus_window_class=0,capture_window_class=0;
    std::string cause;
};

template<class Read>
ClickNativeReadback read_click_native(uintptr_t image,Read&& read,bool input_only=false){
    ClickNativeReadback out;
    auto field=[&]<class T>(uintptr_t object,size_t offset,T& value,const char* name){
        if(!object||object>UINTPTR_MAX-offset||object+offset>UINTPTR_MAX-(sizeof(T)-1)){
            out.cause=std::string("Click native read address invalid: ")+name;return false;
        }
        std::string error;
        if(!read(object+offset,&value,sizeof(T),error)){
            out.cause=std::string("Click native read failed: ")+name+": "+error;return false;
        }
        return true;
    };
    auto finite=[&](const auto& values,const char* name){
        for(auto value:values)if(!std::isfinite(value)){out.cause=std::string("Click native non-finite field: ")+name;return false;}
        return true;
    };
    if(!image||image>UINTPTR_MAX-0x1d00000){out.cause="Click native image range invalid";return out;}
    uintptr_t input=0,ap=0,axes=0,cursor=0,screen=0;
    int32_t count=0;
    if(!field(image,0x13df990,input,"input singleton")||!field(input,0,ap,"input class"))return out;
    if(ap!=image+0x1749430){out.cause="Click native input class differs";return out;}
    if(!field(input,0x7c,out.input_xy,"input normalized XY")||!finite(out.input_xy,"input normalized XY")||
       !field(input,0x50,axes,"input axes")||!field(input,0x58,count,"input axis count"))return out;
    if(count<=27||count>512){out.cause="Click native input axis count outside required 28..512";return out;}
    if(!field(axes,27*0x70+0x60,out.input_edges,"left pressed/released"))return out;
    if(!field(input,0x124,out.mouse_on_screen,"mouse on screen"))return out;
    if(out.mouse_on_screen>1){out.cause="Click native mouse-on-screen byte is not Boolean";return out;}
    out.input_read=true;
    if(input_only)return out;
    if(!field(image,0x1c4e3a0,screen,"screen singleton")||!field(screen,0x54,out.screen_size,"screen dimensions"))return out;
    out.screen_read=true;
    if(!field(image,0x13df9d0,cursor,"cursor singleton")||!field(cursor,0,ap,"cursor class"))return out;
    if(ap!=image+0x1786dd0){out.cause="Click native cursor class differs";return out;}
    uintptr_t trap=0,pressed_window=0,focus_window=0,capture=0;
    if(!field(cursor,0x1f0,out.cursor_ui,"cursor UI XY")||
       !field(cursor,0x1f8,out.cursor_world_screen,"cursor world-screen XY")||
       !field(cursor,0x1e4,out.cursor_world_pick,"cursor world pick")||
       !field(cursor,0x174,out.restrictions,"cursor UI restrictions")||
       !field(cursor,0x214,out.cursor_down_drag,"cursor down/drag")||
       !field(cursor,0x257,out.cursor_enabled,"cursor enabled")||
       !field(cursor,0x256,out.input_gate,"cursor input gate")||
       !field(cursor,0x1e0,out.trap_gate,"cursor trap gate")||
       !field(cursor,0x1d8,trap,"cursor trap")||
       !field(cursor,0x180,capture,"cursor capture window")||
       !field(cursor,0x1c0,pressed_window,"cursor pressed window")||
       !field(cursor,0x1c8,focus_window,"cursor focus window"))return out;
    out.trap=trap!=0;out.pressed_window=pressed_window!=0;out.focus_window=focus_window!=0;out.capture_window=capture!=0;
    auto pane_class=[&](uintptr_t pane,uint32_t& rva,const char* name){
        if(!pane)return true;
        uintptr_t table=0;if(!field(pane,0,table,name))return false;
        if(table<image||table-image>=0x1d00000){out.cause=std::string("Click native class outside image: ")+name;return false;}
        rva=static_cast<uint32_t>(table-image);return true;
    };
    if(!pane_class(pressed_window,out.pressed_window_class,"pressed window class")||
       !pane_class(focus_window,out.focus_window_class,"focus window class")||
       !pane_class(capture,out.capture_window_class,"capture window class")||
       !finite(out.cursor_ui,"cursor UI XY")||!finite(out.cursor_world_screen,"cursor world-screen XY")||
       !finite(out.cursor_world_pick,"cursor world pick"))return out;
    out.cursor_read=true;return out;
}
}
