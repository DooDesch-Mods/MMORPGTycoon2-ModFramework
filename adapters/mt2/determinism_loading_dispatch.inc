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
constexpr uintptr_t Plan=0x17037f0,PumpUnlockReturn=0x526be2,PumpFinishReturn=0x526c55;
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
        scope->certificate.prepare_attempted==1&&scope->game==owner&&scope->tasks==read<uintptr_t>(image+0x13df340)&&
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
    const auto tasks=read<uintptr_t>(image+0x13df340);
    require(tasks&&read<uintptr_t>(owner+0x68)==tasks&&read<uintptr_t>(tasks)==image+0x16fdaf0,"Loading dispatch Task owner/address point changed");
    require(!held_tasks&&read<uint8_t>(tasks+0xf8)==0,"Loading dispatch workers are held");
    require(!uncertain,"Loading dispatch previous queue mutation is uncertain");
    return tasks;
}
bool plan(uintptr_t task){
    require(task!=0,"Loading dispatch null task");
    if(read<uintptr_t>(task)!=image+Plan)return false;
    require(read<int32_t>(task+8)==2&&read<uint8_t>(task+0xe)==0,"Loading dispatch ToonPlan type/completion differs");
    require(read<uintptr_t>(image+Plan+0x18)==image+0x301540,"Loading dispatch ToonPlan Finish slot changed");
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
            require((slot==2||slot==5||slot==6)&&ap==image+0x1714ac0&&type==2&&read<uint8_t>(task+0xe)==0&&
                read<uintptr_t>(ap+0x18)==image+0x2c4e70,
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
        (plan(task)?held:kept).push_back(task);
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
        const auto tasks=read<uintptr_t>(image+0x13df340);
        require(tasks&&tasks==held_tasks&&read<uintptr_t>(owner+0x68)==tasks&&read<uintptr_t>(tasks)==image+0x16fdaf0&&
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
        require((control.phase==mt2::determinism::Gate::Phase::Stepping&&step_drain)||recovery_draining(),"ToonPlan Finish lacks complete-step or exact Recovery drain admission");
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
    for(const auto& [offset,size,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,21>{{
        {0x5258b0,48,"1c49f7b3d7e3ee4431d6f520a961ad718b724641fe4854d512890a90cc5935d9"},
        {0x8819e0,64,"fa3d71ce6e9e7f4de3aa5f0e35cf27540a6bda368fcc202e6b9057e7c6abe7d2"},
        {0x881b30,672,"f0dea32c427373cf7bfc89c1d1dea5183fddd2f452b03d038427559489202303"},
        {0xc367a0,16,"499f1f307c1cb989f968a6b7fcaec591e1828877223d0b0e7e8e8b76cde8c9ca"},
        {0x5258e0,384,"34f36fc926028624a1fb7e1a5ac55effdad48942080b88d408a8a67a3d2c0ae3"},
        {0x526f30,656,"398f2860081c8e99bee70e184aebae5eed5934776697197f7fad5a7d017d9191"},
        {0x529240,80,"88d91bffc505162b97c21c5195590f24730fcae9b9d9f1b3631ec968dd94ee58"},
        {0x5271c0,688,"1130ccf782f91dd0ffcc784111c3909076aa704da106a103b23167c00879c50b"},
        {0xa0b190,32,"4a8e56e13d3367d83f43f02ba400f0650c0d18efdb30f7fde2443d1077fbc0da"},
        {0xa0b1b0,16,"7d304000253004e308040d0ce0e08923cca0c770f023accc196425ce1488ba26"},
        {0xa0b1c0,48,"8bac6f9fd573e2a125dc12da69cdf43bedbab7d789c9db0da6cbe35e51a86633"},
        {0x528d10,1264,"eeb7c04444721ce57671f50444c8dff7ad54aed2ea91677b0923c64825d02e9a"},
        {0x302130,2784,"570667edb34e6cfb490fb2c68ac7d7bc717931bda199f27677af9a54679cb957"},
        {0x2c5150,352,"4ec676eed9155be0d7e8dbff13a88a1ad1574c5444f70e9bad286f105a52085a"},
        {0x2c4e70,16,"57e801b77079454e0fda719f80df81c8a8ecfbfeb5ee311fd1b43029f3151c88"},
        {0xa0b0b0,16,"74058e2f7751de28d7fc699eb506c1bd6a4e617385e3c5575abcc495b8a541ef"},
        {0x526b00,432,"12ca52a501a074d6459b697bf9df2d8c4a7322bed76270b0cfe9dc33b91bd016"},
        {0xa0b0c0,16,"bb65d9ed8a4b4118a4fe43ddefc29df52a3b6176c7214051853c04163170834d"},
        {0x528910,704,"37f926e93e7d92ce59925e9e495c1706ffd3261ffc5af95f6619fc742977fc8f"},
        {0x528bd0,320,"6d81683f78e5b0d0a294bd4e644fc5caf1ca185e09727ac313c0d3feea4a21c0"},
        {0x301540,48,"184187f03856909527c5ed1b829c6cb52d4ea7322ba6164447b41dbbacd6a167"}}}){
        std::string body(size,'\0');SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+offset),body.data(),size,&copied)||copied!=size||digest(body)!=sha)
            throw std::runtime_error("Loading dispatch exact native body differs");
    }
    require(read<uintptr_t>(image+Plan+0x10)==image+0x302130,"Ordered Run exact ToonPlan Run slot differs");
    native_routes->catalog("framework.save.writer-complete",0x5258b0,true);
    native_services->reserve("framework.save.writer-complete");
    hook(reinterpret_cast<void*>(image+0x5258b0),reinterpret_cast<void*>(&save_completion::entry),reinterpret_cast<void**>(&save_completion::original));
    ordered_run::post=reinterpret_cast<Unlock>(image+0xa0b1b0);
    path_lock=reinterpret_cast<Unlock>(image+0xa0b0b0);
    require(read<uintptr_t>(image+0x1714ac0+0x18)==image+0x2c4e70,"Native Pathfinder Finish slot differs");
    require(read<uintptr_t>(image+Plan+0x18)==image+0x301540,"Loading dispatch exact ToonPlan Finish slot differs");
    hook(reinterpret_cast<void*>(image+0xa0b0b0),reinterpret_cast<void*>(&ordered_run::lock),reinterpret_cast<void**>(&ordered_run::original_lock));
    hook(reinterpret_cast<void*>(image+0xa0b0c0),reinterpret_cast<void*>(&unlock),reinterpret_cast<void**>(&original_unlock));
    hook(reinterpret_cast<void*>(image+0x301540),reinterpret_cast<void*>(&finish),reinterpret_cast<void**>(&original_finish));
}
}
