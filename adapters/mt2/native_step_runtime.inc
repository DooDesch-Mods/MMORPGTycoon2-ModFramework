// Common native driver. The private diagnostic Gate is not armed or queried.
namespace step_runtime {
namespace native=mt2mp::game::native;
using Go=void(*)(void*);
using Seed=void(*)(void*,uint64_t);
Go original_go=nullptr,original_pause=nullptr,original_resume=nullptr;
Seed original_seed=nullptr;
uintptr_t image=0,owner=0,held_tasks=0;
DWORD thread=0;
bool installed=false,configured=false,configuration_attempted=false,resume_tasks=false,outer_entered=false;
fs::path executable;
// Retain the concrete driver until process exit, including failed native holds.
mt2::NativeStepDriver* driver=nullptr;
MT2FW_MT2_StepReceipt lifecycle{sizeof(lifecycle)};
mt2::StepNativeObservation bootstrap;
std::array<uint64_t,4> seeded_words{};
std::string failure;
struct Control {
    using Phase=mt2::determinism::Gate::Phase;
    Phase phase=Phase::Passive;
    bool entered=false;
    uint64_t completed=0;
    bool active()const{return phase!=Phase::Passive;}
} control;
void require(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
template<class T>T read(uintptr_t address){
    T value{};SIZE_T copied=0;
    require(address&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&copied)&&copied==sizeof(value),
        "Step lifecycle native read failed address="+std::to_string(address));return value;
}
template<class T>void write(uintptr_t address,const T& value){
    SIZE_T copied=0;
    require(WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof(value),&copied)&&copied==sizeof(value)&&read<T>(address)==value,
        "Step lifecycle native write/readback failed");
}
uint64_t rva(uintptr_t value){require(value>=image&&value-image<0x2000000,"Step native address lies outside pinned image");return value-image;}
bool building_widget_owner(const void*,std::string&);
// These helpers install accepted caller-filtered presentation routing. Tracing
// is never started by the production lifecycle.
#include "determinism_rng_trace.inc"
#include "determinism_presentation_rng.inc"
#include "determinism_timer_step.inc"
namespace motion_probe {
// Loading dispatch observation is a private diagnostic, not a production hook.
unsigned finish_before(uintptr_t){return 0;}
void finish_after(unsigned,uintptr_t){}
}
namespace async_save {
bool before_unlock(void*);bool writer_complete(void*,uintptr_t,Go);
bool resume();bool pump();bool pending();bool started();void retire();
void retain_scope(const mt2::PreparedSaveScope&);
}
#define MT2FW_ORDERED_ASYNC_SAVE
#define MT2FW_ORDERED_RECOVERY
#include "determinism_loading_dispatch.inc"
#undef MT2FW_ORDERED_ASYNC_SAVE
#undef MT2FW_ORDERED_RECOVERY
#include "determinism_task_boundary.inc"
#include "determinism_bootstrap_boundary.inc"
#include "native_region_rng.inc"

