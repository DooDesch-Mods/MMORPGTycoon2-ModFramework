#include "diagnostic_click.h"
#include "diagnostic_click_readback.h"
#include <cstring>
#include <map>
#include <iostream>
#include <limits>
#include <vector>

using Click=mt2fw::mt2::DiagnosticClick;
namespace {
void require(bool condition,const char* cause){if(!condition)throw std::runtime_error(cause);}
template<class Action> void rejected(Action action,const char* cause){
    try{action();}catch(const std::runtime_error& error){require(std::string(error.what()).find(cause)!=std::string::npos,"Original error differs");return;}
    throw std::runtime_error("Expected rejection absent");
}
struct NativeFixture {
    static constexpr uintptr_t image=0x10000000,input=0x20000000,cursor=0x30000000,screen=0x40000000,axes=0x50000000,pane=0x60000000;
    std::map<uintptr_t,std::vector<unsigned char>> memory;
    template<class T>void put(uintptr_t at,const T& value){auto& bytes=memory[at];bytes.resize(sizeof(T));std::memcpy(bytes.data(),&value,sizeof(T));}
    bool read(uintptr_t at,void* output,size_t size,std::string& error){
        const auto found=memory.find(at);if(found==memory.end()||found->second.size()!=size){error="fixture unreadable field";return false;}
        std::memcpy(output,found->second.data(),size);return true;
    }
    NativeFixture(){
        put(image+0x13df990,input);put(input,uintptr_t(image+0x1749430));put(input+0x50,axes);put(input+0x58,int32_t(34));put(input+0x124,uint8_t(0));
        put(input+0x7c,std::array<float,2>{-0.2f,0.7f});put(axes+27*0x70+0x60,std::array<uint8_t,2>{1,0});
        put(image+0x1c4e3a0,screen);put(screen+0x54,std::array<int32_t,2>{1280,720});
        put(image+0x13df9d0,cursor);put(cursor,uintptr_t(image+0x1786dd0));
        put(cursor+0x1f0,std::array<float,2>{582,688});put(cursor+0x1f8,std::array<float,2>{-0.3f,0.9f});
        put(cursor+0x1e4,std::array<float,3>{1,2,3});put(cursor+0x174,std::array<uint8_t,3>{0,0,0});
        put(cursor+0x214,std::array<uint8_t,2>{0,0});
        for(auto offset:{0x257,0x256,0x1e0})put(cursor+offset,uint8_t(0));
        for(auto offset:{0x1d8,0x180,0x1c0})put(cursor+offset,uintptr_t(0));
        put(cursor+0x1c8,pane);put(pane,uintptr_t(image+0x16e5b90));
    }
    mt2fw::mt2::ClickNativeReadback sample(){return mt2fw::mt2::read_click_native(image,[&](auto at,auto output,auto size,auto& error){return read(at,output,size,error);});}
};
struct Fixture {
    Click click;
    std::vector<Click::Edge> events;
    int fail_edge=-1;
    void push(Click::Edge edge){events.push_back(edge);if(int(edge)==fail_edge)throw std::runtime_error("original SDL enqueue failure");}
    void begin(){click.begin("exact-request",1,2,3,582,688,101,1000,[&](auto edge){push(edge);});}
    void seen(Click::Edge edge,bool delivered=true,uint64_t frame=9){click.observe(edge,2,101,582,688,delivered,frame);}
    void present(uint64_t frame=10,uint64_t now=1100){click.present(1,2,3,frame,now,[&](auto edge){push(edge);});}
};
}
int main(){
    try{
        int checks=0;
        {
            Fixture f;f.begin();
            require(f.events==std::vector<Click::Edge>{Click::Edge::motion,Click::Edge::down},"Release entered original poll batch");
            f.present();require(f.events.size()==2,"Unobserved DOWN released");
            f.seen(Click::Edge::down);f.present(9);require(f.events.size()==2,"Same update released");
            f.present(10);f.present(10);require(f.events.size()==3&&f.events.back()==Click::Edge::up,"Release did not enqueue exactly once after later presentation");
            f.seen(Click::Edge::up,true,10);f.present(10);require(f.click.pending(),"Forwarded release claimed consumed before next update");
            f.present(11);require(!f.click.unresolved()&&f.click.phase==Click::Phase::complete,"Input completion absent");
            require(f.click.attempted==7&&f.click.queued==7&&f.click.forwarded==6,"Effect masks differ");
            checks+=7;
        }
        {
            Fixture f;f.begin();
            f.click.observe(Click::Edge::down,99,101,582,688,true,9);
            f.click.observe(Click::Edge::down,2,102,582,688,true,9);
            f.click.observe(Click::Edge::down,2,101,583,688,true,9);
            f.click.observe(Click::Edge::up,2,101,582,688,true,9);
            f.present();require(f.events.size()==2&&f.click.forwarded==0,"Foreign event armed release");
            rejected([&]{f.begin();},"Previous diagnostic click");require(f.events.size()==2,"Pending request replayed");
            checks+=2;
        }
        for(int edge=0;edge<3;++edge){
            Fixture f;f.fail_edge=edge;
            if(edge<2)rejected([&]{f.begin();},"original SDL enqueue failure");
            else {f.begin();f.seen(Click::Edge::down);rejected([&]{f.present();},"original SDL enqueue failure");}
            require(f.click.phase==Click::Phase::failed&&f.click.cause=="original SDL enqueue failure","Partial enqueue lost its original cause");
            auto count=f.events.size();f.present();rejected([&]{f.begin();},"Previous diagnostic click");
            require(f.events.size()==count,"Failed enqueue replayed");checks+=2;
        }
        for(int boundary=0;boundary<6;++boundary){
            Fixture f;f.begin();f.seen(Click::Edge::down);
            if(boundary==0)f.click.check(9,2,3,1100);
            if(boundary==1)f.click.check(1,9,3,1100);
            if(boundary==2)f.click.check(1,2,9,1100);
            if(boundary==3)f.click.check(1,2,3,181000);
            if(boundary==4)f.click.fail("original GameUpdate exception");
            if(boundary==5)f.click.fail("owned focus lost");
            require(f.click.phase==Click::Phase::failed&&!f.click.cause.empty(),"Invalid lifecycle boundary stayed pending");
            const auto cause=f.click.cause;f.present();f.click.fail("later failure");
            require(f.events.size()==2&&f.click.cause==cause,"Late presentation released or hid original failure");checks+=2;
        }
        {
            Fixture f;f.begin();f.seen(Click::Edge::down);f.present(10,39000);
            require(f.events.size()==3&&f.click.phase==Click::Phase::up_queued,"Legitimate 38-second native work prevented release");
            Fixture late;late.begin();late.click.check(1,2,3,181000);
            require(late.click.cause.find("elapsed_ms=180000 required_ms<180000")!=std::string::npos,"Delivery deadline lost measured and required values");checks+=2;
        }
        {
            Fixture f;f.begin();f.seen(Click::Edge::down,false);f.present();
            require(f.click.phase==Click::Phase::failed&&f.events.size()==2,"Captured DOWN released");++checks;
        }
        {
            Fixture f;f.begin();f.seen(Click::Edge::down);f.present();f.seen(Click::Edge::up,false,10);
            require(f.click.phase==Click::Phase::failed&&f.click.forwarded==2,"Captured UP completed");++checks;
        }
        {
            Fixture f;
            rejected([&]{f.click.begin("id",1,2,3,std::numeric_limits<float>::quiet_NaN(),2,1,1,[&](auto e){f.push(e);});},"coordinates invalid");
            rejected([&]{f.click.begin("id",1,0,3,1,2,1,1,[&](auto e){f.push(e);});},"identity");
            require(f.events.empty()&&f.click.phase==Click::Phase::idle,"Invalid request caused input");++checks;
        }
        // Model the pinned native branch, using edges emitted by the actual helper.
        {
            auto dispatch=[](bool press,bool release){return release?2:(press?1:0);};
            require(dispatch(true,true)==2,"Pinned native release-first baseline differs");
            Fixture f;f.begin();require(dispatch(f.events.back()==Click::Edge::down,false)==1,"First delivery did not press");
            f.seen(Click::Edge::down);f.present();require(dispatch(false,f.events.back()==Click::Edge::up)==2,"Later delivery did not release");checks+=3;
        }
        {
            Fixture f;f.begin();
            require(!f.click.drain_position(1,2,3,9,1100),"Position preceded observed DOWN");
            f.seen(Click::Edge::down);
            // Real motion after the button is still delivered; the tail owns the final XY.
            float native_x=123,native_y=456;
            require(f.click.drain_position(1,2,3,9,1100),"DOWN drain lacks final position");
            native_x=f.click.x;native_y=f.click.y;f.click.position_delivered(true);
            require(native_x==582&&native_y==688&&f.click.position_forwarded==1,"Later real motion won DOWN position");
            require(f.click.finish_position_poll(3)&&!f.click.finish_position_poll(3),"Poll tail did not end exactly once");
            require(!f.click.drain_position(1,2,3,9,1100),"Position repeated within one update");
            f.present();f.seen(Click::Edge::up,true,10);
            native_x=777;native_y=222;
            require(f.click.drain_position(1,2,3,10,1200),"UP drain lacks final position");
            native_x=f.click.x;native_y=f.click.y;f.click.position_delivered(true);
            require(native_x==582&&native_y==688&&f.click.position_attempted==2&&
                    f.click.position_forwarded==2&&f.click.last_position_x==582&&f.click.last_position_y==688,
                    "Later real motion won UP position or receipt differs");
            require(f.click.finish_position_poll(3),"UP drain tail absent");
            f.present(11);require(!f.click.drain_position(1,2,3,11,1300),"Completed click retained position override");
            checks+=9;
        }
        for(int boundary=0;boundary<5;++boundary){
            Fixture f;f.begin();f.seen(Click::Edge::down);
            const bool emitted=f.click.drain_position(boundary==0?9:1,boundary==1?9:2,
                boundary==2?9:3,9,boundary==3?181000:1100);
            if(boundary==4){require(emitted,"Captured-position case not emitted");f.click.position_delivered(false);f.click.finish_position_poll(3);}
            else require(!emitted,"Foreign or expired position emitted");
            require(f.click.phase==Click::Phase::failed&&!f.click.cause.empty()&&f.click.position_forwarded==0,
                    "Invalid final position lost failure or claimed forwarding");
            require(!f.click.drain_position(1,2,3,10,1200),"Failed final position replayed");checks+=3;
        }
        {
            NativeFixture n;const auto initial=n.sample();
            require(initial.cause.empty()&&initial.input_read&&initial.screen_read&&initial.cursor_read,"Actual native readback helper failed valid fields");
            require(initial.input_xy[0]==-0.2f&&initial.cursor_ui[0]==582&&initial.input_edges[0]==1&&initial.focus_window_class==0x16e5b90,"Native readback conflated raw input, effective cursor or pane");
            n.put(NativeFixture::cursor+0x1e0,uint8_t(1));n.put(NativeFixture::cursor+0x1d8,uintptr_t(0x70000000));
            n.put(NativeFixture::cursor+0x1f0,std::array<float,2>{100,200});
            const auto locked=n.sample();require(locked.cursor_read&&locked.trap&&locked.trap_gate==1&&locked.cursor_ui[0]==100&&locked.input_xy==initial.input_xy,"Locked cursor divergence hidden");
            n.put(NativeFixture::axes+27*0x70+0x60,std::array<uint8_t,2>{0,1});
            require(n.sample().input_edges[1]==1,"Release sample retained stale native fields");checks+=4;
        }
        {
            NativeFixture n;n.memory.erase(NativeFixture::cursor+0x1f0);auto result=n.sample();
            require(!result.cursor_read&&result.input_read&&result.cause.find("cursor UI XY: fixture unreadable field")!=std::string::npos,"Read failure lost field/original cause or claimed complete");
            NativeFixture wrong;wrong.put(NativeFixture::input,uintptr_t(123));result=wrong.sample();
            require(!result.input_read&&result.cause=="Click native input class differs","Foreign input class accepted");
            NativeFixture nonfinite;nonfinite.put(NativeFixture::input+0x7c,std::array<float,2>{std::numeric_limits<float>::quiet_NaN(),0});
            require(!nonfinite.sample().cause.empty(),"Non-finite input silently serialized");
            NativeFixture count;count.put(NativeFixture::input+0x58,int32_t(27));
            require(!count.sample().input_read,"Absent left axis read");checks+=4;
        }
        {
            Fixture f;f.click.begin("outside",1,2,3,582,688,101,1000,[&](auto edge){f.push(edge);},true);
            require(f.events==std::vector<Click::Edge>{Click::Edge::enter,Click::Edge::motion,Click::Edge::down},"Outside click did not enter before motion/DOWN");
            f.click.presence(true,2,101,true,9);f.seen(Click::Edge::down);f.present(10);
            f.seen(Click::Edge::up,true,10);f.present(11);
            require(f.click.phase==Click::Phase::restore_pending&&f.click.enter_forwarded,"Outside click completed before restoring native mouse state");
            require(f.click.restore_mouse(1,2,3,1200)&&!f.click.restore_mouse(1,2,3,1200),"Native leave was not emitted exactly once");
            f.click.presence(false,2,101,true,11);f.present(11);require(f.click.pending(),"Leave forwarding claimed native consumption");
            f.present(12);require(f.click.phase==Click::Phase::complete&&f.click.leave_forwarded&&f.click.forwarded==6,"Outside restoration did not finish with original button masks");checks+=5;
        }
        {
            Fixture f;f.click.begin("outside",1,2,3,582,688,101,1000,[&](auto edge){f.push(edge);},true);
            f.click.presence(true,2,101,true,9);f.seen(Click::Edge::down);f.present(10);f.seen(Click::Edge::up,true,10);
            f.click.present(1,2,3,11,1200,[&](auto edge){f.push(edge);},true);
            require(f.click.phase==Click::Phase::complete&&f.click.restore_skipped_physical_focus&&!f.click.leave_attempted,"Physical mouse arrival was overwritten by synthetic leave");++checks;
        }
        {
            Fixture f;f.click.begin("outside",1,2,3,582,688,101,1000,[&](auto edge){f.push(edge);},true);
            f.seen(Click::Edge::down);require(f.click.phase==Click::Phase::failed&&f.click.cause.find("preceded owned mouse enter")!=std::string::npos,"Missing native enter was accepted");
            Fixture leave;leave.begin();leave.click.presence(false,2,999,true,9);require(leave.click.phase==Click::Phase::failed&&!leave.click.restore_mouse(1,2,3,1200),"Genuine mouse leave failed silently or replayed");
            Fixture captured;captured.click.begin("outside",1,2,3,582,688,101,1000,[&](auto edge){captured.push(edge);},true);
            captured.click.presence(true,2,101,false,9);require(captured.click.phase==Click::Phase::failed,"Captured native enter accepted");checks+=3;
        }
        {
            Fixture f;f.fail_edge=int(Click::Edge::enter);
            rejected([&]{f.click.begin("outside",1,2,3,582,688,101,1000,[&](auto edge){f.push(edge);},true);},"original SDL enqueue failure");
            require(f.events.size()==1&&f.click.enter_attempted&&!f.click.enter_queued&&f.click.attempted==0,"Failed enter leaked a button");++checks;
            NativeFixture n;require(n.sample().mouse_on_screen==0,"Initial outside read lost");n.put(NativeFixture::input+0x124,uint8_t(1));require(n.sample().mouse_on_screen==1,"Inside read retained stale outside");
            n.put(NativeFixture::input+0x124,uint8_t(2));require(!n.sample().input_read&&!n.sample().cause.empty(),"Malformed native presence accepted");checks+=3;
        }
        std::cout<<checks<<" diagnostic click checks PASS\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
