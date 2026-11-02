#include <nlohmann/json.hpp>
#include <map>
#include <array>
#include <algorithm>
#include <intrin.h>
#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
using Json=nlohmann::json;
using DWORD=unsigned long;
DWORD actual_thread=7;
DWORD GetCurrentThreadId(){return actual_thread;}
namespace mt2::determinism {struct Gate{enum class Phase{Inactive,Loading,Held,Stepping};};}
struct Control{using Phase=mt2::determinism::Gate::Phase;Phase phase=Phase::Loading;bool entered=true;uint64_t completed=0;bool active()const{return phase!=Phase::Inactive;}}control;
struct Log{void log(const char*,const char*,const std::string&) {}} log_instance;Log* runtime=&log_instance;
uintptr_t owner=0x1000,image=0x140000000,held_tasks=0;
constexpr uintptr_t fake_tasks=0x2000,snapshot=0x3000,finished=0x4000;
DWORD thread=7;
std::unordered_map<uintptr_t,uint64_t> memory;
uintptr_t fail_write=0;unsigned writes=0,unlocks=0,finishes=0;
template<class T>T read(uintptr_t a){auto i=memory.find(a);if(i==memory.end())throw std::runtime_error("fake missing address");return T(i->second);}
template<class T>void write(uintptr_t a,T value){++writes;if(a==fail_write)throw std::runtime_error("injected store failure");memory[a]=uint64_t(value);}
namespace timer_step{struct Frame{bool active=true;uint64_t entered=10;}frame;void current_game(){if(read<uintptr_t>(image+0x13df8f0)!=owner)throw std::runtime_error("Game changed");}}
namespace motion_probe {size_t finish_before(uintptr_t){return SIZE_MAX;}void finish_after(size_t,uintptr_t){}}
#include "loading-body.inc"
namespace d=loading_dispatch;
unsigned checks=0;
void check(bool b){++checks;if(!b)throw std::runtime_error("check "+std::to_string(checks));}
template<class F>void rejects(F f,const std::string& part){bool caught=false;try{f();}catch(const std::exception& e){caught=true;check(std::string(e.what()).find(part)!=std::string::npos);}check(caught);}
void native_unlock(void*){++unlocks;}
bool original_raises=false;
bool registered(uintptr_t task){for(auto pair:{std::pair{finished,size_t(0xc0)},std::pair{snapshot,size_t(0xd8)}}){for(int32_t i=0;i<read<int32_t>(fake_tasks+pair.second);++i)if(read<uintptr_t>(pair.first+size_t(i)*8)==task)return true;}return false;}
std::map<uintptr_t,unsigned> per_task_finishes;
std::vector<uintptr_t> callback_order;
unsigned nonplan_finishes=0,end_thinks=0,special_effects=0,native_deletes=0,native_runs=0;
void native_finish(void* task){
    ++finishes;const auto address=reinterpret_cast<uintptr_t>(task);
    check(registered(address)&&read<uint8_t>(address+0xe)==0);
    if(original_raises)throw std::runtime_error("original failed");
    callback_order.push_back(address);++per_task_finishes[address];
    if(read<uint8_t>(address+0xfd))++special_effects;
    ++end_thinks;memory[address+0xd]=1;
}
void reset(){per_task_finishes.clear();callback_order.clear();
    nonplan_finishes=end_thinks=special_effects=native_deletes=native_runs=0;memory.clear();control={};thread=actual_thread=7;held_tasks=0;timer_step::frame={};fail_write=0;writes=unlocks=finishes=0;original_raises=false;
    d::ordered_finish::enabled=false;d::ordered_finish::batches=d::ordered_finish::admitted=d::ordered_finish::moved_slots=d::ordered_finish::stale_visits=d::ordered_finish::last_frame=0;d::ordered_finish::order_hash=14695981039346656037ull;d::ordered_finish::first_ids.clear();d::ordered_finish::last_ids.clear();
    memory[image+0x13df8f0]=owner;memory[image+0x13df340]=fake_tasks;memory[owner+0x68]=fake_tasks;memory[fake_tasks]=image+0x16fdaf0;memory[fake_tasks+0xf8]=0;
    memory[fake_tasks+0xd0]=snapshot;memory[fake_tasks+0xb8]=finished;memory[fake_tasks+0xc0]=0;memory[fake_tasks+0xc4]=16;
    memory[image+d::Plan+0x18]=image+0x301540;
    d::enabled=true;d::step_drain=false;d::quit_empty_dispatch=false;d::uncertain=false;d::partitions=d::deferred_visits=d::first_defer_frame=d::first_release_frame=d::release_admitted=d::finish_entered=d::finish_returned=0;d::pending=0;d::first_error.clear();d::original_unlock=native_unlock;d::original_finish=native_finish;
}
void batch(const std::vector<bool>& plans){int32_t count=int32_t(plans.size());memory[fake_tasks+0xd8]=count;memory[fake_tasks+0xdc]=count;memory[fake_tasks+0xc0]=0;
    for(size_t i=0;i<plans.size();++i){auto task=uintptr_t(0x10000+i*0x100);memory[snapshot+i*8]=task;memory[task]=image+(plans[i]?d::Plan:0x123456);memory[task+8]=2;memory[task+0xc]=memory[task+0xd]=memory[task+0xe]=memory[task+0xfd]=0;}}
