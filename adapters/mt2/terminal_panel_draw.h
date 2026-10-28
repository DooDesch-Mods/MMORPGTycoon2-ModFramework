#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <set>

namespace mt2fw::mt2::terminal_panel {
// Storage and call signatures belong to the pinned MT2 executable. No native
// container or string is constructed by the plugin's C++ runtime.
struct alignas(16) Queue { std::array<unsigned char,0x5d0> bytes{}; };
struct alignas(16) DisplayList { std::array<unsigned char,0x260> bytes{}; };
struct alignas(16) Matrix { std::array<float,16> values{}; };
struct Owner {
    uintptr_t image=0,game=0,mode=0,panel=0,registered_panel=0,screen=0,window=0;
    uint64_t thread=0,current_thread=0;
    int width=0,height=0,mode_index=3;
};
struct Native {
    std::function<void(uintptr_t,void*,size_t)> read;
    void (*queue_construct)(void*)=nullptr;
    void (*queue_destroy)(void*)=nullptr;
    void (*list_construct)(void*,uint64_t,bool)=nullptr;
    void (*list_destroy)(void*)=nullptr;
    void (*start)(void*,void*,int)=nullptr;
    void (*projection)(void*,void*)=nullptr;
    void (*set_projection)(void*,const void*)=nullptr;
    void (*set_view)(void*,const void*)=nullptr;
    void (*window_draw)(void*,void*)=nullptr;
    void (*queue_draw)(void*,void*)=nullptr;
    void (*end)(void*)=nullptr;
    void (*clear_state)(void*)=nullptr;
    void (*set_target)(void*,void*)=nullptr;
    void (*render)(void*,void*)=nullptr;
    void (*present)(void*)=nullptr;
    bool ready()const{return read&&queue_construct&&queue_destroy&&list_construct&&list_destroy&&start&&projection&&set_projection&&set_view&&window_draw&&queue_draw&&end&&clear_state&&set_target&&render&&present;}
};
inline Native native_calls(uintptr_t image,std::function<void(uintptr_t,void*,size_t)> read){
    Native n;n.read=std::move(read);
#define MT2_TERMINAL_CALL(field,rva) n.field=reinterpret_cast<decltype(n.field)>(image+rva)
    MT2_TERMINAL_CALL(queue_construct,0x9b2d90);MT2_TERMINAL_CALL(queue_destroy,0x9b2f50);
    MT2_TERMINAL_CALL(list_construct,0x977b10);MT2_TERMINAL_CALL(list_destroy,0x977cb0);
    MT2_TERMINAL_CALL(start,0x9b3030);MT2_TERMINAL_CALL(projection,0x970940);
    MT2_TERMINAL_CALL(set_projection,0x9733f0);MT2_TERMINAL_CALL(set_view,0x9732f0);
    MT2_TERMINAL_CALL(window_draw,0x81dc30);MT2_TERMINAL_CALL(queue_draw,0x9b33f0);
    MT2_TERMINAL_CALL(end,0x9b3490);MT2_TERMINAL_CALL(clear_state,0x9ba370);
    MT2_TERMINAL_CALL(set_target,0x9ba9d0);MT2_TERMINAL_CALL(render,0x9bf050);
    MT2_TERMINAL_CALL(present,0x9b9cf0);
#undef MT2_TERMINAL_CALL
    return n;
}
inline void require(bool accepted,const std::string& cause){if(!accepted)throw std::runtime_error(cause);}
template<class T>T get(const Native& n,uintptr_t at){T out{};n.read(at,&out,sizeof(out));return out;}
inline void inspect_tree(const Native& n,const Owner& o,uintptr_t pane,std::set<uintptr_t>& visited,unsigned depth=0){
    require(pane&&depth<8&&visited.size()<192&&visited.insert(pane).second,"Terminal pane tree is absent, cyclic or exceeds its bound");
    const auto type=get<uintptr_t>(n,pane)-o.image;
    const bool window=pane==o.panel&&type==0x17883d0;
    const bool view=type==0x1745a80||type==0x16dc930;
    require(window||view||type==0x1743cc0||type==0x16dc680||type==0x1703290||type==0x16e3ec0,
        "Terminal pane tree contains an unsupported native class");
    if(window||view){
        for(size_t offset:{size_t(0x2d0),size_t(0x300)}){
            const auto count=get<int32_t>(n,pane+offset+8);const auto array=get<uintptr_t>(n,pane+offset);
            require(count>=0&&count<=160&&(!count||array),"Terminal pane child array is invalid");
            for(int i=0;i<count;++i)inspect_tree(n,o,get<uintptr_t>(n,array+sizeof(uintptr_t)*i),visited,depth+1);
        }
    }
    if(window){const auto title=get<uintptr_t>(n,pane+0x3b8);if(title)inspect_tree(n,o,title,visited,depth+1);}
}
struct Surface {uintptr_t scene=0,camera=0,renderer=0,target=0;int scene_index=-1;};
inline Surface inspect(const Native& n,const Owner& o,bool visible=true){
    require(n.ready(),"Terminal native draw entrypoints are incomplete");
    require(o.thread&&o.thread==o.current_thread,"Terminal panel requires its bound thread");
    require(o.image&&o.game&&o.mode&&o.panel&&o.panel==o.registered_panel&&o.screen&&o.window,"Terminal panel registry owner is absent or changed");
    require(o.width>0&&o.height>0&&o.width<=16384&&o.height<=16384,"Terminal drawable dimensions are invalid");
    require(o.mode_index==2||o.mode_index==3,"Terminal native Mode is neither Title nor InGame");
    require(get<uintptr_t>(n,o.image+0x13df8f0)==o.game&&get<uintptr_t>(n,o.game+0x28)==o.mode&&
        get<uintptr_t>(n,o.game+(o.mode_index==2?0x350:0x358))==o.mode&&
        get<uintptr_t>(n,o.mode)==o.image+(o.mode_index==2?0x16dfbb0:0x16e6730)&&get<uintptr_t>(n,o.image+0x1c4e3a0)==o.screen,
        "Terminal native Game, Mode or Screen owner changed");
    require(get<uintptr_t>(n,o.image+0x1c4dc08)==o.window,"Terminal native Present window differs from retained pixels");
    require(get<uintptr_t>(n,o.panel)==o.image+0x17883d0,"Terminal panel is not the native base Window");
    require(get<uintptr_t>(n,o.panel+0x318)==0&&get<int32_t>(n,o.panel+0x3b0)==0,
        "Terminal panel has external logic or attached panes");
    require(!visible||(get<uint8_t>(n,o.panel+0x45f)==1&&get<uint8_t>(n,o.panel+0x69)==1),
        "Terminal panel is not visibly open");
    std::set<uintptr_t> tree;inspect_tree(n,o,o.panel,tree);
    Surface s;s.scene=get<uintptr_t>(n,o.panel+0x60);
    require(s.scene&&get<uintptr_t>(n,s.scene)==o.image+0x17494e0&&get<uint8_t>(n,s.scene+0xac)==0,
        "Terminal panel does not belong to a native 2D Scene");
    const auto count=get<int32_t>(n,o.screen+0x30);const auto scenes=get<uintptr_t>(n,o.screen+0x10);
    require(scenes&&count>0&&count<=128,"Terminal Screen scene table is invalid");
    for(int i=0;i<count;++i)if(get<uintptr_t>(n,scenes+sizeof(uintptr_t)*i)==s.scene){
        require(s.scene_index<0,"Terminal Scene is registered twice");s.scene_index=i;
    }
    require(s.scene_index>=0,"Terminal panel Scene is not owned by Screen");
    // Viewport, stencil and depth-clear scenes need a different pass. This leaf
    // only draws an owned full-window panel over the retained window image.
    const auto stencil=get<uint8_t>(n,s.scene+0xaf),viewport=get<uint8_t>(n,s.scene+0xb0),depth=get<uint8_t>(n,s.scene+0xb2);
    require(stencil==0&&viewport==0&&depth==0,"Terminal Scene requires full-window plain pass: stencil="+
        std::to_string(stencil)+" viewport="+std::to_string(viewport)+" depth_clear="+std::to_string(depth));
    s.camera=get<uintptr_t>(n,s.scene+0x38);s.renderer=get<uintptr_t>(n,o.screen);
    require(s.camera&&get<uintptr_t>(n,s.camera)==o.image+0x16c2540&&s.renderer&&
        get<uintptr_t>(n,s.renderer)==o.image+0x171c850,"Terminal camera or renderer class differs");
    require(get<uint8_t>(n,s.renderer+0x9a4)==1,"Terminal renderer still has an unfinished native GPU query");
    s.target=get<uintptr_t>(n,s.renderer+0x150);
    require(s.target&&get<uintptr_t>(n,s.target+0x60),"Terminal present target has no existing backing");
    auto buffer=get<uintptr_t>(n,s.target+0x58);if(!buffer)buffer=get<uintptr_t>(n,s.target+0x60);
    const auto framebuffer=get<uint32_t>(n,buffer+0x1c);const auto width=get<int32_t>(n,s.target+0x68),height=get<int32_t>(n,s.target+0x6c);
    require(framebuffer==0&&width==o.width&&height==o.height,"Terminal present target differs: FBO="+std::to_string(framebuffer)+
        " size="+std::to_string(width)+"x"+std::to_string(height)+" required=0/"+std::to_string(o.width)+"x"+std::to_string(o.height));
    return s;
}
struct Attempt {
    bool submitted=false,present_attempted=false,present_returned=false,blocked=false;
    std::string error;
};
// The caller owns executable pins, the exact generated child tree, the retained
// image and the local-only swap scope. This leaf neither polls input nor updates
// any Window, Scene, Mode, Timer, Task queue or Game object.
inline bool draw(const Native& n,const Owner& o,const std::function<void()>& repaint,Attempt& receipt){
    if(receipt.blocked)return false;
    receipt.submitted=receipt.present_attempted=receipt.present_returned=false;
    Queue queue;DisplayList list;bool queue_ready=false,list_ready=false;
    auto retain=[&](const std::string& cause){if(receipt.error.empty())receipt.error=cause;else receipt.error+="; "+cause;receipt.blocked=true;};
    try{
        require(bool(repaint),"Terminal draw lacks retained background owner");
        const auto s=inspect(n,o);
        n.queue_construct(&queue);queue_ready=true;n.list_construct(&list,0x19000,true);list_ready=true;
        n.start(&queue,reinterpret_cast<void*>(s.scene),s.scene_index);
        Matrix projection;n.projection(&projection,reinterpret_cast<void*>(s.camera));
        for(float v:projection.values)require(std::isfinite(v),"Terminal native projection contains a non-finite value");
        std::memcpy(queue.bytes.data()+0x40,&projection,sizeof(projection));
        n.set_projection(&list,&projection);n.set_view(&list,queue.bytes.data()+0x80);
        n.window_draw(reinterpret_cast<void*>(o.panel),&queue);
        n.queue_draw(&queue,&list);n.end(&queue);
        n.clear_state(reinterpret_cast<void*>(s.renderer));n.set_target(reinterpret_cast<void*>(s.renderer),reinterpret_cast<void*>(s.target));
        repaint();receipt.submitted=true;n.render(reinterpret_cast<void*>(s.renderer),&list);
        receipt.present_attempted=true;n.present(reinterpret_cast<void*>(s.renderer));receipt.present_returned=true;
    }catch(const std::exception& e){retain(e.what());}
    catch(...){retain("Terminal panel draw raised an unknown native exception");}
    // Retire each constructed native object once. Cleanup never replays a draw
    // or Present after a native call with an unknown outcome.
    if(list_ready)try{n.list_destroy(&list);}catch(const std::exception& e){retain(std::string("DisplayList cleanup failed: ")+e.what());}catch(...){retain("DisplayList cleanup raised an unknown exception");}
    if(queue_ready)try{n.queue_destroy(&queue);}catch(const std::exception& e){retain(std::string("RenderQueue cleanup failed: ")+e.what());}catch(...){retain("RenderQueue cleanup raised an unknown exception");}
    return !receipt.blocked&&receipt.present_returned;
}
static_assert(sizeof(Queue)==0x5d0&&sizeof(DisplayList)==0x260&&sizeof(Matrix)==64);
}
