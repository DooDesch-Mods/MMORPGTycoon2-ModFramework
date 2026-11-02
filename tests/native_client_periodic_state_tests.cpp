#include "native_client_periodic_state.h"
#include <iostream>
#include <functional>

using State=mt2fw::mt2::NativeClientPeriodicState;
unsigned checks=0;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
bool raises(const std::function<void()>& action,const char* cause){
    try{action();}catch(const std::exception& e){return std::strstr(e.what(),cause)!=nullptr;}return false;
}
struct Fixture {
    State state;
    MT2FW_MT2_StepReceipt step{};
    MT2FW_MT2_SnapshotRequest arm{},sealed{},release{};
    bool owner=true,visible=true,locked=true,notify_throws=false,post_failure=false;
    unsigned notifications=0,unlocks=0,guards=0;
    Fixture(){
        step.actual_role=MT2FW_MT2_SESSION_CLIENT;step.lease=5;step.session=11;step.world_epoch=9;step.generation=3;
        step.thread=7;step.tick=20479;step.invocation=123;
        arm.size=sizeof(arm);arm.operation=MT2FW_MT2_SNAPSHOT_ARM_PERIODIC;arm.lease=step.lease;arm.session=step.session;
        arm.world_epoch=step.world_epoch;arm.tick=20612;arm.invocation=124;arm.order=7;arm.peer_mask=3;
        std::memcpy(arm.nonce,"12345678-1234-1234-1234-123456789abc",37);
        sealed=arm;sealed.operation=MT2FW_MT2_SNAPSHOT_SEAL;sealed.invocation=0;sealed.witness_sha256[0]=42;
        release=sealed;release.operation=MT2FW_MT2_SNAPSHOT_RELEASE;release.ack_mask=3;
    }
    void retain(){state.retain(step,0x1000,0x2000,6);}
    void bind(){check(state.bind(arm),"fresh periodic binding unavailable");}
    void ready(){retain();bind();state.seal(sealed);}
    bool complete(){return state.complete(release,[&]{++guards;if(!owner)throw std::runtime_error("native owner differs");},[&]{
        ++notifications;if(notify_throws)throw std::runtime_error("native NotifyDoneSaving failed");
        visible=false;if(locked){++unlocks;locked=false;}
    },[&]{if(post_failure||visible||locked)throw std::runtime_error("native completion readback differs");});}
};
int main(){try{
    for(bool lock:{false,true}){Fixture f;f.locked=lock;f.ready();check(f.complete(),"matching RELEASE did not complete UI");
        check(f.notifications==1&&f.unlocks==unsigned(lock)&&f.state.attempted&&f.state.returned&&!f.state.pending(),"original callback lock ownership changed");
        check(!f.complete()&&f.notifications==1&&f.state.notify_attempts==1,"completed UI notification replayed");}
    {Fixture f;f.retain();check(!f.complete()&&!f.notifications&&f.state.phase==State::Phase::Delegated,"unassociated arbitrary Save completed UI");}
    for(auto operation:{MT2FW_MT2_SNAPSHOT_ARM,MT2FW_MT2_SNAPSHOT_SEAL,MT2FW_MT2_SNAPSHOT_CAPTURE}){Fixture f;f.retain();f.arm.operation=uint32_t(operation);
        check(!f.state.bind(f.arm)&&!f.complete()&&!f.notifications,"nonperiodic Host snapshot completed periodic notice");}
    {Fixture f;f.retain();f.arm.tick=f.step.tick-1;check(!f.state.bind(f.arm)&&!f.complete(),"pre-request capture closed newer notice");}
    {Fixture f;f.retain();f.arm.order=6;check(!f.state.bind(f.arm)&&!f.complete(),"old capture order closed newer notice");}
    for(unsigned field=0;field<7;++field){Fixture f;f.ready();
        switch(field){case 0:++f.release.lease;break;case 1:++f.release.session;break;case 2:++f.release.world_epoch;break;
            case 3:++f.release.tick;break;case 4:++f.release.order;break;case 5:f.release.nonce[0]='a';break;default:f.release.peer_mask=1;}
        check(raises([&]{f.complete();},"bound snapshot certificate")&&!f.notifications&&!f.state.attempted,
            "foreign RELEASE closed periodic notice");}
    for(unsigned field=0;field<3;++field){Fixture f;f.ready();
        if(field==0)f.release.ack_mask=1;else if(field==1)++f.release.witness_sha256[0];else f.release.operation=MT2FW_MT2_SNAPSHOT_PUMP;
        check(raises([&]{f.complete();},"successful shared RELEASE")&&!f.notifications,"failed/incomplete Save closed periodic notice");}
    {Fixture f;f.retain();f.bind();check(raises([&]{f.complete();},"unsealed")&&!f.notifications,"unsealed periodic completion reached native callback");}
    {Fixture f;f.ready();f.owner=false;check(raises([&]{f.complete();},"native owner differs")&&!f.notifications&&!f.state.attempted,
        "owner mismatch reached native completion");}
    {Fixture f;f.ready();f.notify_throws=true;check(raises([&]{f.complete();},"NotifyDoneSaving failed")&&f.state.attempted&&!f.state.returned,
        "native notification failure lost its attempted receipt");
        check(raises([&]{f.complete();},"failed")&&f.notifications==1&&f.state.cause=="native NotifyDoneSaving failed","failed native notification replayed or lost cause");}
    {Fixture f;f.ready();f.post_failure=true;check(raises([&]{f.complete();},"readback differs")&&f.state.attempted&&f.state.returned&&
        f.state.phase==State::Phase::Failed,"failed post-call proof became confirmed success");}
    {Fixture f;f.ready();check(raises([&]{f.retain();},"unfinished delegated owner"),"new delegation replaced pending owner");}
    {Fixture f;f.ready();f.complete();++f.step.tick;f.retain();check(!f.complete()&&f.notifications==1,"old nonce completed a newer delegation");}
    for(auto role:{MT2FW_MT2_SESSION_HOST,MT2FW_MT2_SESSION_STANDALONE}){Fixture f;f.step.actual_role=uint32_t(role);
        check(raises([&]{f.retain();},"incomplete native identity")&&!f.notifications,"Host or standalone acquired Client UI owner");}
    std::cout<<"PASS "<<checks<<" Client periodic UI state checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
