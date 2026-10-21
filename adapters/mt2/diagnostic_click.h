#pragma once
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace mt2fw::mt2 {
// MT2 gives Release priority over Press when both edges reach one input update.
// Keep the release behind a forwarded press and a later owned presentation.
struct DiagnosticClick {
    enum class Phase { idle, down_queued, down_forwarded, up_queued, up_forwarded, restore_pending, restore_forwarded, complete, failed };
    enum class Edge { motion, down, up, enter };
    Phase phase=Phase::idle;
    std::string id,cause;
    uintptr_t owner=0;
    uint32_t window_id=0,thread=0;
    float x=0,y=0;
    uint64_t stamp=0,started=0,down_frame=0,up_frame=0;
    unsigned attempted=0,queued=0,forwarded=0;
    bool borrowed_mouse=false,enter_attempted=false,enter_queued=false,enter_forwarded=false;
    bool leave_attempted=false,leave_forwarded=false,restore_skipped_physical_focus=false;
    uint64_t leave_frame=0;
    void presence(bool entered,uint32_t event_window,uint64_t timestamp,bool delivered,uint64_t frame){
        if(!pending()||event_window!=window_id)return;
        if(timestamp!=stamp){
            if(!entered)fail("Diagnostic click interrupted by real mouse leave; no replay");
            return;
        }
        if(!delivered){fail("Diagnostic click window mouse event was captured before native input; no replay");return;}
        if(entered&&borrowed_mouse&&enter_queued)enter_forwarded=true;
        if(!entered&&phase==Phase::restore_pending&&leave_attempted){leave_forwarded=true;leave_frame=frame;phase=Phase::restore_forwarded;}
    }
    bool restore_mouse(uintptr_t current_owner,uint32_t current_window,uint32_t current_thread,uint64_t now){
        check(current_owner,current_window,current_thread,now);
        if(phase!=Phase::restore_pending||leave_attempted)return false;
        leave_attempted=true;return true;
    }
    uint64_t position_frame=0,position_attempted=0,position_forwarded=0;
    float last_position_x=0,last_position_y=0;
    bool position_frame_valid=false,position_poll_end=false;
    // End the poll drain after its final logical motion, before another SDL pump.
    bool finish_position_poll(uint32_t current_thread){
        if(!position_poll_end||current_thread!=thread)return false;
        position_poll_end=false;return true;
    }
    bool drain_position(uintptr_t current_owner,uint32_t current_window,uint32_t current_thread,
                        uint64_t frame,uint64_t now){
        check(current_owner,current_window,current_thread,now);
        if(!pending()||phase==Phase::down_queued||phase==Phase::restore_pending||phase==Phase::restore_forwarded||
           (position_frame_valid&&position_frame==frame))return false;
        position_frame=frame;position_frame_valid=true;position_poll_end=true;
        ++position_attempted;return true;
    }
    void position_delivered(bool delivered){
        if(!pending())return;
        if(!delivered){fail("Diagnostic click final position was captured before native input; no replay");return;}
        ++position_forwarded;last_position_x=x;last_position_y=y;
    }
    bool pending() const {return phase!=Phase::idle&&phase!=Phase::complete&&phase!=Phase::failed;}
    bool unresolved() const {return pending()||phase==Phase::failed;}
    const char* name() const {
        switch(phase){
        case Phase::idle:return "idle";case Phase::down_queued:return "down_queued";
        case Phase::down_forwarded:return "down_forwarded";case Phase::up_queued:return "up_queued";
        case Phase::up_forwarded:return "up_forwarded";
        case Phase::restore_pending:return "restore_mouse_pending";case Phase::restore_forwarded:return "restore_mouse_forwarded";
        case Phase::complete:return "input_complete";
        case Phase::failed:return "failed";}
        return "failed";
    }
    void fail(const std::string& error){if(pending()){cause=error;phase=Phase::failed;}}
    void check(uintptr_t current_owner,uint32_t current_window,uint32_t current_thread,uint64_t now){
        if(!pending())return;
        if(owner!=current_owner||window_id!=current_window||thread!=current_thread)
            fail("Diagnostic click owned window or thread changed; no replay");
        else if(now<started)fail("Diagnostic click monotonic time moved backwards; no replay");
        else if(now-started>=180000)
            fail("Diagnostic click delivery timeout elapsed_ms="+std::to_string(now-started)+" required_ms<180000; no replay");
    }
    template<class Push>
    void begin(const std::string& request,uintptr_t owned_window,uint32_t owned_id,uint32_t owned_thread,
               float px,float py,uint64_t timestamp,uint64_t now,Push&& push,bool mouse_outside=false){
        if(unresolved())throw std::runtime_error("Previous diagnostic click is unresolved; no replay");
        if(request.empty()||!owned_window||!owned_id||!owned_thread||!timestamp||
           !std::isfinite(px)||!std::isfinite(py))throw std::runtime_error("Diagnostic click identity or coordinates invalid");
        *this={};id=request;owner=owned_window;window_id=owned_id;thread=owned_thread;
        x=px;y=py;stamp=timestamp;started=now;phase=Phase::down_queued;
        borrowed_mouse=mouse_outside;
        try{
            if(borrowed_mouse){enter_attempted=true;push(Edge::enter);enter_queued=true;}
            attempted|=1;push(Edge::motion);queued|=1;
            attempted|=2;push(Edge::down);queued|=2;
        }catch(const std::exception& error){fail(error.what());throw;}
        catch(...){fail("Diagnostic click enqueue raised an unknown exception; no replay");throw;}
    }
    bool matches(uint32_t event_window,uint64_t timestamp,float px,float py) const {
        return pending()&&event_window==window_id&&timestamp==stamp&&px==x&&py==y;
    }
    void observe(Edge edge,uint32_t event_window,uint64_t timestamp,float px,float py,
                 bool delivered,uint64_t frame){
        if(!matches(event_window,timestamp,px,py))return;
        if(!delivered){fail("Diagnostic click event was captured before native input; no replay");return;}
        if(edge==Edge::down&&phase==Phase::down_queued){
            if(borrowed_mouse&&!enter_forwarded){fail("Diagnostic click DOWN preceded owned mouse enter; no replay");return;}
            forwarded|=2;down_frame=frame;phase=Phase::down_forwarded;
        }
        else if(edge==Edge::up&&phase==Phase::up_queued){forwarded|=4;up_frame=frame;phase=Phase::up_forwarded;}
    }
    template<class Push>
    void present(uintptr_t current_owner,uint32_t current_window,uint32_t current_thread,
                 uint64_t frame,uint64_t now,Push&& push,bool physical_mouse_owned=false){
        check(current_owner,current_window,current_thread,now);
        if(phase==Phase::restore_forwarded&&frame>leave_frame){phase=Phase::complete;return;}
        if(phase==Phase::up_forwarded&&frame>up_frame){
            if(borrowed_mouse&&!physical_mouse_owned)phase=Phase::restore_pending;
            else {restore_skipped_physical_focus=borrowed_mouse&&physical_mouse_owned;phase=Phase::complete;}
            return;
        }
        if(phase!=Phase::down_forwarded||frame<=down_frame)return;
        try{attempted|=4;push(Edge::up);queued|=4;phase=Phase::up_queued;}
        catch(const std::exception& error){fail(error.what());throw;}
        catch(...){fail("Diagnostic click release enqueue raised an unknown exception; no replay");throw;}
    }
};
}
