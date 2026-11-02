#include "native_client_periodic_state.h"
#include "native_blocking_save_scope.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <iostream>
#include <map>
#include <memory>
#include <functional>
using Json=nlohmann::json;
namespace mt2=mt2fw::mt2;
unsigned checks=0,native_calls=0,native_unlocks=0;
void require(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
void check(bool value,const char* cause){++checks;require(value,cause);}
uintptr_t image=0x140000000,owner=0x1000;
uint64_t native_thread=7;
uint64_t GetCurrentThreadId(){return native_thread;}
std::map<uintptr_t,uint64_t> memory;
template<class T>T read(uintptr_t address){const auto found=memory.find(address);require(found!=memory.end(),"unmapped fixture read");return T(found->second);}
struct {uintptr_t game=owner,mode=0x2000;uint64_t world_epoch=9;}bootstrap;
struct {uint64_t generation=3;}lifecycle;
struct Control{enum class Phase{Held,Stepping};Phase phase=Phase::Stepping;bool entered=true;}control;
bool outer_entered=true,held_valid=true,post_failure=false;
namespace timer_step {struct {bool active=true;}frame;}
struct NativeSteps {MT2FW_MT2_StepReceipt value{};auto snapshot(){return value;}}steps;
NativeSteps* native_steps=&steps;
void validate_save_mode(void* receiver){
    require(reinterpret_cast<uintptr_t>(receiver)==bootstrap.mode&&read<uintptr_t>(owner+0x28)==bootstrap.mode&&
        read<int32_t>(bootstrap.mode+0x30c)==0,"native Mode or zero Save count differs");
}
uintptr_t tasks(){return 0x4000;}
void held(){require(held_valid,"native scheduler is not held");}
namespace async_save {std::unique_ptr<int>retained;std::atomic<void*>published{nullptr};}
namespace snapshot_prepare {
enum class Phase{Idle,Armed,Prepared,Sealed,Consumed,Released,Failed};
Phase phase=Phase::Idle;uint64_t last_order=6;MT2FW_MT2_SnapshotRequest certificate{};
}
struct Log {void log(const char*,const char*,const std::string&) {}}logger;
Log* runtime=&logger;
namespace presentation_rng {void verify(uintptr_t,size_t,const char*){}}
uint32_t native_exception=0;
uint32_t test_native_notify(uintptr_t window){
    ++native_calls;if(native_exception)return native_exception;
    memory[window+0x45f]=post_failure?1:0;
    if(memory[window+0x4ec]){++native_unlocks;memory[window+0x4ec]=0;}
    return 0;
}
#include "client-periodic-ui.inc"
namespace ui=client_periodic_ui;
bool raises(const std::function<void()>& action,const char* cause){
    try{action();}catch(const std::exception& e){return std::strstr(e.what(),cause)!=nullptr;}return false;
}
struct Fixture {
    uintptr_t window=0x3000;
    MT2FW_MT2_SnapshotRequest arm{},sealed{},release{};
    Fixture(){
        ui::reset();memory.clear();native_calls=native_unlocks=native_exception=0;native_thread=7;
        bootstrap={owner,0x2000,9};lifecycle.generation=3;control={};timer_step::frame.active=outer_entered=held_valid=true;post_failure=false;
        async_save::published=nullptr;async_save::retained.reset();snapshot_prepare::last_order=6;
        snapshot_prepare::phase=snapshot_prepare::Phase::Idle;snapshot_prepare::certificate={};
        auto& step=steps.value;step={};step.actual_role=MT2FW_MT2_SESSION_CLIENT;step.phase=MT2FW_MT2_STEP_ENTERED;
        step.lease=5;step.session=11;step.world_epoch=9;step.generation=3;step.thread=7;step.tick=20479;step.invocation=123;
        memory[owner+0x28]=bootstrap.mode;memory[bootstrap.mode+0x90]=window;memory[bootstrap.mode+0x30c]=0;
        memory[window]=image+0x1712570;memory[image+0x1712570+0x1d0]=image+0x81ec20;
        memory[window+0x45f]=1;memory[window+0x359]=1;memory[window+0x4ec]=1;memory[window+0x4e8]=0;
        arm.size=sizeof(arm);arm.operation=MT2FW_MT2_SNAPSHOT_ARM_PERIODIC;arm.lease=5;arm.session=11;
        arm.world_epoch=9;arm.tick=20612;arm.invocation=124;arm.order=7;arm.peer_mask=3;
        std::memcpy(arm.nonce,"12345678-1234-1234-1234-123456789abc",37);
        sealed=arm;sealed.operation=MT2FW_MT2_SNAPSHOT_SEAL;sealed.invocation=0;sealed.witness_sha256[0]=42;
        release=sealed;release.operation=MT2FW_MT2_SNAPSHOT_RELEASE;release.ack_mask=3;
    }
    void retain(){ui::retain(reinterpret_cast<void*>(bootstrap.mode),image+0x65f685);}
    void ready(){
        retain();memory[window+0x4e8]=1;control.phase=Control::Phase::Held;control.entered=false;timer_step::frame.active=false;
        auto& step=steps.value;step.phase=MT2FW_MT2_STEP_HELD;step.completed_tick=arm.tick;step.callbacks_inflight=1;step.workers_held=1;
        ui::arm(arm);ui::seal(sealed);snapshot_prepare::certificate=arm;
        std::memcpy(snapshot_prepare::certificate.witness_sha256,sealed.witness_sha256,32);snapshot_prepare::phase=snapshot_prepare::Phase::Released;
    }
    void complete(){ui::released(release);}
};
int main(){try{
    for(uint64_t lock:{0ull,1ull}){Fixture f;memory[f.window+0x4ec]=lock;f.ready();f.complete();
        check(native_calls==1&&native_unlocks==unsigned(lock)&&memory[f.window+0x45f]==0&&memory[f.window+0x4ec]==0,
            "matching periodic release did not preserve original native lock ownership");
        check(memory[f.window+0x4e8]==1&&memory[bootstrap.mode+0x30c]==0&&ui::state.returned,
            "UI completion changed native saving state or decremented zero Save count");
        f.complete();check(native_calls==1,"native notification was replayed");}
    {Fixture f;f.retain();f.arm.operation=MT2FW_MT2_SNAPSHOT_ARM;ui::arm(f.arm);ui::seal(f.sealed);f.complete();
        check(native_calls==0&&ui::state.phase==mt2::NativeClientPeriodicState::Phase::Delegated,"ordinary Snapshot completed native periodic UI");}
    {Fixture f;memory[f.window]=image+0x1712560;
        check(raises([&]{f.retain();},"address point")&&!native_calls,"ABI table base was accepted as AutosaveWindow address point");}
    {Fixture f;check(raises([&]{ui::retain(reinterpret_cast<void*>(bootstrap.mode),image+0x65f686);},"outside original")&&!native_calls,
        "foreign periodic caller retained UI completion");}
    for(unsigned field=0;field<12;++field){Fixture f;f.ready();
        switch(field){case 0:++steps.value.lease;break;case 1:++steps.value.session;break;case 2:++steps.value.world_epoch;break;
            case 3:++steps.value.generation;break;case 4:native_thread=8;break;case 5:control.entered=true;break;
            case 6:timer_step::frame.active=true;break;case 7:--steps.value.completed_tick;break;
            case 8:steps.value.phase=MT2FW_MT2_STEP_FAILED;break;case 9:steps.value.actual_role=MT2FW_MT2_SESSION_HOST;break;
            case 10:outer_entered=false;break;default:steps.value.callbacks_inflight=0;}
        check(raises([&]{f.complete();},"post-request held service")&&!native_calls,"stale/reentrant native owner completed periodic UI");}
    {Fixture f;f.ready();memory[bootstrap.mode+0x30c]=1;
        check(raises([&]{f.complete();},"zero Save count")&&!native_calls,"local Save was decremented or completed");}
    {Fixture f;f.ready();async_save::retained=std::make_unique<int>(1);
        check(raises([&]{f.complete();},"local native writer")&&!native_calls,"foreign writer completed periodic UI");}
    {Fixture f;f.ready();held_valid=false;check(raises([&]{f.complete();},"scheduler")&&!native_calls,"released workers completed periodic UI");}
    {Fixture f;f.ready();memory[bootstrap.mode+0x90]=0x3001;
        check(raises([&]{f.complete();},"no longer belongs")&&!native_calls,"replaced Window completed periodic UI");}
    {Fixture f;f.ready();memory[f.window+0x4e8]=0;
        check(raises([&]{f.complete();},"unwound visible")&&!native_calls,"original SetMode stack was not allowed to unwind");}
    {Fixture f;f.ready();snapshot_prepare::phase=snapshot_prepare::Phase::Failed;
        check(raises([&]{f.complete();},"successful ordinary")&&!native_calls,"failed Snapshot completed UI");}
    {Fixture f;f.ready();snapshot_prepare::certificate.operation=MT2FW_MT2_SNAPSHOT_ARM;
        check(raises([&]{f.complete();},"different purpose")&&!native_calls,"generic Snapshot replaced periodic certificate");}
    {Fixture f;f.ready();native_exception=0xc0000005;
        check(raises([&]{f.complete();},"native exception")&&ui::state.attempted&&!ui::state.returned&&native_calls==1,
            "native exception lost its consumed completion attempt");
        check(raises([&]{f.complete();},"failed")&&native_calls==1,"native fault was retried");}
    {Fixture f;f.ready();post_failure=true;
        check(raises([&]{f.complete();},"without hiding")&&ui::state.attempted&&ui::state.returned,
            "failed native postcondition was reported as success");}
    std::cout<<"PASS "<<checks<<" extracted Client periodic native UI checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
