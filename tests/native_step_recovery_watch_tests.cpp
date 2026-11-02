#include "native_step_recovery.h"
#include "native_blocking_save_scope.h"
#include <atomic>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <functional>

namespace mt2=mt2fw::mt2;
int checks=0;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
struct {uintptr_t mode=0x1000;} bootstrap;
uintptr_t held_tasks=0x2000;
int32_t native_saves=0;
bool held_context=true,clock_valid=true,queues_empty=true;
int initial_reads=0,shape_reads=0,clock_reads=0,queue_reads=0;
template<class T>T read(uintptr_t address){require(address==bootstrap.mode+0x30c,"unexpected native read");return T(native_saves);}
namespace snapshot_prepare {
enum class Phase{Idle,Armed,Prepared,Sealed,Consumed,Released,Failed};
Phase phase=Phase::Idle;
MT2FW_MT2_SnapshotRequest certificate{};
uint64_t last_order=0;
uintptr_t prepared_mode=0,prepared_tasks=0;
bool capture_returned=false;
struct {uint64_t lease=0;} recovery_owner;
void shape(const MT2FW_MT2_SnapshotRequest& request){++shape_reads;require(request.size==sizeof(request),"snapshot shape differs");}
bool empty_hash(const MT2FW_MT2_SnapshotRequest& request){for(auto b:request.witness_sha256)if(b)return false;return true;}
void clock(){++clock_reads;require(clock_valid&&native_saves==0,"clock or native Save count differs");}
void empty_tasks(){++queue_reads;require(queues_empty,"native queues not empty");}
}
namespace async_save {
struct Capture {
    MT2FW_MT2_SnapshotRequest certificate{};
    mt2::PreparedSaveState save;
    std::atomic<uintptr_t> task{0};
    bool entry_returned=false,complete=false;
    unsigned worker_progress=0;
};
std::unique_ptr<Capture> retained;
std::atomic<Capture*> published{nullptr};
Capture& held_owner(){
    auto* p=published.load();require(p&&p==retained.get()&&held_context&&clock_valid,"async native owner differs");return *p;
}
}
namespace recovery_runtime {
bool attempted=false,capture_consumed=false;
void initial_tasks(){++initial_reads;require(queues_empty,"initial native task owner differs");}
#include "native_step_recovery_watch.inc"
}
using Phase=snapshot_prepare::Phase;
using Proof=mt2::NativeStepRecovery::LocalProof;
using namespace recovery_runtime;
struct Fixture {
    MT2FW_MT2_RecoverySpec spec{};
    MT2FW_MT2_StepReceipt step{};
    Fixture(){
        watch_reset();attempted=capture_consumed=false;snapshot_prepare::phase=Phase::Idle;
        snapshot_prepare::certificate={};snapshot_prepare::recovery_owner.lease=0;snapshot_prepare::last_order=0;
        snapshot_prepare::prepared_mode=snapshot_prepare::prepared_tasks=0;snapshot_prepare::capture_returned=false;
        async_save::published=nullptr;async_save::retained.reset();native_saves=0;
        held_context=clock_valid=queues_empty=true;initial_reads=shape_reads=clock_reads=queue_reads=0;
        spec.original.physical_ids[0]=100;spec.original.physical_ids[1]=101;
        step.lease=5;step.session=11;step.world_epoch=9;step.completed_tick=1968;step.generation=3;
        step.thread=7;step.actual_role=MT2FW_MT2_SESSION_HOST;
    }
    void prepared(){
        auto& c=snapshot_prepare::certificate;c={};c.size=sizeof(c);c.operation=MT2FW_MT2_SNAPSHOT_ARM;
        c.lease=step.lease;c.session=step.session;c.world_epoch=step.world_epoch;c.tick=step.completed_tick;
        c.invocation=123;c.order=5;c.peer_mask=3;
        std::memcpy(c.nonce,"12345678-1234-1234-1234-123456789abc",37);
        snapshot_prepare::last_order=c.order;snapshot_prepare::prepared_mode=bootstrap.mode;
        snapshot_prepare::prepared_tasks=held_tasks;snapshot_prepare::phase=Phase::Prepared;
    }
    void sealed(){snapshot_prepare::certificate.witness_sha256[0]=42;snapshot_prepare::phase=Phase::Sealed;}
    void writer(){
        sealed();snapshot_prepare::phase=Phase::Consumed;
        auto value=std::make_unique<async_save::Capture>();value->certificate=snapshot_prepare::certificate;
        value->entry_returned=true;value->task=0x3000;value->save={bootstrap.mode,held_tasks,uint32_t(step.thread),1,1,0,0,true};
        async_save::retained=std::move(value);async_save::published=async_save::retained.get();native_saves=1;queues_empty=false;
    }
    void begin(bool initial=false){healthy_watch(spec,step,Proof::WatchBegin,initial);}
    void end(bool initial=false){healthy_watch(spec,step,Proof::WatchEnd,initial);}
    void cycle(){begin();end();}
    void release(){
        async_save::published=nullptr;async_save::retained.reset();native_saves=0;queues_empty=true;
        snapshot_prepare::capture_returned=true;snapshot_prepare::phase=Phase::Released;
    }
};
bool raises(const std::function<void()>& operation,const char* cause){
    try{operation();}catch(const std::exception& e){return std::strstr(e.what(),cause)!=nullptr;}return false;
}
int main(){try{
    {Fixture f;f.begin(true);f.end(true);adoption_ready();check(initial_reads==2&&!watch_pending,"initial native hold lost its task proof");}
    {Fixture f;f.prepared();const auto original=snapshot_prepare::certificate;f.cycle();
        check(shape_reads==2&&clock_reads==2&&queue_reads==2&&std::memcmp(&original,&snapshot_prepare::certificate,sizeof(original))==0,
            "healthy Prepared watch changed or skipped ordinary native ownership");
        check(raises(adoption_ready,"old pending snapshot"),"actual recovery adopted Prepared");
        f.sealed();f.cycle();check(raises(adoption_ready,"old pending snapshot"),"actual recovery adopted Sealed");
        f.writer();auto* owned=async_save::retained.get();f.begin();owned->worker_progress=1;f.end();
        check(async_save::retained.get()==owned&&async_save::published==owned&&native_saves==1&&!owned->complete,
            "healthy watch pumped, retired or replaced the ordinary writer");
        check(raises(adoption_ready,"native Save writer"),"actual recovery adopted a live ordinary writer");
        owned->complete=true;owned->save.resumes=1;native_saves=0;f.cycle();
        check(raises(adoption_ready,"native Save writer"),"actual recovery adopted an unretired completed ordinary writer");
        f.release();f.cycle();adoption_ready();check(!ordinary_watch_active&&!watch_pending,"ordinary release was not acknowledged");}
    {Fixture f;f.prepared();f.writer();async_save::retained->complete=true;native_saves=0;f.cycle();
        f.release();++f.step.completed_tick;f.prepared();snapshot_prepare::certificate.order=++snapshot_prepare::last_order;
        f.cycle();check(ordinary_watch_owner.tick==f.step.completed_tick,"released Save followed by Next was mistaken for stale ownership");}
    {Fixture f;f.prepared();f.sealed();f.step.actual_role=MT2FW_MT2_SESSION_CLIENT;f.cycle();f.release();f.cycle();adoption_ready();}
    {Fixture f;f.prepared();snapshot_prepare::certificate.operation=MT2FW_MT2_SNAPSHOT_ARM_PERIODIC;f.cycle();f.writer();f.cycle();
        check(ordinary_watch_owner.operation==MT2FW_MT2_SNAPSHOT_ARM_PERIODIC,"healthy WATCH lost explicit periodic purpose");f.release();f.cycle();adoption_ready();}
    {Fixture f;f.prepared();f.sealed();snapshot_prepare::phase=Phase::Consumed;snapshot_prepare::capture_returned=true;f.cycle();}
    for(auto phase:{Phase::Armed,Phase::Failed}){Fixture f;f.prepared();snapshot_prepare::phase=phase;
        check(raises([&]{f.cycle();},"unknown or armed snapshot phase"),"stale snapshot phase passed WATCH");
        check(raises(adoption_ready,"old pending snapshot"),"stale snapshot phase passed adoption");}
    for(int field=0;field<10;++field){Fixture f;f.prepared();auto& c=snapshot_prepare::certificate;
        switch(field){case 0:++c.lease;break;case 1:++c.session;break;case 2:++c.world_epoch;break;case 3:++c.tick;break;
            case 4:c.invocation=0;break;case 5:++c.order;break;case 6:c.peer_mask=1;break;
            case 7:++snapshot_prepare::prepared_mode;break;case 8:++snapshot_prepare::prepared_tasks;break;default:c.operation=MT2FW_MT2_SNAPSHOT_CAPTURE;}
        check(raises([&]{f.cycle();},"identity or native owner differs"),"foreign snapshot identity passed WATCH");}
    {Fixture f;f.prepared();f.begin();snapshot_prepare::certificate.nonce[0]='a';
        check(raises([&]{f.end();},"ordinary snapshot owner changed"),"observer replaced ordinary certificate");}
    {Fixture f;f.prepared();f.sealed();f.begin();++snapshot_prepare::certificate.witness_sha256[0];
        check(raises([&]{f.end();},"changed during observation"),"observer replaced sealed witness");}
    {Fixture f;f.prepared();f.sealed();f.cycle();++snapshot_prepare::certificate.witness_sha256[0];
        check(raises([&]{f.cycle();},"sealed ordinary witness changed"),"sealed witness changed between frames");}
    {Fixture f;f.prepared();f.sealed();f.cycle();snapshot_prepare::phase=Phase::Prepared;snapshot_prepare::certificate.witness_sha256[0]=0;
        check(raises([&]{f.cycle();},"phase went backwards"),"ordinary phase went backwards");}
    {Fixture f;f.prepared();f.writer();f.begin();++async_save::retained->task;
        check(raises([&]{f.end();},"changed during observation"),"observer replaced SaveTask");}
    {Fixture f;f.prepared();f.writer();f.cycle();++async_save::retained->task;
        check(raises([&]{f.cycle();},"writer changed before release"),"ordinary writer changed between frames");}
    for(int field=0;field<8;++field){Fixture f;f.prepared();f.writer();auto& c=*async_save::retained;
        switch(field){case 0:++c.certificate.lease;break;case 1:++c.certificate.witness_sha256[0];break;
            case 2:++c.save.mode;break;case 3:++c.save.tasks;break;case 4:++c.save.thread;break;
            case 5:c.save.asynchronous=false;break;case 6:c.save.saves=2;break;default:c.entry_returned=false;}
        check(raises([&]{f.cycle();},"not owned by the exact ordinary snapshot"),"foreign writer passed WATCH");}
    for(int field=0;field<4;++field){Fixture f;f.prepared();f.writer();
        switch(field){case 0:async_save::published=nullptr;break;case 1:async_save::retained.reset();break;
            case 2:held_context=false;break;default:clock_valid=false;}
        check(raises([&]{f.cycle();},"async native owner differs"),"unowned writer passed WATCH");}
    {Fixture f;f.prepared();f.writer();native_saves=2;check(raises([&]{f.cycle();},"Save count differs"),"multiple native Saves passed WATCH");}
    {Fixture f;f.prepared();f.writer();snapshot_prepare::phase=Phase::Released;
        check(raises([&]{f.cycle();},"unowned native Save writer"),"Released adopted a retained writer");}
    {Fixture f;f.prepared();f.cycle();f.release();snapshot_prepare::certificate.nonce[0]='a';
        check(raises([&]{f.cycle();},"without exact release"),"foreign release retired ordinary watch owner");}
    {Fixture f;f.prepared();f.cycle();snapshot_prepare::phase=Phase::Idle;
        check(raises([&]{f.cycle();},"without exact release"),"ordinary owner disappeared without release");}
    {Fixture f;f.prepared();snapshot_prepare::recovery_owner.lease=1;
        check(raises([&]{f.cycle();},"recovery-owned"),"recovery capture relabeled as ordinary watch");}
    {Fixture f;f.prepared();f.begin();check(raises(adoption_ready,"overlaps"),"adoption ran inside observation");}
    {Fixture f;f.prepared();queues_empty=false;check(raises([&]{f.cycle();},"queues not empty"),"Prepared bypassed native queue proof");}
    std::cout<<"PASS "<<checks<<" production recovery WATCH checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
