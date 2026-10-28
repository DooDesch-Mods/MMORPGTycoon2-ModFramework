#include "native_steps.h"
#include "native_step_pins.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace mt2fw::mt2;
static_assert(sizeof(MT2FW_MT2_StepReceipt)==408);
static_assert(offsetof(MT2FW_MT2_StepReceipt,bootstrap_coverage)==364);
static_assert(offsetof(MT2FW_MT2_StepReceipt,initial_witness_sha256)==376);
int checks=0;
void check(bool condition,const char* cause){++checks;if(!condition)throw std::runtime_error(cause);}
struct Fixture {
    MT2FW_Host owner{};
    NativeStepKernel::Proof proof;
    uint64_t thread=7;
    int originals=0,services=0,mode=0;
    bool leave_service=false;
    bool throw_advance=false,bad_timer=false,throw_service=false,read_inside=false;
    MT2FW_MT2_StepNext retained=nullptr;
    void* continuation=nullptr;
    uint64_t saved_invocation=0;
    NativeStepKernel kernel;
    MT2FW_MT2_StepServiceDraftV1 api{};
    MT2FW_MT2_StepReceipt receipt{sizeof(receipt)};
    MT2FW_MT2_StepSpec spec{};
    Fixture():kernel(&owner,[this](auto*,const auto&){return proof;},[this](const auto& frame,auto& r){
        r.original_attempted=1;++originals;
        if(read_inside)for(int i=0;i<2;++i){
            auto fresh=MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};
            check(api.read(api.context,frame.lease,&fresh)==MT2FW_MT2_OK,"reentrant retained read failed");
            check(fresh.phase==MT2FW_MT2_STEP_ENTERED&&fresh.original_attempted==1&&fresh.original_returned==0,"fresh read did not show original running");
            check(fresh.lease==frame.lease&&fresh.session==frame.session&&fresh.world_epoch==frame.world_epoch&&fresh.tick==frame.tick&&
                fresh.invocation==frame.invocation&&fresh.thread==frame.thread&&fresh.actual_role==MT2FW_MT2_SESSION_CLIENT,"fresh read boundary identity differs");
        }
        if(throw_advance)throw std::runtime_error("original cause after native attempt");
        r.original_returned=1;r.timer_calls=bad_timer?2u:1u;r.workers_held=1;
    },[this]{return thread;}){
        proof.owner=proof.provider=proof.context=proof.exclusive_routes=proof.bootstrap=proof.quiescent=true;
        proof.role=MT2FW_MT2_SESSION_CLIENT;proof.thread=7;proof.world_epoch=9;proof.witness[0]=42;
        spec.proof=[](void*,const MT2FW_MT2_StepProofQuery*,MT2FW_MT2_StepProofReply*){return MT2FW_MT2_UNAVAILABLE;};
        spec.save_request=[](void*,const MT2FW_MT2_SaveRequest*,char*,uint32_t){return MT2FW_MT2_UNAVAILABLE;};
        spec.size=sizeof(spec);spec.dt_bits=MT2FW_MT2_STEP_FIXED_DT_BITS;spec.session=11;spec.session_provider_token=22;
        spec.frozen_peer_mask=3;std::memcpy(spec.snapshot_uuid,"snapshot-1",sizeof("snapshot-1"));spec.user=this;
        spec.service=[](void* user,const auto*){
            auto& f=*static_cast<Fixture*>(user);++f.services;
            if(f.throw_service)throw std::runtime_error("held transport cause");
            if(f.leave_service){f.kernel.native_exit_requested();auto fresh=MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};
                check(f.api.read(f.api.context,f.receipt.lease,&fresh)==MT2FW_MT2_OK,"Quit ACK read unavailable");
                check(fresh.phase==MT2FW_MT2_STEP_RETIRING&&fresh.outcome==MT2FW_MT2_STEP_NOT_ATTEMPTED&&fresh.callbacks_inflight==1&&fresh.workers_held==1&&
                    !fresh.original_attempted&&!fresh.original_returned&&!fresh.timer_calls&&!fresh.invocation,"Quit service receipt pretends an original or loses hold");}
            return MT2FW_MT2_OK;
        };
        spec.boundary=[](void* user,const auto* frame,MT2FW_MT2_StepNext next,void* context){
            auto& f=*static_cast<Fixture*>(user);
            if(f.mode==1)return MT2FW_MT2_STEP_KEEP_HELD;
            if(f.mode==2)return MT2FW_MT2_STEP_COMPLETED;
            if(f.mode==3)throw std::runtime_error("command failed after attempt");
            if(f.mode==4){f.kernel.boundary();return MT2FW_MT2_STEP_KEEP_HELD;}
            if(f.mode==5){auto r=MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};
                check(f.api.unbind(f.api.context,frame->lease,&r)==MT2FW_MT2_BUSY,"retirement did not report BUSY");return MT2FW_MT2_STEP_KEEP_HELD;}
            if(f.mode==7&&f.retained){auto old=MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};
                check(f.retained(f.continuation,f.saved_invocation,&old)==MT2FW_MT2_INVALID,"old invocation entered a later callback");}
            f.retained=next;f.continuation=context;f.saved_invocation=frame->invocation;
            auto result=MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};
            auto outcome=next(context,frame->invocation,&result);
            if(f.mode==6)next(context,frame->invocation,&result);
            return outcome==MT2FW_MT2_OK?MT2FW_MT2_STEP_COMPLETED:MT2FW_MT2_STEP_STOP_UNKNOWN;
        };
        check(kernel.query(&owner,MT2FW_MT2_STEP_DRAFT_VERSION,&api,sizeof(api))==MT2FW_MT2_OK,"query failed");
    }
    MT2FW_MT2_Result bind(){const auto result=api.bind(api.context,&spec,&receipt);
        if(result==MT2FW_MT2_OK&&proof.bootstrap){auto life=MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};
            life.generation=1;life.seed_attempted=life.seed_returned=life.bootstrap_held=life.workers_held=1;
            life.world_epoch=proof.world_epoch;life.bootstrap_coverage=MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS;life.initial_witness_sha256[0]=42;
            kernel.lifecycle(life);check(api.read(api.context,receipt.lease,&receipt)==MT2FW_MT2_OK,"synthetic covered bootstrap read");}return result;}
    MT2FW_MT2_Result activate(){
        MT2FW_MT2_StepActivation a{};a.size=sizeof(a);a.lease=receipt.lease;a.session=11;a.world_epoch=9;a.first_tick=1;
        std::memcpy(a.snapshot_uuid,"snapshot-1",sizeof("snapshot-1"));a.initial_witness_sha256[0]=42;
        return api.activate(api.context,&a,&receipt);
    }
    void ready(){check(bind()==MT2FW_MT2_OK,"bind failed");check(activate()==MT2FW_MT2_OK,"activate failed");}
};
int main(){try{
    {Fixture f;f.spec.save_request=nullptr;check(f.bind()==MT2FW_MT2_INVALID&&f.originals==0,"missing draft 9 Save callback admitted");}
    check(!step_pins::select_step_owner(true,false),"lab ownership selector changed");
    check(step_pins::select_step_owner(false,true),"step owner not selected");
    bool conflict=false;try{step_pins::select_step_owner(true,true);}catch(const std::exception&){conflict=true;}
    check(conflict,"two physical owners were admitted");
    {Fixture f;f.proof.bootstrap=false;check(f.bind()==MT2FW_MT2_OK,"preload arm requires premature bootstrap");
        check(f.activate()==MT2FW_MT2_UNAVAILABLE&&f.originals==0&&std::strstr(f.receipt.cause,"bootstrap"),"C14 gate was bypassed or its cause hidden");}
    {Fixture f;f.proof.lab_gate=true;check(f.bind()==MT2FW_MT2_UNAVAILABLE&&f.originals==0&&std::strstr(f.receipt.cause,"Go/Timer"),"lab gate got a second owner or lost conflict cause");}
    {Fixture f;f.proof.exclusive_routes=false;check(f.bind()==MT2FW_MT2_UNAVAILABLE,"unowned routes admitted");}
    {Fixture f;f.proof.provider=false;check(f.bind()==MT2FW_MT2_UNAVAILABLE,"unacknowledged provider admitted");}
    {Fixture f;f.spec.dt_bits=0;check(f.bind()==MT2FW_MT2_INVALID,"variable dt admitted");}
    {Fixture f;f.ready();f.receipt=f.kernel.boundary();check(f.originals==1&&f.services==1&&f.receipt.completed_tick==1,"one boundary did not run once");
        check(f.receipt.actual_role==MT2FW_MT2_SESSION_CLIENT,"Client was presented as Host");
        auto r=MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};check(f.retained(f.continuation,f.saved_invocation,&r)==MT2FW_MT2_INVALID&&f.originals==1,"escaped Next replayed native work");
        f.mode=7;f.receipt=f.kernel.boundary();check(f.originals==2&&f.receipt.completed_tick==2&&f.receipt.tick==2,"next admitted tick did not progress");}
    {Fixture f;f.ready();f.mode=1;f.receipt=f.kernel.boundary();check(f.originals==0&&f.receipt.tick==1,"waiting consumed a tick");}
    {Fixture f;f.ready();f.mode=2;f.receipt=f.kernel.boundary();check(f.originals==0&&f.receipt.outcome==MT2FW_MT2_STEP_UNKNOWN,"consumer fabricated native completion");}
    {Fixture f;f.ready();f.mode=3;f.receipt=f.kernel.boundary();check(f.receipt.outcome==MT2FW_MT2_STEP_UNKNOWN&&std::strstr(f.receipt.cause,"command failed"),"original command cause lost");
        f.mode=0;f.kernel.boundary();check(f.originals==0,"failed command was replayed");}
    {Fixture f;f.ready();f.mode=4;f.receipt=f.kernel.boundary();check(f.receipt.phase==MT2FW_MT2_STEP_FAILED&&f.originals==0,"nested Go was not rejected");}
    {Fixture f;f.ready();f.mode=5;f.receipt=f.kernel.boundary();check(f.receipt.phase==MT2FW_MT2_STEP_FAILED&&f.originals==0,"retirement continued native work");
        check(f.api.unbind(f.api.context,f.receipt.lease,&f.receipt)==MT2FW_MT2_OK,"quiescent retirement failed");
        check(f.kernel.held()&&!f.kernel.reset_after_world_exit(),"retirement silently resumed the world");
        f.proof.world_exited=true;check(f.kernel.reset_after_world_exit()&&!f.kernel.held(),"proved native exit did not clear retained hold");}
    {Fixture f;f.ready();f.mode=6;f.receipt=f.kernel.boundary();check(f.originals==1&&f.receipt.outcome==MT2FW_MT2_STEP_UNKNOWN,"duplicate Next replayed or claimed success");}
    {Fixture f;f.ready();f.throw_advance=true;f.receipt=f.kernel.boundary();check(f.originals==1&&f.receipt.original_attempted==1&&f.receipt.original_returned==0,"partial native attempt was hidden");
        f.kernel.boundary();check(f.originals==1&&std::strstr(f.receipt.cause,"original cause"),"partial native failure was replayed or lost");}
    {Fixture f;f.ready();f.bad_timer=true;f.receipt=f.kernel.boundary();check(f.receipt.original_returned==1&&f.receipt.outcome==MT2FW_MT2_STEP_UNKNOWN,"bad Timer count became confirmed");}
    {Fixture f;f.ready();f.proof.world_epoch=10;f.receipt=f.kernel.boundary();check(f.originals==0&&f.receipt.phase==MT2FW_MT2_STEP_FAILED,"retired world advanced");}
    {Fixture f;f.ready();f.proof.role=MT2FW_MT2_SESSION_HOST;f.receipt=f.kernel.boundary();check(f.originals==0&&f.receipt.actual_role==MT2FW_MT2_SESSION_CLIENT,"role change bypassed lease");}
    {Fixture f;f.ready();f.thread=8;f.receipt=f.kernel.boundary();check(f.originals==0&&f.receipt.phase==MT2FW_MT2_STEP_FAILED,"foreign thread advanced");}
    {Fixture f;f.ready();f.throw_service=true;f.receipt=f.kernel.boundary();check(f.originals==0&&std::strstr(f.receipt.cause,"held transport cause"),"held service failure was suppressed");}
    {Fixture f;f.ready();f.read_inside=true;f.receipt=f.kernel.boundary();
        check(f.originals==1&&f.receipt.original_returned==1&&f.receipt.phase==MT2FW_MT2_STEP_HELD,"original-running read remained set after return");}
    {Fixture f;f.proof.bootstrap=false;check(f.bind()==MT2FW_MT2_OK,"startup bind");
        check(f.receipt.actual_role==MT2FW_MT2_SESSION_CLIENT&&!f.receipt.seed_returned,"bind must retain role without seeding");
        auto life=MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};life.generation=8;life.seed_attempted=life.seed_returned=1;life.workers_held=1;
        f.kernel.lifecycle(life);auto r=f.kernel.maintenance();check(r.seed_returned&&f.services==1&&f.originals==0,"armed maintenance performed an original");
        life.phase=MT2FW_MT2_STEP_LOADING;life.bootstrap_held=1;life.bootstrap_coverage=MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS;life.world_epoch=9;life.initial_witness_sha256[0]=42;f.kernel.lifecycle(life);
        f.proof.bootstrap=true;f.receipt=f.kernel.snapshot();check(f.receipt.phase==MT2FW_MT2_STEP_LOADING,"actual loading state was not retained");
        check(f.activate()==MT2FW_MT2_OK&&f.originals==0,"loading activation did not remain held");}
    {Fixture f;f.spec.service=[](void* p,const MT2FW_MT2_StepReceipt* r){auto& t=*static_cast<Fixture*>(p);
        check(r->callbacks_inflight==1&&r->phase==MT2FW_MT2_STEP_ARMED,"startup callback receipt differs");
        check(t.activate()==MT2FW_MT2_OK&&t.receipt.callbacks_inflight==1,"startup service cannot activate");return MT2FW_MT2_OK;};
        check(f.bind()==MT2FW_MT2_OK,"startup activation bind");auto r=f.kernel.maintenance();
        check(r.phase==MT2FW_MT2_STEP_HELD&&!r.callbacks_inflight&&f.originals==0,"startup activation lent Next or retained callback");
        f.mode=1;f.kernel.boundary();check(f.originals==0,"held boundary unexpectedly ran Go");}
    {Fixture f;f.spec.service=[](void* p,const auto*){auto& t=*static_cast<Fixture*>(p);t.kernel.maintenance();return MT2FW_MT2_OK;};
        check(f.bind()==MT2FW_MT2_OK,"reentry bind");auto r=f.kernel.maintenance();check(r.phase==MT2FW_MT2_STEP_FAILED&&f.originals==0&&!r.callbacks_inflight,"startup service reentry admitted");}
    {Fixture f;check(f.bind()==MT2FW_MT2_OK,"failure bind");f.throw_service=true;auto r=f.kernel.maintenance();
        check(r.phase==MT2FW_MT2_STEP_FAILED&&std::strstr(r.cause,"held transport cause"),"startup failure cause lost");
        const auto terminal=f.kernel.maintenance();
        check(f.services==2&&f.originals==0&&terminal.phase==MT2FW_MT2_STEP_FAILED&&terminal.outcome==r.outcome&&
            terminal.completed_tick==r.completed_tick&&!terminal.callbacks_inflight&&std::strcmp(terminal.cause,r.cause)==0,
            "terminal local service replayed native work or lost the retained failure");}
    {Fixture f;check(f.bind()==MT2FW_MT2_OK,"role-change bind");f.proof.role=MT2FW_MT2_SESSION_HOST;
        auto r=f.kernel.maintenance();check(r.phase==MT2FW_MT2_STEP_FAILED&&f.services==0&&f.originals==0,"preload role changed within frozen lease");}
    for(unsigned coverage:{0u,1u,2u}){Fixture f;f.proof.bootstrap=false;check(f.bind()==MT2FW_MT2_OK,"coverage bind");
        auto life=MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};life.generation=1;life.seed_attempted=life.seed_returned=1;
        life.bootstrap_held=coverage==1?0u:1u;life.bootstrap_coverage=coverage;life.world_epoch=9;life.initial_witness_sha256[0]=42;
        bool rejected=false;try{f.kernel.lifecycle(life);}catch(const std::exception& e){rejected=std::strstr(e.what(),"coverage")!=nullptr;}
        check(rejected&&f.kernel.snapshot().bootstrap_coverage==0,"missing/unknown/premature Developer coverage accepted");}
    {Fixture f;f.proof.bootstrap=false;check(f.bind()==MT2FW_MT2_OK,"uncovered activation bind");f.proof.bootstrap=true;
        check(f.activate()==MT2FW_MT2_UNAVAILABLE&&std::strstr(f.receipt.cause,"Developer coverage"),"old uncovered bootstrap authorized activation");}
    {Fixture f;f.ready();f.receipt=f.kernel.boundary();const auto lease=f.receipt.lease;
        f.leave_service=true;f.receipt=f.kernel.boundary();check(f.originals==1&&f.receipt.phase==MT2FW_MT2_STEP_RETIRING&&f.receipt.completed_tick==1&&!f.receipt.callbacks_inflight,"Leave service lent Next or lost completed tick");
        f.kernel.boundary();check(f.originals==1,"retiring world lent Next");
        check(!f.kernel.reset_after_world_exit(),"bound callbacks were discarded on exit");
        check(f.api.unbind(f.api.context,lease,&f.receipt)==MT2FW_MT2_OK,"teardown callback retirement failed");
        check(!f.kernel.reset_after_world_exit(),"unbind manufactured native world exit");
        f.proof.world_exited=true;f.proof.quiescent=false;check(!f.kernel.reset_after_world_exit(),"running exited workers released");
        f.proof.quiescent=true;check(f.kernel.reset_after_world_exit(),"verified exited-world hold not reset");
        f.leave_service=false;f.ready();check(f.receipt.lease>lease,"second world reused retired lease");
        f.receipt=f.kernel.boundary();check(f.originals==2&&f.receipt.completed_tick==1,"same-process second bind did not run its first original");}
    std::cout<<checks<<" native step provider checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"Step provider failed after "<<checks<<": "<<error.what()<<'\n';return 1;}}
