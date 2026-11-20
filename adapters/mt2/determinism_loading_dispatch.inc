// Included inside lab after timer_step. Native arrays retain task ownership.
namespace loading_dispatch {
using Unlock=void(*)(void*);
using Finish=void(*)(void*);
Unlock original_unlock=nullptr;
Finish original_finish=nullptr;
bool enabled=false,uncertain=false;
bool quit_empty_dispatch=false;
uint64_t partitions=0,deferred_visits=0,first_defer_frame=0,first_release_frame=0;
uint64_t release_admitted=0,finish_entered=0,finish_returned=0;
int32_t pending=0;
std::string first_error;
constexpr uintptr_t Plan=0x1709b90,PumpUnlockReturn=0x52d002,PumpFinishReturn=0x52d075;
void require(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
#ifdef MT2FW_ORDERED_RECOVERY
struct RecoveryDrain {
    inline static thread_local const RecoveryDrain* current=nullptr;
    const MT2FW_MT2_RecoveryReceipt& certificate;
    uintptr_t game,tasks;
    RecoveryDrain(const MT2FW_MT2_RecoveryReceipt& receipt,uintptr_t g,uintptr_t t):certificate(receipt),game(g),tasks(t){
        require(!current&&native_steps->recovery_snapshot()==&receipt&&receipt.prepare_attempted==1&&
            receipt.phase==MT2FW_MT2_RECOVERY_PREPARING&&receipt.interruption&&receipt.generation&&receipt.lease&&
            thread==GetCurrentThreadId()&&g==owner&&t&&control.phase==mt2::determinism::Gate::Phase::Held&&
            !control.entered&&!timer_step::frame.active&&!held_tasks,"Recovery drain lacks its independently consumed no-Go certificate");
        current=this;
    }
    ~RecoveryDrain(){current=nullptr;}
    RecoveryDrain(const RecoveryDrain&)=delete;RecoveryDrain& operator=(const RecoveryDrain&)=delete;
};
bool recovery_draining(){
    const auto* scope=RecoveryDrain::current;if(!scope)return false;
    require(native_steps->recovery_snapshot()==&scope->certificate&&scope->certificate.phase==MT2FW_MT2_RECOVERY_PREPARING&&
        scope->certificate.prepare_attempted==1&&scope->game==owner&&scope->tasks==read<uintptr_t>(image+0x13eb340)&&
        thread==GetCurrentThreadId()&&!control.entered&&!timer_step::frame.active,"Recovery drain owner escaped its no-Go preparation");
    return true;
}
#else
bool recovery_draining(){return false;}
#endif
uintptr_t manager(){
    require(thread==GetCurrentThreadId(),"Loading dispatch game thread changed");
    require((control.entered&&timer_step::frame.active)||recovery_draining(),"Loading dispatch lacks admitted outer frame or exact Recovery drain");
    timer_step::current_game();
    const auto tasks=read<uintptr_t>(image+0x13eb340);
    require(tasks&&read<uintptr_t>(owner+0x68)==tasks&&read<uintptr_t>(tasks)==image+0x1703e90,"Loading dispatch Task owner/address point changed");
    require(!held_tasks&&read<uint8_t>(tasks+0xf8)==0,"Loading dispatch workers are held");
    require(!uncertain,"Loading dispatch previous queue mutation is uncertain");
    return tasks;
}
bool plan(uintptr_t task){
    require(task!=0,"Loading dispatch null task");
    if(read<uintptr_t>(task)!=image+Plan)return false;
    require(read<int32_t>(task+8)==2&&read<uint8_t>(task+0xe)==0,"Loading dispatch ToonPlan type/completion differs");
    require(read<uintptr_t>(image+Plan+0x18)==image+0x304e90,"Loading dispatch ToonPlan Finish slot changed");
    return true;
}
#include "determinism_ordered_finish.inc"
#include "determinism_ordered_run.inc"
#ifdef MT2FW_ORDERED_RECOVERY
#include "native_completed_callbacks.inc"
#else
namespace completed_callbacks {void clear(){} void record(uintptr_t){} bool matches(uintptr_t){return false;}}
#endif
bool step_drain=false;
bool cancelled_completion(uintptr_t task){
    return control.phase==mt2::determinism::Gate::Phase::Stepping&&!step_drain&&
        plan(task)&&read<uint8_t>(task+0xc)==1;
}
struct StepDrain {
    bool active=false;
    StepDrain(){
        if(!enabled||control.phase!=mt2::determinism::Gate::Phase::Stepping)return;
        manager();require(!step_drain,"ToonPlan complete-step drain is reentrant");
        step_drain=true;active=true;
    }
    ~StepDrain(){if(active)step_drain=false;}
    StepDrain(const StepDrain&)=delete;StepDrain& operator=(const StepDrain&)=delete;
};
uint64_t path_child_attempted=0,path_child_returned=0,path_children_observed=0;
Unlock path_lock=nullptr;
uint64_t path_children(uintptr_t tasks){
    require(manager()==tasks&&step_drain,"Path children lack complete-step drain ownership");
    require(path_lock,"Native Pathfinder queue lock is unavailable");path_lock(reinterpret_cast<void*>(tasks+0xf0));
    struct Release {void* mutex;~Release(){original_unlock(mutex);}} release{reinterpret_cast<void*>(tasks+0xf0)};
    constexpr std::array<size_t,7> arrays{0x28,0x40,0x58,0x70,0x88,0xa0,0xb8};
    uint64_t total=0;std::set<uintptr_t> unique;
    for(size_t slot=0;slot<arrays.size();++slot){const auto offset=arrays[slot];
        const auto data=read<uintptr_t>(tasks+offset);const auto count=read<int32_t>(tasks+offset+8),capacity=read<int32_t>(tasks+offset+12);
        require(count>=0&&capacity>=count&&count<=1000000&&(!count||data),"Path child queue bounds differ offset="+std::to_string(offset));
        for(int32_t i=0;i<count;++i){const auto task=read<uintptr_t>(data+size_t(i)*8);
            require(task&&unique.insert(task).second,"Path child queue has absent or duplicate ownership");
            const auto ap=read<uintptr_t>(task);const auto type=read<int32_t>(task+8);
            require((slot==2||slot==5||slot==6)&&ap==image+0x171ae60&&type==2&&read<uint8_t>(task+0xe)==0&&
                read<uintptr_t>(ap+0x18)==image+0x2c8480,
                "Post-ToonPlan work is not a native Pathfinder child: queue="+std::to_string(offset)+" count="+std::to_string(count)+
                " addresspoint="+std::to_string(ap>=image?ap-image:ap)+" type="+std::to_string(type));
            ++total;
        }
    }
    return total;
}
template<class Finish>void finish_complete_step(void* receiver,const Finish& finish){
    completed_callbacks::clear();
    const auto before=finish_returned;
    ordered_run::begin(reinterpret_cast<uintptr_t>(receiver));
    finish(receiver);
    ordered_run::end(reinterpret_cast<uintptr_t>(receiver));
    const auto tasks=reinterpret_cast<uintptr_t>(receiver);
    if(step_drain&&finish_returned!=before){
        const auto children=path_children(tasks);
        if(children){
            path_children_observed+=children;++path_child_attempted;
            // Original ToonPlan completion can enqueue native Pathfinder work.
            // Its pinned no-op Finish cannot create another callback generation.
            finish(receiver);++path_child_returned;
            require(path_children(tasks)==0,"Native Pathfinder child settlement left additional work; no further drain");
        }
    }
    completed_callbacks::record(tasks);
}

void partition(uintptr_t tasks){
    const auto count=read<int32_t>(tasks+0xd8),capacity=read<int32_t>(tasks+0xdc);
    const auto source=read<uintptr_t>(tasks+0xd0),target=read<uintptr_t>(tasks+0xb8);
    require(count>=0&&capacity==count&&read<int32_t>(tasks+0xc0)==0&&read<int32_t>(tasks+0xc4)>=count,
        "Loading dispatch snapshot/count/capacity differs");
    require(!count||(source&&target&&source!=target),"Loading dispatch native arrays absent or aliased");
    std::vector<uintptr_t> kept,held;std::set<uintptr_t> unique;
    for(int32_t i=0;i<count;++i){
        const auto task=read<uintptr_t>(source+size_t(i)*8);
        require(unique.insert(task).second,"Loading dispatch duplicate task ownership");
        // Original CancelTasks pumps until its cancelled task completes. That
        // callback must return before the enclosing Update can reach StepDrain.
        (plan(task)&&!cancelled_completion(task)?held:kept).push_back(task);
    }
    // The original holds +f0 and has just copied +c0 into +d8. The former
    // finished-array allocation can hold every copied pointer without growth.
    // No callback can observe the relocation before both counts are published.
    if(!held.empty()){
        uncertain=true;
        for(size_t i=0;i<held.size();++i)write(target+i*8,held[i]);
        for(size_t i=0;i<kept.size();++i)write(source+i*8,kept[i]);
        write(tasks+0xc0,int32_t(held.size()));write(tasks+0xd8,int32_t(kept.size()));
        uncertain=false;++partitions;deferred_visits+=held.size();
        if(!first_defer_frame)first_defer_frame=timer_step::frame.entered;
    }
    pending=int32_t(held.size());
}
#include "native_save_completion.inc"
#include "native_save_cache_completion.inc"
void before_unlock(void* mutex){
    completed_callbacks::clear();
    if(save_cache::current){save_cache::before_unlock(mutex);return;}
#ifdef MT2FW_ORDERED_ASYNC_SAVE
    if(async_save::before_unlock(mutex))return;
#endif
    if(save_completion::current){save_completion::before_unlock(mutex);return;}
    if(quit_empty_dispatch){
        require(thread==GetCurrentThreadId()&&control.phase==mt2::determinism::Gate::Phase::Held&&
            !control.entered&&!timer_step::frame.active,"Quit dispatch is outside its held command scope");
        const auto tasks=read<uintptr_t>(image+0x13eb340);
        require(tasks&&tasks==held_tasks&&read<uintptr_t>(owner+0x68)==tasks&&read<uintptr_t>(tasks)==image+0x1703e90&&
            reinterpret_cast<uintptr_t>(mutex)==tasks+0xf0,"Quit dispatch Task owner/mutex changed");
        require(read<uint8_t>(tasks+0xf8)==1&&read<int32_t>(tasks+0xa8)==0&&read<int32_t>(tasks+0x90)==0,
            "Quit dispatch workers are not paused/drained");
        // The original autosave Dispatch pumps even an empty finished array.
        // Only that empty snapshot may pass outside an authoritative frame.
        require(!uncertain&&!pending&&read<int32_t>(tasks+0xc0)==0&&read<int32_t>(tasks+0xd8)==0&&read<int32_t>(tasks+0xdc)==0,
            "Quit dispatch has finished callbacks or uncertain ownership");
        return;
    }
    const auto tasks=manager();
    require(reinterpret_cast<uintptr_t>(mutex)==tasks+0xf0,"Loading dispatch mutex owner differs");
    // Recovery consumes original deferred work locally; normal peer ordering
    // and its temporary native queue views are not used in this scope.
    if(recovery_draining()){require(!pending&&!uncertain,"Recovery drain has old deferred/uncertain callback ownership");return;}
    // Early Dispatch may see only the workers that happened to finish first.
    // Keep ToonPlan ownership in the native finished queue until the existing
    // post-Update full drain. Other callbacks and ProcGL retain their order.
    if(control.phase==mt2::determinism::Gate::Phase::Loading||
       (control.phase==mt2::determinism::Gate::Phase::Stepping&&!step_drain)){partition(tasks);return;}
    if(control.phase==mt2::determinism::Gate::Phase::Stepping)ordered_finish::order(tasks);
    if(control.phase==mt2::determinism::Gate::Phase::Stepping&&pending){
        const auto count=read<int32_t>(tasks+0xd8);const auto source=read<uintptr_t>(tasks+0xd0);
        require(count>=pending&&source,"Loading dispatch release snapshot lost deferred tasks");
        int32_t plans=0;for(int32_t i=0;i<count;++i)plans+=plan(read<uintptr_t>(source+size_t(i)*8));
        require(plans>=pending,"Loading dispatch release lacks deferred ToonPlans");
        if(!first_release_frame)first_release_frame=timer_step::frame.entered;
        release_admitted+=uint64_t(pending);pending=0;
    }
}
void unlock_call(void* mutex,uintptr_t caller){
    try{ordered_run::before_unlock(mutex,caller);}
    catch(const std::exception& error){original_unlock(mutex);runtime->log("steps","error","Ordered Run native transition failed: "+std::string(error.what()));throw;}
    catch(...){original_unlock(mutex);runtime->log("steps","error","Ordered Run native transition raised an unknown exception; no replay");throw;}
    // Unrelated threads and all other mutexes only forward the original call.
    if(caller!=image+PumpUnlockReturn||!enabled||!control.active()){original_unlock(mutex);return;}
    try{before_unlock(mutex);}
    catch(const std::exception& e){if(first_error.empty()){first_error=e.what();runtime->log("steps","error",first_error);}original_unlock(mutex);throw;}
    catch(...){if(first_error.empty())first_error="Loading dispatch queue boundary raised; no replay";original_unlock(mutex);throw;}
    original_unlock(mutex);
}
__declspec(noinline) void unlock(void* mutex){unlock_call(mutex,reinterpret_cast<uintptr_t>(_ReturnAddress()));}
void finish_call(void* task,uintptr_t caller){
    if(!enabled||!control.active()){original_finish(task);return;}
    try{
        manager();require(caller==image+PumpFinishReturn,"Loading dispatch ToonPlan Finish caller differs");
        require((control.phase==mt2::determinism::Gate::Phase::Stepping&&step_drain)||recovery_draining()||
            cancelled_completion(reinterpret_cast<uintptr_t>(task)),"ToonPlan Finish lacks complete-step, cancelled-completion or exact Recovery drain admission");
        const auto observation=motion_probe::finish_before(reinterpret_cast<uintptr_t>(task));
        ++finish_entered;original_finish(task);++finish_returned;
        motion_probe::finish_after(observation,reinterpret_cast<uintptr_t>(task));
    }catch(const std::exception& e){if(first_error.empty())first_error=e.what();throw;}
    catch(...){if(first_error.empty())first_error="Loading dispatch original ToonPlan Finish raised; no replay";throw;}
}
__declspec(noinline) void finish(void* task){finish_call(task,reinterpret_cast<uintptr_t>(_ReturnAddress()));}
bool retained_only(uintptr_t tasks){
    if(!enabled||control.phase!=mt2::determinism::Gate::Phase::Loading)return false;
    require(manager()==tasks,"Loading dispatch retained owner differs");
    const auto count=read<int32_t>(tasks+0xc0);const auto source=read<uintptr_t>(tasks+0xb8);
    require(count==pending&&count>=0&&(!count||source),"Loading dispatch retained count differs");
    std::set<uintptr_t> unique;
    for(int32_t i=0;i<count;++i){const auto task=read<uintptr_t>(source+size_t(i)*8);
        require(plan(task)&&unique.insert(task).second,"Loading dispatch retained queue contains other/duplicate tasks");}
    return true;
}
void cancel(){require(!uncertain,"Loading dispatch cannot cancel uncertain queue mutation");ordered_run::cancel();}
Json status(){return {{"enabled",enabled},{"step_drain",step_drain},{"ordered_run",ordered_run::status()},{"path_child_attempted",path_child_attempted},{"path_child_returned",path_child_returned},{"path_children_observed",path_children_observed},{"ordered_finish",ordered_finish::status()},{"pending",pending},{"partitions",partitions},{"deferred_visits",deferred_visits},
    {"first_defer_frame",first_defer_frame},{"first_release_frame",first_release_frame},{"release_admitted",release_admitted},
    {"finish_entered",finish_entered},{"finish_returned",finish_returned},{"uncertain",uncertain},{"first_error",first_error}};}
void install(const Json& config){
    enabled=config.value("defer_loading_plans",false);ordered_finish::install(config);if(!enabled)return;
    require(config.value("task_barrier",false)&&config.value("bootstrap_boundary",false),"Loading dispatch requires task and bootstrap boundaries");
    for(const auto& [offset,size,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,22>{{
        {0x52bca0,48,"c360b80e08ab9d4bcb128bfd19a4b49e0231dd50874513ddd1918bc968b69033"},
        {0x88a7a0,64,"c00f6bfb163738db32a6d1ecbb36d5597e4ec43c5a0f346a933f89f15aeb9d4c"},
        {0x88a8f0,672,"fe0a9bb2690227fdebc3c90607ca3e61125db70074c82104efd5f7a0c01054cc"},
        {0xc43050,16,"499f1f307c1cb989f968a6b7fcaec591e1828877223d0b0e7e8e8b76cde8c9ca"},
        {0x52bcd0,384,"24d300bdcd38782ede1c7d2abbe0f312e719e5170df2c4e413b168be08eac0e4"},
        {0x52d350,656,"7032628a84fa3aadf1ea0db5dfd2a61b24598742ffa65260d104b7d8ee324dd5"},
        {0x52f6c0,80,"9f9e9c3e8fda22042d4cc140f3045a409ab7eb5ec7ba83cd8008f28da6fd2241"},
        {0x52d5e0,688,"fe60322d41bc348ab845c447946183b4632f8b731d755122f51404afe3ab76cc"},
        {0x306e50,2960,"1ffdfb4f9a2b1583318ce4915e3176897b7309b6a73cf3cbe3448def04b21398"},
        {0xa15d50,32,"880aca15d3c48a1445edd270fd16c3059182f388373065b47134512bcb5945c3"},
        {0xa15d70,16,"0cda1e1a74db7014ee00491e24368aacfdcdc4b7ecd7f3a3ebf09a38f0994589"},
        {0xa15d80,48,"5e5d1112c8eed8b7390cece342b2c264ec6e1629f9edbe72959f1276ee971275"},
        {0x52f190,1264,"6de4bba2d451254c1734c52df8119f69ceedb7321d0cb3f003bf637585822317"},
        {0x305a80,2784,"e75f5bccc0b1897a2f000539a81818632e0c32113ad7a5ffd237e86eabf73a39"},
        {0x2c8760,352,"00ad1db9327c9096d66275838d4bf11ebf9848ad6164240b5c67711c4ba043df"},
        {0x2c8480,16,"57e801b77079454e0fda719f80df81c8a8ecfbfeb5ee311fd1b43029f3151c88"},
        {0xa15c70,16,"1b6c7cf1f5a5e33a415cf4c8ee1f30220a18186f45f1e55644059940bf551249"},
        {0x52cf20,432,"db5e9cc494cc1f60c38e250084967df1e2bebe2f84a2bb18f1fd3a02430aa176"},
        {0xa15c80,16,"c0f0bce8becfaa6e989b6d4f26882e89fef666eca6ad5b141461597af0a02cf4"},
        {0x52ed90,704,"0f35b050a174025f709834afba78f53ef14ccf40575b2a1a3a9b16a27eadc8ed"},
        {0x52f050,320,"6460f08cadac4262089b2ef0e0295dab33bcaa055c680ff8159aacba6ca1b91b"},
        {0x304e90,48,"a0703198546cfb6c7851aca8acd296958e16bdddd062da7ea1b4ecabbb08c4cd"}}}){
        std::string body(size,'\0');SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+offset),body.data(),size,&copied)||copied!=size||digest(body)!=sha)
            throw std::runtime_error("Loading dispatch exact native body differs");
    }
    require(read<uintptr_t>(image+Plan+0x10)==image+0x305a80,"Ordered Run exact ToonPlan Run slot differs");
    native_routes->catalog("framework.save.writer-complete",0x52bca0,true);
    native_services->reserve("framework.save.writer-complete");
    hook(reinterpret_cast<void*>(image+0x52bca0),reinterpret_cast<void*>(&save_completion::entry),reinterpret_cast<void**>(&save_completion::original));
    ordered_run::post=reinterpret_cast<Unlock>(image+0xa15d70);
    path_lock=reinterpret_cast<Unlock>(image+0xa15c70);
    require(read<uintptr_t>(image+0x171ae60+0x18)==image+0x2c8480,"Native Pathfinder Finish slot differs");
    require(read<uintptr_t>(image+Plan+0x18)==image+0x304e90,"Loading dispatch exact ToonPlan Finish slot differs");
    hook(reinterpret_cast<void*>(image+0xa15c70),reinterpret_cast<void*>(&ordered_run::lock),reinterpret_cast<void**>(&ordered_run::original_lock));
    hook(reinterpret_cast<void*>(image+0xa15c80),reinterpret_cast<void*>(&unlock),reinterpret_cast<void**>(&original_unlock));
    hook(reinterpret_cast<void*>(image+0x304e90),reinterpret_cast<void*>(&finish),reinterpret_cast<void**>(&original_finish));
}
}