uint32_t building_widget_lookup(uintptr_t entry,uint64_t uid,uintptr_t& object)noexcept{
    __try{object=reinterpret_cast<uintptr_t(*)(uint64_t)>(entry)(uid);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool building_widget_owner(const void* building,std::string& error)try{
    error.clear();
    if(!thread||GetCurrentThreadId()!=thread||!configured||!driver||!driver->frozen()||!lifecycle.bootstrap_held)return false;
    const auto receipt=native_steps->snapshot();
    if(receipt.phase!=MT2FW_MT2_STEP_HELD&&receipt.phase!=MT2FW_MT2_STEP_ENTERED)return false;
    require(receipt.lease&&receipt.session&&receipt.thread==thread&&receipt.session==bootstrap.identity.session&&
        receipt.world_epoch==bootstrap.world_epoch&&receipt.generation==bootstrap.identity.generation&&
        lifecycle.generation==bootstrap.identity.generation&&owner==bootstrap.game,
        "Building widget ordered lease/session/world identity differs");
    require(owner&&read<uintptr_t>(image+0x13df8f0)==owner&&read<uintptr_t>(owner+0x28)==bootstrap.mode&&
        read<uintptr_t>(bootstrap.mode+0x58)==bootstrap.state&&read<uintptr_t>(bootstrap.state+0x160)==bootstrap.world&&
        read<uintptr_t>(bootstrap.world+0x158)==bootstrap.map,"Building widget ordered current World chain differs");
    const auto pointer=reinterpret_cast<uintptr_t>(building);if(!pointer||read<uintptr_t>(pointer)!=image+0x16d93c0)return false;
    const auto manager=read<uintptr_t>(image+0x13df440);require(manager,"Building widget current Shard manager absent");
    const auto root=read<uintptr_t>(manager+0x60);require(root,"Building widget current root Shard absent");
    const auto uid=read<uint64_t>(pointer+0x18);if(!uid||read<uintptr_t>(pointer+0x170)!=root)return false;
    uintptr_t found=0;const auto exception=building_widget_lookup(image+0x47ce20,uid,found);
    require(!exception,"Building widget native UID lookup failed SEH="+std::to_string(exception));
    if(found!=pointer)return false;
    require(read<uint64_t>(found+0x18)==uid,"Building widget native full UID changed");
    return true;
}catch(const std::exception& problem){error=problem.what();return false;}
catch(...){error="Building widget owner proof raised unknown exception";return false;}

uintptr_t tasks(){
    require(thread==GetCurrentThreadId()&&owner&&read<uintptr_t>(image+0x13df8f0)==owner&&read<uintptr_t>(owner)==image+0x1743740,
        "Step lifecycle Game/thread changed");
    const auto value=read<uintptr_t>(image+0x13df340);
    require(value&&read<uintptr_t>(owner+0x68)==value&&read<uintptr_t>(value)==image+0x16fdaf0,"Step lifecycle Task owner changed");return value;
}
void held(){
    require(held_tasks&&tasks()==held_tasks&&read<uint8_t>(held_tasks+0xf8)==1&&read<int32_t>(held_tasks+0xa8)==0&&read<int32_t>(held_tasks+0x90)==0,
        "Step lifecycle scheduler is not paused/drained");
}
namespace client_periodic_ui {
void arm(const MT2FW_MT2_SnapshotRequest&);void seal(const MT2FW_MT2_SnapshotRequest&);
void released(const MT2FW_MT2_SnapshotRequest&);void reset();
}
#include "native_snapshot_prepare.inc"
#include "native_step_clock_pause.inc"
#include "native_async_save.inc"
void begin_prepared_async(void* mode,const void* filename,bool absolute){async_save::begin(mode,filename,absolute);}
void returned_prepared_async(){async_save::entry_returned();}
void validate_save_mode(void* receiver){require(snapshot_prepare::mode()==reinterpret_cast<uintptr_t>(receiver),"Native Save request Mode is not the current bound world");}
#include "native_client_periodic_ui.inc"
void delegate_client_periodic(void* receiver,uintptr_t caller){
    const auto receipt=native_steps->snapshot();
    require(receipt.actual_role==MT2FW_MT2_SESSION_CLIENT&&receipt.thread==GetCurrentThreadId()&&
        receipt.lease&&receipt.session&&receipt.world_epoch&&
        (receipt.phase==MT2FW_MT2_STEP_HELD||receipt.phase==MT2FW_MT2_STEP_ENTERED),
        "Client periodic completion lacks its accepted native execution owner");
    validate_save_mode(receiver);tasks();
    client_periodic_ui::retain(receiver,caller);
    // Pinned PeriodicAutosave resets this timer after its asynchronous request.
    // The shared Host owns rotation and serialization; neither runs locally.
    write<uint32_t>(reinterpret_cast<uintptr_t>(receiver)+0xc8,0);
    validate_save_mode(receiver);tasks();
    runtime->log("steps","info","Client periodic save delegated to Host tick="+std::to_string(receipt.tick));
}

#include "native_step_command_hold.inc"
void pause_workers(){
    const auto value=tasks();
    if(!held_tasks){resume_tasks=read<uint8_t>(value+0xf8)==0;held_tasks=value;}
    require(value==held_tasks,"Step lifecycle held Task owner changed");
    original_pause(reinterpret_cast<void*>(value));held();lifecycle.workers_held=1;
}
void release_workers(){
    held();require(resume_tasks,"Step lifecycle did not observe running workers before Pause");
    original_resume(reinterpret_cast<void*>(held_tasks));
    require(read<uint8_t>(held_tasks+0xf8)==0,"Step lifecycle Resume was not acknowledged");
    held_tasks=0;lifecycle.workers_held=0;
}
uintptr_t rng(){const auto value=read<uintptr_t>(image+0x164b250);require(value==image+0x1c4e7a0,"Step Default RNG receiver changed");return value;}
void publish(){if(driver&&driver->frozen())native_steps->lifecycle(lifecycle);}
void retain_failure(){
    // Preserve the observed hold ACK even when the original operation failed.
    try{if(lifecycle.generation)publish();}
    catch(const std::exception& error){failure+="; failure receipt publication failed: "+std::string(error.what());}
    catch(...){failure+="; failure receipt publication raised unknown native exception";}
    native_steps->failed(failure);runtime->log("steps","error",failure);outer_entered=false;
}
void seed(const mt2::StepNativeObservation& selected){
    require(selected.phase==mt2::StepNativeObservation::Phase::Preload&&!lifecycle.seed_attempted,"Step seed requires one unattempted verified preload");
    native::WorldStatus world;std::string cause;
    require(native::read_world_status(world,cause)&&world.active_mode_index==2,"Step seed requires actual title mode: "+cause);
    lifecycle.generation=selected.identity.generation;
    pause_workers();presentation_rng::arm();loading_dispatch::ordered_run::arm(held_tasks);
    region_rng_load::arm(selected.identity,driver->region_rng_rows());
    control.phase=Control::Phase::Preload;
    lifecycle.seed_attempted=1;publish();
    original_seed(reinterpret_cast<void*>(rng()),*selected.identity.agreed_seed);
    seeded_words=read<std::array<uint64_t,4>>(rng());
    require(seeded_words==mt2::determinism::Gate::seed_words(*selected.identity.agreed_seed)&&read<int32_t>(rng()+0x20)==0,
        "Step original InitWithSeed readback differs; no replay");
    lifecycle.seed_returned=1;publish();
}
void capture_bootstrap(){
    bootstrap_boundary::require_loaded();task_boundary::require_loaded();
    pause_workers();timer_step::restore_held();presentation_rng::check();
    require(resume_tasks,"Step bootstrap did not capture a running scheduler");
    bootstrap.game=owner;bootstrap.mode=read<uintptr_t>(owner+0x28);
    bootstrap.state=read<uintptr_t>(bootstrap.mode+0x58);bootstrap.world=read<uintptr_t>(bootstrap.state+0x160);
    bootstrap.map=read<uintptr_t>(bootstrap.world+0x158);bootstrap.resume_workers=resume_tasks;
    control.phase=Control::Phase::Held;
    // Mode completed after GameUpdate's normal observation. Publish the real
    // world lifecycle now, without running actions, update callbacks or a Go.
    mt2::observe_world(*runtime);
}
bool combine(mt2::StepNativeObservation& observed){
    if(!lifecycle.bootstrap_held)return false;
    require(observed.phase==mt2::StepNativeObservation::Phase::Loaded&&observed.identity==bootstrap.identity&&observed.world_epoch==bootstrap.world_epoch&&
        observed.game==bootstrap.game&&observed.mode==bootstrap.mode&&observed.state==bootstrap.state&&observed.world==bootstrap.world&&observed.map==bootstrap.map,
        "Step sealed NativeOpen identity differs from independently held bootstrap");
    observed.resume_workers=bootstrap.resume_workers;observed.witness=bootstrap.witness;return true;
}
bool bootstrap_export_attempted=false;
void export_bootstrap_certificate(const Json& canonical,const std::string& hash,const mt2::StepNativeObservation& observed){
    bool private_profile=false;
    for(const auto& part:root){auto name=part.string();std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});private_profile|=name=="_mptest";}
    if(!private_profile||bootstrap_export_attempted)return;
    bootstrap_export_attempted=true;
    require(canonical.dump().size()<=1024*1024,"Private bootstrap certificate exceeds bounded diagnostic export");
    const auto path=root/"diagnostics"/("step-bootstrap-certificate-"+std::to_string(GetCurrentProcessId())+".json");
    require(!fs::exists(path),"Private bootstrap certificate path already exists; no overwrite");
    const Json artifact={{"schema_version",1},{"canonical_sha256",hash},{"canonical",canonical},
        {"local_evidence",{{"process_id",GetCurrentProcessId()},{"generation",observed.identity.generation},
            {"world_epoch",observed.world_epoch},{"completed_steps",control.completed},{"bootstrap_deferred",bootstrap_boundary::deferred},
            {"bootstrap_resumed",bootstrap_boundary::resumed},{"before_first_mode_update",true}}}};
    atomic_json(path,artifact);
    require(read_json(path)==artifact,"Private bootstrap certificate persisted readback differs");
}
void certify(const mt2::StepNativeObservation& observed){
    require(bootstrap.game&&!lifecycle.bootstrap_held&&observed.phase==mt2::StepNativeObservation::Phase::Loaded&&observed.world_epoch,
        "Step certificate lacks completed native bootstrap and sealed load");
    held();presentation_rng::check();
    require(observed.game==bootstrap.game&&observed.mode==bootstrap.mode&&observed.state==bootstrap.state&&observed.world==bootstrap.world&&observed.map==bootstrap.map,
        "Step NativeOpen world differs from native bootstrap pointers");
    auto phase_guard=[&](){
        require(bootstrap.game&&!lifecycle.bootstrap_held&&observed.phase==mt2::StepNativeObservation::Phase::Loaded&&observed.world_epoch&&
            observed.game==bootstrap.game&&observed.game==owner&&observed.mode==bootstrap.mode&&observed.state==bootstrap.state&&observed.world==bootstrap.world&&observed.map==bootstrap.map&&
            control.phase==Control::Phase::Held&&!control.completed&&!control.entered&&!timer_step::frame.active,
            "TickClock initialization lacks the fresh held zero-step world");
        held();bootstrap_boundary::require_loaded();
    };
    const auto phase=mt2::tick_clock_phase::initialize(image,owner,
        [](uintptr_t p){return read<uint64_t>(p);},[](uintptr_t p){return read<uint32_t>(p);},[](uintptr_t p){return read<uint8_t>(p);},
        [](uintptr_t p,uint32_t value){write<uint32_t>(p,value);},phase_guard);
    Json clock_phase=Json::array(),before_phase=Json::array();
    for(size_t i=0;i<phase.after.size();++i){const auto& a=phase.before[i];const auto& b=phase.after[i];
        before_phase.push_back({{"budget_bits",a.budget},{"next_index",a.cursor}});
        clock_phase.push_back({{"count",b.count},{"period_bits",b.period},{"rate_bits",b.rate},{"budget_bits",b.budget},{"next_index",b.cursor}});}
    runtime->log("steps","info","Fresh-world TickClock phase initialized before="+before_phase.dump()+" after="+clock_phase.dump());
    native::SimulationState sim;std::string cause;
    require(native::read_simulation_state(sim,cause),"Step bootstrap simulation read failed: "+cause);
    const auto random=read<std::array<uint64_t,4>>(rng());const auto extra=read<int32_t>(rng()+0x20);
    mt2::determinism::WorkWitness work([](uintptr_t at,size_t count){
        mt2::determinism::WorkWitness::Bytes value(count);SIZE_T copied=0;
        require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),value.data(),count,&copied)&&copied==count,"Step canonical world read failed");return value;
    },image,owner);
    require(control.phase==Control::Phase::Held&&!control.completed&&!control.entered&&!timer_step::frame.active,
        "Initial Developer certificate is outside the held zero-step boundary");
    bootstrap_boundary::require_loaded();
    auto developers=work.developers(true).at("comparable");
    auto& developer_rows=developers.at("developers");
    std::sort(developer_rows.begin(),developer_rows.end(),[](const Json& a,const Json& b){return a.at("id").get<int32_t>()<b.at("id").get<int32_t>();});
    auto subscribers=work.subscribers().at("comparable");
    auto& rows=subscribers.at("rows");std::sort(rows.begin(),rows.end(),[](const Json& a,const Json& b){return a.at("saved_id").get<int32_t>()<b.at("saved_id").get<int32_t>();});
    std::vector<int32_t> pending;
    const auto count=read<int32_t>(held_tasks+0xc0);const auto data=read<uintptr_t>(held_tasks+0xb8);
    require(count==loading_dispatch::pending&&count>=0&&(!count||data),"Step certificate retained plan count differs");
    for(int32_t i=0;i<count;++i){const auto task=read<uintptr_t>(data+size_t(i)*8);require(loading_dispatch::plan(task),"Step certificate retained non-plan callback");
        const auto subscriber=read<uintptr_t>(task+0x20);require(subscriber&&read<uintptr_t>(subscriber)==image+0x16e75f0,"Step certificate plan owner differs");
        pending.push_back(read<int32_t>(subscriber+0x20));}
    std::sort(pending.begin(),pending.end());
    // This is an explicit observed-state certificate, not a full-world hash.
    // No pointers, frame counts, clock measurements or local generation enter it.
    const auto region_random=region_rng_load::seal();
    const Json canonical={{"schema","mt2-step-bootstrap-v5-tick-clock-phase"},{"region_rng",region_random},{"tick_clock_manager_paused",false},{"tick_clock_phase",clock_phase},{"bootstrap_coverage",MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS},{"snapshot_sha256",observed.identity.snapshot_sha256},
        {"snapshot_bytes",observed.identity.snapshot_bytes},{"seed",*observed.identity.agreed_seed},{"seeded_words",seeded_words},
        {"default_rng",random},{"default_rng_extra",extra},{"clock",work.clock().at("comparable")},
        {"developers",developers},{"cash",sim.cash},{"logged_in_subscribers",sim.logged_in_subscribers},{"subscribers",subscribers},{"pending_plan_owners",pending}};
    work.finish();held();require(read<std::array<uint64_t,4>>(rng())==random&&read<int32_t>(rng()+0x20)==extra,"Step bootstrap RNG drifted during certificate read");
    const auto hash=digest(canonical.dump());require(hash.size()==64,"Step bootstrap SHA256 encoding differs");
    export_bootstrap_certificate(canonical,hash,observed);
    for(size_t i=0;i<32;++i)bootstrap.witness[i]=uint8_t(std::stoul(hash.substr(i*2,2),nullptr,16));
    bootstrap.identity=observed.identity;bootstrap.world_epoch=observed.world_epoch;bootstrap.phase=mt2::StepNativeObservation::Phase::Loaded;
    lifecycle.world_epoch=observed.world_epoch;lifecycle.bootstrap_held=1;lifecycle.bootstrap_coverage=MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS;
    std::copy(bootstrap.witness.begin(),bootstrap.witness.end(),lifecycle.initial_witness_sha256);publish();
}
void before_step(const MT2FW_MT2_StepBoundary&){
    snapshot_prepare::before_step();
    held();control.phase=Control::Phase::Stepping;control.entered=true;
    timer_step::frame.game=owner;timer_step::frame.active=true;++timer_step::frame.entered;
    bootstrap_boundary::before_step();held_tasks=0;lifecycle.workers_held=0;
}
void after_update(void* game){
    if(!driver||!driver->frozen())return;
    if(control.phase==Control::Phase::Loading)task_boundary::after_update(game);
    else {loading_dispatch::StepDrain drain;driver->after_update(game);}
}
__declspec(noinline) void timer(void* receiver,float incoming){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(driver&&control.phase==Control::Phase::Stepping)driver->timer_update(receiver,incoming,caller);
    else timer_step::update_call(receiver,incoming,caller);
}

