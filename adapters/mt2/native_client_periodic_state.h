#pragma once
#include "sdk/native_steps_draft.h"
#include <algorithm>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>

namespace mt2fw::mt2 {
// This receipt owns only the local notice. A confirmed Host periodic Save
// purpose must be associated with its certificate before it can complete UI.
struct NativeClientPeriodicState {
    enum class Phase { Idle, Delegated, Bound, Sealed, Attempted, Returned, Failed };
    Phase phase=Phase::Idle;
    uint64_t lease=0,session=0,world_epoch=0,generation=0,thread=0,request_tick=0,request_invocation=0,order_floor=0;
    uintptr_t mode=0,window=0;
    MT2FW_MT2_SnapshotRequest certificate{};
    uint64_t delegations=0,notify_attempts=0,notify_returns=0;
    bool attempted=false,returned=false;
    std::string cause;

    static void need(bool value,const char* error){if(!value)throw std::runtime_error(error);}
    bool pending()const{return phase!=Phase::Idle&&phase!=Phase::Returned;}
    void retain(const MT2FW_MT2_StepReceipt& step,uintptr_t current_mode,uintptr_t current_window,uint64_t last_order){
        need(!pending(),"Client periodic UI already has an unfinished delegated owner");
        need(step.actual_role==MT2FW_MT2_SESSION_CLIENT&&step.lease&&step.session&&step.world_epoch&&step.generation&&step.thread&&
            step.tick&&current_mode&&current_window,"Client periodic UI delegation has incomplete native identity");
        lease=step.lease;session=step.session;world_epoch=step.world_epoch;generation=step.generation;thread=step.thread;
        request_tick=step.tick;request_invocation=step.invocation;order_floor=last_order;mode=current_mode;window=current_window;
        certificate={};attempted=returned=false;cause.clear();++delegations;phase=Phase::Delegated;
    }
    bool bind(const MT2FW_MT2_SnapshotRequest& request){
        if(phase==Phase::Idle||phase==Phase::Returned||request.operation!=MT2FW_MT2_SNAPSHOT_ARM_PERIODIC)return false;
        need(phase==Phase::Delegated,"Client periodic UI certificate binding is repeated or failed");
        need(request.size==sizeof(request)&&!request.reserved&&
            request.lease==lease&&request.session==session&&request.world_epoch==world_epoch,
            "Client periodic UI binding has a foreign snapshot owner");
        if(request.tick<request_tick||request.order<=order_floor)return false;
        need(request.invocation&&request.peer_mask&&request.peer_mask<=15&&(request.peer_mask&1)&&!request.ack_mask&&
            std::all_of(std::begin(request.witness_sha256),std::end(request.witness_sha256),[](auto byte){return byte==0;}),
            "Client periodic UI binding lacks its fresh native ARM certificate");
        need(request.nonce[36]==0,"Client periodic UI binding nonce is not terminated");
        for(size_t i=0;i<36;++i)need(i==8||i==13||i==18||i==23?request.nonce[i]=='-':
            (request.nonce[i]>='0'&&request.nonce[i]<='9')||(request.nonce[i]>='a'&&request.nonce[i]<='f'),
            "Client periodic UI binding nonce is not canonical");
        certificate=request;phase=Phase::Bound;return true;
    }
    void same(const MT2FW_MT2_SnapshotRequest& request)const{
        need(request.size==sizeof(request)&&!request.reserved&&request.lease==certificate.lease&&request.session==certificate.session&&
            request.world_epoch==certificate.world_epoch&&request.tick==certificate.tick&&request.order==certificate.order&&
            request.peer_mask==certificate.peer_mask&&!request.invocation&&std::memcmp(request.nonce,certificate.nonce,37)==0,
            "Client periodic UI release differs from its bound snapshot certificate");
    }
    void seal(const MT2FW_MT2_SnapshotRequest& request){
        if(phase==Phase::Idle||phase==Phase::Returned||phase==Phase::Delegated)return;
        need(phase==Phase::Bound,"Client periodic UI SEAL is repeated or failed");same(request);
        need(request.operation==MT2FW_MT2_SNAPSHOT_SEAL&&!request.ack_mask&&
            std::any_of(std::begin(request.witness_sha256),std::end(request.witness_sha256),[](auto byte){return byte!=0;}),
            "Client periodic UI SEAL lacks its shared snapshot witness");
        std::memcpy(certificate.witness_sha256,request.witness_sha256,32);phase=Phase::Sealed;
    }
    template<class Guard,class Notify,class Post>bool complete(const MT2FW_MT2_SnapshotRequest& request,const Guard& guard,const Notify& notify,const Post& post){
        if(phase==Phase::Idle||phase==Phase::Returned||phase==Phase::Delegated)return false;
        try{
            need(phase==Phase::Sealed&&!attempted,"Client periodic UI completion is repeated, unsealed or failed");same(request);
            need(request.operation==MT2FW_MT2_SNAPSHOT_RELEASE&&request.ack_mask==certificate.peer_mask&&
                std::memcmp(request.witness_sha256,certificate.witness_sha256,32)==0,
                "Client periodic UI completion lacks its successful shared RELEASE");
            guard();attempted=true;++notify_attempts;phase=Phase::Attempted;
            notify();returned=true;++notify_returns;
            post();phase=Phase::Returned;return true;
        }catch(const std::exception& error){phase=Phase::Failed;if(cause.empty())cause=error.what();throw;}
        catch(...){phase=Phase::Failed;if(cause.empty())cause="Client periodic UI completion raised unknown exception";throw;}
    }
};
}