void pump_unlock(){d::unlock_call(reinterpret_cast<void*>(fake_tasks+0xf0),image+d::PumpUnlockReturn);}
void next_snapshot(){auto count=read<int32_t>(fake_tasks+0xc0);for(int32_t i=0;i<count;++i)memory[snapshot+size_t(i)*8]=memory[finished+size_t(i)*8];memory[fake_tasks+0xd8]=count;memory[fake_tasks+0xdc]=count;memory[fake_tasks+0xc0]=0;}
namespace o=d::ordered_finish;
void owned_batch(const std::vector<int32_t>& ids){
    reset();control.phase=Control::Phase::Stepping;o::enabled=true;
    batch(std::vector<bool>(ids.size(),true));
    memory[image+0x13df690]=0x60000;memory[0x60000]=image+0x1725f40;memory[0x60040]=0x70000;memory[0x60048]=ids.size();memory[0x6004c]=ids.size();memory[0x60068]=10000;
    for(size_t i=0;i<ids.size();++i){const auto task=uintptr_t(0x10000+i*0x100),subscriber=uintptr_t(0x30000+i*0x100),toon=uintptr_t(0x40000+i*0x100),brain=uintptr_t(0x50000+i*0x100);
        memory[0x70000+i*8]=subscriber;memory[subscriber]=image+0x16e75f0;memory[subscriber+0x20]=uint64_t(ids[i]);memory[subscriber+0x80]=toon;memory[subscriber+0x88]=brain;
        memory[task+0x20]=subscriber;memory[task+0x10]=toon;memory[task+0xc]=0;memory[task+0xd]=0;memory[task+0xfd]=0;
        memory[toon]=image+0x1744e90;memory[toon+0x940]=subscriber;memory[brain]=image+0x16fdd60;memory[brain+0x18]=toon;memory[brain+0x20]=task;memory[brain+0x28]=subscriber;
    }
}
std::vector<int32_t> snapshot_ids(){std::vector<int32_t> out;for(int32_t i=0;i<read<int32_t>(fake_tasks+0xd8);++i){auto task=read<uintptr_t>(snapshot+size_t(i)*8);out.push_back(read<int32_t>(read<uintptr_t>(task+0x20)+0x20));}return out;}
void copied(const std::vector<uintptr_t>& tasks){
    memory[fake_tasks+0xd8]=tasks.size();memory[fake_tasks+0xdc]=tasks.size();memory[fake_tasks+0xc0]=0;
    for(size_t i=0;i<tasks.size();++i)memory[snapshot+i*8]=tasks[i];
}
std::vector<uintptr_t> retained(){std::vector<uintptr_t> out;for(int32_t i=0;i<read<int32_t>(fake_tasks+0xc0);++i)out.push_back(read<uintptr_t>(finished+size_t(i)*8));return out;}
void complete(){d::StepDrain scope;pump_unlock();for(int32_t i=0;i<read<int32_t>(fake_tasks+0xd8);++i)
    d::finish_call(reinterpret_cast<void*>(read<uintptr_t>(snapshot+size_t(i)*8)),image+d::PumpFinishReturn);}