// Retained pixels keep presentation independent from authoritative Go. SDL
// pumping leaves events queued for the existing native ownership input route.
mt2::determinism::WaitRender pixels;
uint64_t next_present=0;
struct SwapCounters {
    uint64_t calls=0,returned=0,succeeded=0,failed=0,total_ns=0,max_ns=0;
    void finish(bool result,uint64_t elapsed){
        ++returned;if(result)++succeeded;else ++failed;
        total_ns+=elapsed;max_ns=std::max(max_ns,elapsed);
    }
    Json status()const{return {{"calls",calls},{"returned",returned},{"succeeded",succeeded},{"failed",failed},{"total_ns",total_ns},{"max_ns",max_ns}};}
};
SwapCounters native_swaps,retained_swaps;
uint64_t retained_pacing_skips=0;
#include "native_step_presentation_metrics.inc"
Json presentation_status(){
    return {{"scope","process-lifetime ordered native and retained SDL swaps; terminal excluded"},
        {"native",native_swaps.status()},{"retained",retained_swaps.status()},
        {"cadence",swap_cadence.status(SwapCadence::now_ns())},
        {"retained_pacing_skips",retained_pacing_skips},{"command_hold",command_hold::status()},{"client_periodic_ui",client_periodic_ui::status()}};
}
bool native_swap(SDL_Window* target){
    if(!installed||!control.entered||target!=window||thread!=GetCurrentThreadId())return original_swap(target);
    ++native_swaps.calls;const auto began=std::chrono::steady_clock::now();
    const bool result=original_swap(target);
    native_swaps.finish(result,uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-began).count()));
    observe_successful_swap(result,false);
    // The native Go already presented. Pace from its successful return so a
    // blocking swap cannot make the following retained repaint immediately due.
    if(result)next_present=GetTickCount64()+16;
    return result;
}
void recovery_runtime_reset();
#include "native_step_exit.inc"
#include "native_step_recovery_runtime.inc"
void recovery_runtime_reset(){recovery_runtime::reset();}
template<class T>void resolve(T& out,const char* name){out=reinterpret_cast<T>(SDL_GL_GetProcAddress(name));require(out!=nullptr,std::string("Step presentation entrypoint absent: ")+name);}
void initialize_pixels(){
    auto& a=pixels.gl;if(a.ready())return;
    resolve(a.error,"glGetError");resolve(a.get,"glGetIntegerv");resolve(a.is_enabled,"glIsEnabled");resolve(a.enable,"glEnable");resolve(a.disable,"glDisable");
    resolve(a.gen_textures,"glGenTextures");resolve(a.bind_texture,"glBindTexture");resolve(a.bind_buffer,"glBindBuffer");resolve(a.tex_parameter,"glTexParameteri");
    resolve(a.tex_image,"glTexImage2D");resolve(a.copy_image,"glCopyTexSubImage2D");resolve(a.gen_framebuffers,"glGenFramebuffers");resolve(a.bind_framebuffer,"glBindFramebuffer");
    resolve(a.attach,"glFramebufferTexture2D");resolve(a.complete,"glCheckFramebufferStatus");resolve(a.read_buffer,"glReadBuffer");resolve(a.draw_buffer,"glDrawBuffer");resolve(a.blit,"glBlitFramebuffer");
}
void capture(SDL_Window* target){
    if(!installed||!control.entered)return;
    initialize_pixels();int width=0,height=0;
    require(SDL_GetWindowSizeInPixels(target,&width,&height),std::string("Step presentation dimensions failed: ")+SDL_GetError());window=target;
    pixels.capture(reinterpret_cast<uintptr_t>(target),reinterpret_cast<uintptr_t>(SDL_GL_GetCurrentContext()),GetCurrentThreadId(),width,height);
}
void present(){
    SDL_PumpEvents();const auto now=GetTickCount64();if(!pixels.valid)return;
    if(now<next_present){++retained_pacing_skips;return;}
    int width=0,height=0;require(SDL_GetWindowSizeInPixels(window,&width,&height),std::string("Step held dimensions failed: ")+SDL_GetError());
    pixels.repaint(reinterpret_cast<uintptr_t>(window),reinterpret_cast<uintptr_t>(SDL_GL_GetCurrentContext()),GetCurrentThreadId(),width,height);
    ++retained_swaps.calls;const auto began=std::chrono::steady_clock::now();
    const bool result=original_swap(window);
    retained_swaps.finish(result,uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-began).count()));
    observe_successful_swap(result,true);
    require(result,std::string("Step held swap failed: ")+SDL_GetError());pixels.presented(result);next_present=now+16;
}
bool terminal_present=false,terminal_swap_ok=false;
bool terminal_presenting(){return terminal_present;}
void terminal_swap_result(bool result){terminal_swap_ok=result;}
std::string terminal_error;
void terminal_hold(void* game){
    SDL_PumpEvents();
    try{
        require(!outer_entered&&!control.entered&&!timer_step::frame.active&&thread&&thread==GetCurrentThreadId()&&
            reinterpret_cast<uintptr_t>(game)==owner,"Terminal service changed its outer Game/thread owner");
        require(native_steps&&native_steps->snapshot().phase==MT2FW_MT2_STEP_FAILED,"Terminal service requires a retained failed lease");
        held();outer_entered=true;
        struct Clear {~Clear(){outer_entered=false;terminal_present=false;}} clear;
        // Bootstrap can stop before the first normal InGame Desktop frame.
        // Rebind through the same current Mode/thread/registry checks before local UI maintenance.
        mt2::desktop::frame(german);
        diagnostics(true);
        native_steps->maintenance();
        require(native_steps->terminal_error().empty(),native_steps->terminal_error());
        if(exit_requested)return;
        const auto now=GetTickCount64();if(!pixels.valid||now<next_present)return;
        int width=0,height=0;require(SDL_GetWindowSizeInPixels(window,&width,&height),std::string("Terminal dimensions failed: ")+SDL_GetError());
        const auto receipt=native_steps->snapshot();std::string error;terminal_present=true;terminal_swap_ok=false;
        const bool shown=mt2::desktop::terminal_draw(receipt.lease,reinterpret_cast<uintptr_t>(window),width,height,[&]{
            pixels.repaint(reinterpret_cast<uintptr_t>(window),reinterpret_cast<uintptr_t>(SDL_GL_GetCurrentContext()),GetCurrentThreadId(),width,height);
        },error);
        terminal_present=false;require(shown,error);
        require(terminal_swap_ok,std::string("Terminal native Present lacks successful SDL swap: ")+SDL_GetError());
        pixels.presented(terminal_swap_ok);next_present=now+16;
        if(!terminal_error.empty()){runtime->log("steps","info","Terminal local presentation recovered");terminal_error.clear();}
    }catch(const std::exception& error){
        if(terminal_error!=error.what()){terminal_error=error.what();runtime->log("steps","error","Terminal local service failed: "+terminal_error);}
    }catch(...){
        const std::string cause="Terminal local service raised an unknown exception";
        if(terminal_error!=cause){terminal_error=cause;runtime->log("steps","error",cause);}
    }
}
#include "native_poll_delay.inc"
void configure(){
    if(!installed||configured||configuration_attempted)return;
    require(engine_thread&&engine_thread==GetCurrentThreadId(),"Step configuration requires actual bound game thread");thread=engine_thread;
    configuration_attempted=true;
    auto backend=mt2::make_native_step_backend(image,executable,*native_routes,
        {original_go,timer_step::original_update,reinterpret_cast<void*>(&go),reinterpret_cast<void*>(&timer)},before_step);
    snapshot_prepare::install();clock_pause::install();async_save::install();client_periodic_ui::install();
    polling_delay::install();
    const auto native_finish=backend.finish_all;
    recovery_runtime::native_finish=native_finish;
    backend.finish_all=[native_finish](void* tasks){
        const auto receipt=native_steps->snapshot();
        const bool ordered=GetCurrentThreadId()==thread&&receipt.thread==thread&&receipt.phase==MT2FW_MT2_STEP_ENTERED&&
            receipt.lease&&receipt.session&&receipt.world_epoch&&receipt.tick&&receipt.invocation&&receipt.callbacks_inflight==1&&
            receipt.original_attempted==1&&receipt.original_returned==0&&receipt.workers_held==0;
        if(ordered)polling_delay::proof();
        mt2::poll_delay::Scope scope(ordered,thread);
        loading_dispatch::finish_complete_step(tasks,native_finish);
    };
    snapshot_prepare::finish_all=backend.finish_all;
    snapshot_prepare::all_done=backend.all_done;
    backend.after_go=snapshot_prepare::after_go;
    backend.bootstrap=combine;backend.world_exited=[]{return exit_proof().world_exited;};
    driver=new mt2::NativeStepDriver(std::move(backend));
    require(native_steps->configure({[](const auto* host,const auto& spec){return driver->inspect(host,spec);},[](const auto& frame,auto& receipt){
        try{driver->advance(frame,receipt);}
        catch(...){control.entered=false;timer_step::frame.active=false;held_tasks=tasks();throw;}
        control.entered=false;timer_step::frame.active=false;held_tasks=tasks();resume_tasks=true;held();
        ++control.completed;control.phase=Control::Phase::Held;lifecycle.workers_held=1;
    },exit_proof,reset_driver,preflight_exit,snapshot_prepare::operation,exit_save,
    [](const auto* host,const auto& spec,auto proof,void* user){driver->recovery_local(host,spec,proof,user);},
    {recovery_runtime::local,recovery_runtime::prepare,recovery_runtime::capture,recovery_runtime::pump,recovery_runtime::retire}}),"Step adapter rejected common native driver");configured=true;
}
#include "native_step_rng_diagnostic.inc"
#include "native_step_checkpoint.inc"
void go(void* game){
    if(!failure.empty()){terminal_hold(game);return;}
    if(!driver||!driver->frozen()){original_go(game);return;}
    try{
        require(!outer_entered&&thread==GetCurrentThreadId(),"Step common outer frame reentered or changed thread");
        outer_entered=true;owner=reinterpret_cast<uintptr_t>(game);tasks();presentation_rng::check_building_widget();
        if(exit_requested){exit_frame(game);outer_entered=false;return;}
        const auto receipt=native_steps->snapshot();
        if(receipt.phase==MT2FW_MT2_STEP_FAILED){outer_entered=false;terminal_hold(game);return;}
        if(receipt.phase==MT2FW_MT2_STEP_RETIRING){present();outer_entered=false;return;}
        // A fully activated world first observes transport at its held boundary.
        // The normal source may require live membership and must run afterward.
        if(lifecycle.bootstrap_held&&receipt.phase==MT2FW_MT2_STEP_HELD){
            native_steps->boundary();checkpoint(native_steps->snapshot());present();outer_entered=false;return;
        }
        auto observed=driver->observation();
        if(!lifecycle.seed_attempted){seed(observed);native_steps->maintenance();outer_entered=false;present();return;}
        require(lifecycle.seed_returned==1,"Step seed attempt lacks acknowledgement; no replay");
        if(bootstrap.game){
            if(!lifecycle.bootstrap_held&&observed.phase==mt2::StepNativeObservation::Phase::Loaded)certify(observed);
            if(native_steps->snapshot().phase==MT2FW_MT2_STEP_HELD){native_steps->boundary();checkpoint(native_steps->snapshot());}else native_steps->maintenance();
            present();outer_entered=false;return;
        }
        if(observed.phase==mt2::StepNativeObservation::Phase::Preload){
            held();require(read<std::array<uint64_t,4>>(rng())==seeded_words&&read<int32_t>(rng()+0x20)==0,"Step seeded preload drifted before load");
            native_steps->maintenance();present();outer_entered=false;return;
        }
        require(observed.phase==mt2::StepNativeObservation::Phase::Loading,"Step original load lacks actual loading proof");
        if(held_tasks)release_workers();control.phase=Control::Phase::Loading;lifecycle.phase=MT2FW_MT2_STEP_LOADING;publish();
        control.entered=true;timer_step::begin(owner,MT2FW_MT2_STEP_FIXED_DT_BITS);
        original_go(game);timer_step::finish();control.entered=false;
        if(bootstrap_boundary::pending)capture_bootstrap();
        observed=driver->observation();
        if(bootstrap.game&&observed.phase==mt2::StepNativeObservation::Phase::Loaded)certify(observed);
        publish();native_steps->maintenance();outer_entered=false;
    }catch(const std::exception& error){
        failure=error.what();control.entered=false;timer_step::frame.active=false;control.phase=Control::Phase::Failed;
        try{if(!held_tasks)pause_workers();}catch(const std::exception& pause){failure+="; held restoration failed: "+std::string(pause.what());}
        catch(...){failure+="; held restoration raised unknown native exception";}
        retain_failure();
    }catch(...){
        failure="Step common lifecycle raised unknown native exception; no replay";control.entered=false;timer_step::frame.active=false;control.phase=Control::Phase::Failed;
        try{if(!held_tasks)pause_workers();}catch(const std::exception& pause){failure+="; held restoration failed: "+std::string(pause.what());}
        catch(...){failure+="; held restoration raised unknown native exception";}
        retain_failure();
    }
}
void install(const fs::path& game){
    if(lab::enabled())return;
    image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));executable=game/"MT2.exe";
    native_routes->catalog("framework.step.go",0x956730,true);native_routes->catalog("framework.step.timer",0xa36300,true);
    native_services->reserve("framework.step.go");native_services->reserve("framework.step.timer");
    const Json config={{"defer_loading_plans",true},{"task_barrier",true},{"bootstrap_boundary",true},{"presentation_rng",true},{"digital_rng",true},{"rng_trace",true}};
    presentation_rng::building_widget_owner=&building_widget_owner;
    loading_dispatch::install(config);task_boundary::install(config);bootstrap_boundary::install(config);presentation_rng::install(config);rng_trace::install(config);
    region_rng_load::install();
    verify_exit_bodies();
    presentation_rng::verify(0x5cbc90,1568,"be08e431cc80e32f2221116ffa72422b752873a767cf69dc2b890e69a9f6eb35");
    presentation_rng::verify(0x5ebd20,1536,"9cc67821c5fbcd7dc598f59a5d70bb6fc130b8ce318426476f5da9e1a0f1076d");
    presentation_rng::verify(0x5c9280,1376,"3cb9d22045b77ceaddd0fde3f375e8ee7a3c9cb51d828eb28702c527d1a65459");
    native_routes->catalog("framework.step.resume",0x526350,true);
    native_services->reserve("framework.step.resume");
    original_pause=reinterpret_cast<Go>(image+0x526240);original_resume=reinterpret_cast<Go>(image+0x526350);original_seed=reinterpret_cast<Seed>(image+0x9fc090);
    hook(reinterpret_cast<void*>(image+0x526350),reinterpret_cast<void*>(&command_hold::resume_entry),reinterpret_cast<void**>(&original_resume));
    hook(reinterpret_cast<void*>(image+0xa36300),reinterpret_cast<void*>(&timer),reinterpret_cast<void**>(&timer_step::original_update));
    hook(reinterpret_cast<void*>(image+0x956730),reinterpret_cast<void*>(&go),reinterpret_cast<void**>(&original_go));installed=true;
}
}
