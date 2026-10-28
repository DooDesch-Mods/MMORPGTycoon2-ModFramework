// Included inside the private lab namespace after timer_step.
namespace task_boundary {
using Finish=void(*)(void*);
using Done=bool(*)(void*);
Finish finish_all=nullptr;
Done all_done=nullptr;
bool enabled=false;
uint64_t checked=0,skipped=0,attempted=0,returned=0;
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
uint64_t loading_skipped=0,ready_frame=0;
int32_t last_mode_state=-1,last_map_state=-1;
bool load_ready(){
    native::WorldStatus world;std::string cause;
    if(!native::read_world_status(world,cause))throw std::runtime_error("Task boundary load status read failed: "+cause);
    last_mode_state=world.in_game_state;last_map_state=-1;
    if(world.active_mode_index!=3||world.in_game_state<2){
        require(control.phase==mt2::determinism::Gate::Phase::Loading,"Task boundary stepped world lost its loaded Mode");return false;
    }
    require(world.in_game_state<=4,"Task boundary unexpected native load state");
    const auto mode=read<uintptr_t>(owner+0x28);
    require(mode&&mode==read<uintptr_t>(image+0x13df320)&&read<uintptr_t>(mode)==image+0x16e6730,"Task boundary Mode singleton/address point changed");
    const auto state=read<uintptr_t>(mode+0x58);
    require(state,"Task boundary GameState is absent after native init");
    const auto world_owner=read<uintptr_t>(state+0x160);
    require(world_owner&&world_owner==read<uintptr_t>(image+0x13df9c0)&&read<uintptr_t>(world_owner)==image+0x174aef0,"Task boundary World singleton/address point changed");
    const auto map=read<uintptr_t>(world_owner+0x158);
    require(map&&map==read<uintptr_t>(image+0x13df8c0)&&read<uintptr_t>(map)==image+0x1742860,"Task boundary Map singleton/address point changed");
    last_map_state=read<int32_t>(map+0x3d4);
    const auto ready=read<uint8_t>(map+0x47f);
    require(ready<=1,"Task boundary Map load-ready flag is invalid");
    if(!ready){require(control.phase==mt2::determinism::Gate::Phase::Loading,"Task boundary stepped world lost Map load readiness");return false;}
    // Original Mode.Update tests this flag before enabling its first state4
    // simulation. Earlier map work can synchronously await Main-thread draws.
    ready_frame=timer_step::frame.entered;return true;
}
void require_loaded(){
    if(enabled&&(!ready_frame||ready_frame!=timer_step::frame.entered))
        throw std::runtime_error("Task boundary first state4 frame lacked a prior load-ready boundary before Mode simulation");
}
std::string first_error;
uintptr_t receiver(void* game){
    require(thread==GetCurrentThreadId(),"Task boundary game thread changed");
    require(control.entered&&timer_step::frame.active&&
        (control.phase==mt2::determinism::Gate::Phase::Loading||control.phase==mt2::determinism::Gate::Phase::Stepping),
        "Task boundary lacks an admitted loading or stepping outer frame");
    require(owner==reinterpret_cast<uintptr_t>(game)&&timer_step::frame.game==owner,"Task boundary Game receiver changed");
    timer_step::current_game();
    const auto tasks=read<uintptr_t>(image+0x13df340);
    require(tasks&&read<uintptr_t>(owner+0x68)==tasks&&read<uintptr_t>(tasks)==image+0x16fdaf0,"Task boundary Task singleton/address point changed");
    require(!held_tasks&&read<uint8_t>(tasks+0xf8)==0,"Task boundary requires released workers");
    require(read<int32_t>(tasks+0xc0)>=0,"Task boundary finished count is negative");
    return tasks;
}
void after_update(void* game){
    if(!enabled||!control.active())return;
    try{
        const auto tasks=receiver(game);++checked;
        if(!load_ready()){++loading_skipped;return;}
        if(all_done(reinterpret_cast<void*>(tasks))&&read<int32_t>(tasks+0xc0)==0){++skipped;return;}
        ++attempted;
        if(attempted==1)runtime->log("determinism","info","Task boundary entering original FinishAllTasks after GameUpdate; native wait has no timeout or GL pump; no replay if it does not return");
        // The original owns the wait, GL pump and completion-list lifetime.
        // Do not replace this with direct Run/Finish calls or repeat it if a
        // completion callback creates new work.
        loading_dispatch::StepDrain drain;
        loading_dispatch::finish_complete_step(reinterpret_cast<void*>(tasks),finish_all);++returned;
        require(receiver(game)==tasks,"Task boundary owner changed during FinishAllTasks");
        require(all_done(reinterpret_cast<void*>(tasks)),"Task boundary FinishAllTasks returned with pending/running work; no repeated drain");
        require(read<int32_t>(tasks+0xc0)==0||loading_dispatch::retained_only(tasks),"Task boundary FinishAllTasks returned with finished callbacks; no repeated drain");
        if(returned==1)runtime->log("determinism","info","Task boundary first original FinishAllTasks returned with no pending/running work and only explicitly deferred loading callbacks");
    }catch(const std::exception& problem){if(first_error.empty())first_error=problem.what();throw;}
    catch(...){if(first_error.empty())first_error="Task boundary original FinishAllTasks raised an unknown exception; no replay";throw;}
}
Json status(){return {{"enabled",enabled},{"loading_skipped",loading_skipped},{"ready_frame",ready_frame},{"last_mode_state",last_mode_state},{"last_map_state",last_map_state},{"checked",checked},{"skipped",skipped},{"attempted",attempted},{"returned",returned},{"first_error",first_error}};}
void install(const Json& config){
    enabled=config.value("task_barrier",false);if(!enabled)return;
    for(const auto& [offset,size,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,2>{{
        {0x526d50,480,"666f4cac19d8d04df16bdff53afbcc87899ad708f157aeae5674cf284a4540fa"},
        {0x525ae0,112,"ffb5c39d83679b65d6d17eed0f5e6031cd7cd967efd3bff784713138a5940046"}}}){
        std::string body(size,'\0');SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+offset),body.data(),size,&copied)||copied!=size||digest(body)!=sha)
            throw std::runtime_error("Exact original Task FinishAllTasks/AllTasksAreDone body differs");
    }
    finish_all=reinterpret_cast<Finish>(image+0x526d50);all_done=reinterpret_cast<Done>(image+0x525ae0);
}
}