// Model only the pinned Pump callback/complete/orphan sequence. Queue
// admission and Finish guards above are extracted production code.
void pump_native(){
    pump_unlock();
    const auto count=read<int32_t>(fake_tasks+0xd8);
    for(int32_t i=0;i<count;++i){const auto task=read<uintptr_t>(snapshot+size_t(i)*8);
        if(d::plan(task))d::finish_call(reinterpret_cast<void*>(task),image+d::PumpFinishReturn);
        else{++nonplan_finishes;callback_order.push_back(task);}
        memory[task+0xe]=1;
        if(read<uint8_t>(task+0xd))++native_deletes;
    }
}
int main(){try{
    // Exhaust every completion arrival order and every early/late split.
    std::vector<int32_t> permutation={0,1,2,3};uint64_t canonical_hash=0;
    do{for(size_t split=0;split<=4;++split){
        owned_batch({9,2,7,5});std::vector<uintptr_t> early,late;
        for(size_t i=0;i<4;++i)(i<split?early:late).push_back(0x10000+size_t(permutation[i])*0x100);
        copied(early);pump_unlock();check(read<int32_t>(fake_tasks+0xd8)==0&&finishes==0&&o::batches==0);
        auto all=retained();all.insert(all.end(),late.begin(),late.end());copied(all);complete();
        check(snapshot_ids()==std::vector<int32_t>({2,5,7,9}));check(finishes==4&&d::pending==0&&o::admitted==4&&o::batches==1&&!d::step_drain);
        if(!canonical_hash)canonical_hash=o::order_hash;check(o::order_hash==canonical_hash);
    }}while(std::next_permutation(permutation.begin(),permutation.end()));
    // Repeated early native Pumps retain each pointer once, with no Finish.
    owned_batch({9,2});pump_unlock();for(unsigned i=0;i<3;++i){next_snapshot();pump_unlock();check(d::pending==2&&finishes==0);}
    next_snapshot();complete();check(finishes==2&&d::release_admitted==2&&!d::step_drain);
    // Loading plans survive the first admitted early Pump until full drain.
    owned_batch({9,2});control.phase=Control::Phase::Loading;pump_unlock();check(d::pending==2);
    control.phase=Control::Phase::Stepping;next_snapshot();pump_unlock();check(finishes==0&&d::pending==2);
    next_snapshot();complete();check(finishes==2&&d::pending==0&&o::batches==1);
    // Native non-Toon work stays in its original callback slots/order.
    owned_batch({9,2,7});memory[0x10100]=image+0x123456;pump_unlock();
    check(read<int32_t>(fake_tasks+0xd8)==1&&memory[snapshot]==0x10100&&d::pending==2&&finishes==0);
    next_snapshot();complete();check(snapshot_ids()==std::vector<int32_t>({7,9})&&finishes==2);
    // Admission is scoped, non-reentrant, and cannot leak after native throw.
    owned_batch({9});rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"drain admission");check(finishes==0);
    {d::StepDrain outer;check(d::step_drain);rejects([&]{d::StepDrain inner;},"reentrant");check(d::step_drain);}check(!d::step_drain);
    owned_batch({9});original_raises=true;rejects(complete,"original failed");check(!d::step_drain&&finishes==1&&d::finish_entered==1&&d::finish_returned==0);
    owned_batch({9});control.phase=Control::Phase::Loading;{d::StepDrain loading;check(!d::step_drain);}pump_unlock();check(finishes==0&&d::pending==1);
    owned_batch({9});actual_thread=8;rejects([&]{d::StepDrain wrong;},"game thread");check(!d::step_drain);
    owned_batch({9});d::enabled=false;{d::StepDrain disabled;check(!d::step_drain);}check(finishes==0);
    // Unknown mutation stays visible and original unlock remains exactly once.
    owned_batch({9,2});fail_write=finished+8;rejects(pump_unlock,"injected store");check(d::uncertain&&unlocks==1&&finishes==0);rejects(d::cancel,"uncertain");
    // The native cancellation loop must finish before the later full drain.
    owned_batch({9,2,7,5,4,8});memory[0x1010c]=memory[0x1030c]=1;
    memory[0x103fd]=1;memory[0x10200]=image+0x123456;
    pump_native();
    check(callback_order==std::vector<uintptr_t>({0x10100,0x10200,0x10300}));
    check(finishes==2&&nonplan_finishes==1&&end_thinks==2&&special_effects==1&&native_deletes==2);
    check(retained()==std::vector<uintptr_t>({0x10000,0x10400,0x10500})&&d::pending==3&&o::batches==0);
    check(read<uint8_t>(0x1010e)==1&&read<uint8_t>(0x1030e)==1&&read<uint8_t>(0x1000e)==0);
    check(per_task_finishes[0x10100]==1&&per_task_finishes[0x10300]==1&&per_task_finishes[0x10000]==0);
    const auto cancelled_done=read<uint8_t>(0x1010e)&&read<uint8_t>(0x1030e);
    check(cancelled_done&&native_runs==0&&d::finish_entered==2&&d::finish_returned==2);
    next_snapshot();pump_native();check(finishes==2&&native_deletes==2&&d::pending==3);
    next_snapshot();{d::StepDrain drain;pump_native();}
    check(snapshot_ids()==std::vector<int32_t>({4,8,9})&&finishes==5&&native_deletes==5&&native_runs==0);
    check(o::admitted==3&&d::pending==0&&per_task_finishes[0x10100]==1&&per_task_finishes[0x10300]==1);
    // A completed task cannot enter the new completion path a second time.
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10100),image+d::PumpFinishReturn);},"type/completion");
    check(finishes==5);
    // Loading ownership is unchanged; cancellation admission is Step-only.
    owned_batch({9});memory[0x1000c]=1;control.phase=Control::Phase::Loading;
    pump_unlock();check(d::pending==1&&finishes==0);
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"drain admission");
    control.phase=Control::Phase::Held;
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"drain admission");
    // Exact original Pump caller, scheduler, task type and Finish slot remain required.
    owned_batch({9});memory[0x1000c]=1;
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn+1);},"caller differs");
    check(finishes==0);
    owned_batch({9});memory[0x1000c]=1;memory[owner+0x68]=0;
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"owner/address");check(finishes==0);
    owned_batch({9});memory[0x1000c]=1;memory[image+d::Plan+0x18]=image+0x301541;
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"Finish slot changed");check(finishes==0);
    owned_batch({9});memory[0x1000c]=1;memory[0x10008]=3;
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"type/completion");check(finishes==0);
    owned_batch({9});memory[0x1000c]=1;actual_thread=8;
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"game thread");check(finishes==0);
    owned_batch({9});memory[0x1000c]=1;control.entered=false;
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"admitted outer");check(finishes==0);
    owned_batch({9});memory[0x1000c]=1;original_raises=true;
    rejects(pump_native,"original failed");
    check(finishes==1&&d::finish_entered==1&&d::finish_returned==0&&memory[0x1000e]==0&&native_deletes==0&&native_runs==0);
    check(d::first_error=="original failed");
    std::cout<<checks<<" complete-step native body checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
