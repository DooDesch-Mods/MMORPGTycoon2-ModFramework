// Included by the Framework adapter only in the private experiment candidate.
namespace lab {
namespace native=mt2fw::game::native;
mt2::determinism::Gate control;
using Go=void(*)(void*);
Go original_go=nullptr;
Go original_pause=nullptr,original_resume=nullptr;
using Seed=void(*)(void*,uint64_t);
Seed original_seed=nullptr;
uint64_t seed_input=0;
uint32_t reset_attempted_count=0,reset_count=0;
bool load_attempted=false,load_confirmed=false;
bool camera_attempted=false;
std::set<std::string> time_tokens;
Json seed_initialization;
bool installed=false;
std::atomic<bool> fixture_opened=false,watch_open=false;
std::mutex observation_gate;
std::string observation_error;
uint64_t loading_frames=0;
uintptr_t owner=0,image=0;
uintptr_t held_tasks=0;
bool resume_tasks=false;
DWORD thread=0;
std::string fixture_path,fixture_sha;
Json retained;
bool active(){return control.active();}
bool enabled(){return installed;}
Json load_status(){
    Json out={{"phase",control.name()},{"error",control.error}};
    // publish_failure already retained this evidence at the original failure.
    // Returning it must not re-read native state or rerun the failed witness.
    if(control.phase==mt2::determinism::Gate::Phase::Failed)out["failure_evidence"]=retained;
    return out;
}


template<class T>T read(uintptr_t address){
    T value{};SIZE_T copied=0;
    if(!address||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),&value,sizeof value,&copied)||copied!=sizeof value)
        throw std::runtime_error("Determinism witness native read failed address="+std::to_string(address)+" bytes="+std::to_string(sizeof value));
    return value;
}
template<class T>void write(uintptr_t address,const T& value){
    SIZE_T copied=0;
    if(!WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),&value,sizeof value,&copied)||copied!=sizeof value||read<T>(address)!=value)
        throw std::runtime_error("Determinism exact dt store/readback failed");
}
uint64_t rva(uintptr_t value){if(value<image||value-image>=0x2000000)throw std::runtime_error("Determinism native class/callback lies outside pinned image");return value-image;}
uint32_t bits(float value){return std::bit_cast<uint32_t>(value);}
#include "determinism_rng_trace.inc"
#include "determinism_presentation_rng.inc"
#include "determinism_timer_step.inc"
#include "determinism_motion_probe.inc"
#include "determinism_loading_dispatch.inc"
#include "determinism_task_boundary.inc"
#include "determinism_bootstrap_boundary.inc"
#include "determinism_building_receipt.inc"
#include "determinism_npc_receipt.inc"
#include "determinism_wait_render.inc"
#include "determinism_presentation_probe.inc"
#include "determinism_terrain_probe.inc"
void pause_workers(){
    const auto tasks=read<uintptr_t>(image+0x13df340);
    if(read<uintptr_t>(owner+0x68)!=tasks||rva(read<uintptr_t>(tasks))!=0x16fdaf0)
        throw std::runtime_error("Determinism Task owner changed before pause");
    if(held_tasks&&held_tasks!=tasks)throw std::runtime_error("Determinism held Task owner changed");
    if(!held_tasks){held_tasks=tasks;resume_tasks=read<uint8_t>(tasks+0xf8)==0;}
    // Native Pause holds its mutex and drains running/waiting work before return.
    // Its internal wait is not bounded. The owned-process runner retains timeouts.
    original_pause(reinterpret_cast<void*>(tasks));
    if(read<uint8_t>(tasks+0xf8)!=1||read<int32_t>(tasks+0xa8)||read<int32_t>(tasks+0x90))
        throw std::runtime_error("Determinism native Task Pause did not establish a drained held scheduler");
}
void release_workers(){
    if(!held_tasks)return;
    if(read<uintptr_t>(image+0x13df340)!=held_tasks||read<uintptr_t>(owner+0x68)!=held_tasks)
        throw std::runtime_error("Determinism held Task owner changed before release");
    if(resume_tasks){
        original_resume(reinterpret_cast<void*>(held_tasks));
        if(read<uint8_t>(held_tasks+0xf8)!=0)throw std::runtime_error("Determinism native Task Resume was not acknowledged");
    }
    held_tasks=0;resume_tasks=false;
}
std::array<uint64_t,4> seed_readback(uintptr_t rng){
    std::array<uint64_t,4> words{};for(unsigned i=0;i<4;++i)words[i]=read<uint64_t>(rng+i*8);return words;
}
uintptr_t seed_receiver(){
    const auto rng=read<uintptr_t>(image+0x164b250);
    if(rng!=image+0x1c4e7a0)throw std::runtime_error("Determinism seed Default receiver differs");
    return rng;
}
void verify_seed_barrier(){
    if(reset_count!=1||reset_attempted_count!=1||!held_tasks||read<uint8_t>(held_tasks+0xf8)!=1||read<int32_t>(held_tasks+0xa8)||read<int32_t>(held_tasks+0x90))
        throw std::runtime_error("Determinism seed barrier lacks one reset and drained paused workers");
    if(read<uintptr_t>(image+0x13df340)!=held_tasks||read<uintptr_t>(owner+0x68)!=held_tasks)
        throw std::runtime_error("Determinism Task owner changed across seed barrier");
    const auto rng=seed_receiver();
    if(seed_readback(rng)!=mt2::determinism::Gate::seed_words(seed_input)||read<int32_t>(rng+0x20)!=0)
        throw std::runtime_error("Determinism Default RNG changed after seed barrier; load rejected without repair");
}
void establish_seed_barrier(){
    if(control.phase!=mt2::determinism::Gate::Phase::Arming||reset_attempted_count||load_attempted)
        throw std::runtime_error("Determinism seed initialization was already attempted or is outside arming boundary");
    native::WorldStatus state;std::string cause;
    if(!native::read_world_status(state,cause)||state.active_mode_index!=2)throw std::runtime_error("Determinism seed barrier requires native title mode: "+cause);
    pause_workers();wait_render::arm();presentation_rng::arm();const auto rng=seed_receiver();
    ++reset_attempted_count;original_seed(reinterpret_cast<void*>(rng),seed_input);
    const auto words=seed_readback(rng);const auto extra=read<int32_t>(rng+0x20);
    seed_initialization={{"seed",seed_input},{"reset_count",0},{"words",words},{"extra",extra}};
    if(words!=mt2::determinism::Gate::seed_words(seed_input)||extra!=0)
        throw std::runtime_error("Determinism original InitWithSeed returned unexpected s[4]/p readback");
    reset_count=1;seed_initialization["reset_count"]=reset_count;
    verify_seed_barrier();control.seeded();
}
void isolated(){
    if(!runtime||!runtime->plugins().empty()||GetModuleHandleW(L"mt2_multiplayer.dll")||GetModuleHandleW(L"MT2_Multiplayer.dll")||GetModuleHandleW(L"MT2MultiplayerPlugin.dll"))
        throw std::runtime_error("Determinism experiment requires an empty Framework plugin set and no Multiplayer module");
    if(thread&&thread!=GetCurrentThreadId())throw std::runtime_error("Determinism gate game thread changed");
}
Json metadata(){return {{"schema_version",2},{"terrain_probe",terrain_probe::status()},{"wait_render",wait_render::status()},{"fixed_timestep",timer_step::status()},{"task_boundary",task_boundary::status()},{"loading_dispatch",loading_dispatch::status()},{"motion_probe",motion_probe::status()},{"bootstrap_boundary",bootstrap_boundary::status()},{"experiment_id",control.experiment},{"phase",control.name()},
    {"completed_steps",control.completed},{"attempted_steps",control.attempted},{"remaining_steps",control.remaining},
    {"rng_trace",rng_trace::status()},{"presentation_rng",presentation_rng::status()},{"dt_bits",control.dt_bits},{"loading_frames",loading_frames},{"error",control.error},
    {"seed_initialization",seed_initialization},{"reset_attempted_count",reset_attempted_count},{"load_attempted",load_attempted},{"load_confirmed",load_confirmed}};}
Json witness(){
    isolated();presentation_rng::check();std::string cause;native::WorldStatus world;native::SimulationState sim;native::SoftwareState software;native::CameraState camera;
    if(!native::read_world_status(world,cause)||!native::read_simulation_state(sim,cause)||!native::read_software_state(software,cause)||!native::read_camera_state(camera,cause))throw std::runtime_error(cause);
    if(!fixture_opened||world.active_mode_index!=3||world.in_game_state!=4||!world.world_present||world.quitting||world.edit_operations||world.saves_in_progress)
        throw std::runtime_error("Determinism witness requires witnessed fixture and current state4/edit0/save0 World");
    std::string folder;if(!native::read_active_world_folder(folder,cause)||folder!=fs::path(fixture_path).parent_path().filename().string())throw std::runtime_error("Determinism loaded folder differs: "+cause);
    if(software.replica_armed||software.replica_bound)throw std::runtime_error("Determinism witness found active Software replica");
    const auto game=read<uintptr_t>(image+0x13df8f0);
    const auto game_ap=game?read<uintptr_t>(game):0,expected_game_ap=image+0x1743730+0x10;
    if(owner!=game||game_ap!=expected_game_ap)throw std::runtime_error("Determinism current game receiver differs: owner_expected="+std::to_string(owner)+" owner_actual="+std::to_string(game)+" ap_expected="+std::to_string(expected_game_ap)+" ap_actual="+std::to_string(game_ap));
    const auto rng=read<uintptr_t>(image+0x164b250),tasks=read<uintptr_t>(image+0x13df340),ticks=read<uintptr_t>(image+0x13df350),updates=read<uintptr_t>(image+0x13df4a0);
    if(rng!=image+0x1c4e7a0)throw std::runtime_error("Determinism Default RNG object differs");
    if(read<uintptr_t>(game+0x68)!=tasks||read<uintptr_t>(game+0x150)!=ticks||read<uintptr_t>(game+0x70)!=updates||
       rva(read<uintptr_t>(tasks))!=0x16fdaf0||rva(read<uintptr_t>(ticks))!=0x16fdc40||rva(read<uintptr_t>(updates))!=0x1709650)
        throw std::runtime_error("Determinism scheduler singleton ownership differs");
    const auto random=read<std::array<uint64_t,4>>(rng);
    const auto random_extra=read<int32_t>(rng+0x20);
    const auto building_keys=building_receipt::snapshot();
    const auto npc_keys=npc_receipt::snapshot();
    mt2::determinism::WorkWitness work([](uintptr_t address,size_t size){
        mt2::determinism::WorkWitness::Bytes bytes(size);SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),bytes.data(),size,&copied)||copied!=size)
            throw std::runtime_error("Determinism work witness native copy failed address="+std::to_string(address)+" required="+std::to_string(size)+" actual="+std::to_string(copied));
        return bytes;
    },image,game,building_keys);
    const auto path_nodes=npc_receipt::installed?work.bind_fixture_dispatch(npc_keys):Json(nullptr);
    const auto internal_clock=work.clock(),update_schedule=work.updates();
    const bool before_first_mode_update=control.completed==0&&control.attempted==0&&bootstrap_boundary::enabled&&
        bootstrap_boundary::deferred==1&&bootstrap_boundary::pending&&!bootstrap_boundary::resumed&&
        bootstrap_boundary::branch_changed&&!bootstrap_boundary::uncertain;
    const auto developers=work.developers(before_first_mode_update),tutorial=work.tutorial();
    const auto subscribers=work.subscribers();
    const auto npcs=npc_receipt::installed?work.npcs(npc_keys):Json(nullptr);
    const auto fixture_buildings=work.fixture_building_poses(3),development=work.development_project(),finished=work.finished_tasks();
    Json clocks=Json::array(),local_clocks=Json::array();
    for(unsigned slot=0;slot<4;++slot){const auto tick=work.tick_clock(slot);
        clocks.push_back(tick.at("comparable"));local_clocks.push_back(tick.at("local_evidence"));}
    const auto running_tasks=read<int32_t>(tasks+0xa8),waiting_tasks=read<int32_t>(tasks+0x90);
    if(held_tasks!=tasks||read<uint8_t>(tasks+0xf8)!=1||running_tasks||waiting_tasks)throw std::runtime_error("Determinism witness requires native paused/drained Task scheduler");
    const auto snapshot_rng=read<std::array<uint64_t,4>>(rng);
    if(snapshot_rng!=random||random_extra!=read<int32_t>(rng+0x20)||read<uint8_t>(tasks+0xf8)!=1||running_tasks!=read<int32_t>(tasks+0xa8)||waiting_tasks!=read<int32_t>(tasks+0x90))
        throw std::runtime_error("Determinism worker/RNG drift during witness capture");
    work.finish();
    if(npc_receipt::snapshot()!=npc_keys)throw std::runtime_error("NPC receipt changed during complete work witness");
    if(building_receipt::snapshot()!=building_keys)throw std::runtime_error("Building receipt changed during complete work witness");
    Json gaps=Json::array({"full logical world and transient entity state not yet captured","worker callback/completion queues lack canonical locked snapshot","UpdateManager receiver state and in-step registrations not yet captured","unobserved background producers are not excluded","Clock cached window graph and EventListener state not yet captured"});
    for(const auto& gap:work.gaps())gaps.push_back(gap);
    Json out=metadata();out["comparable"]={
        {"seed_input",seed_input},{"default_rng",random},{"default_rng_extra",random_extra},{"clock",{{"scale_bits",bits(sim.clock.time_scale)},{"pause_scale_bits",bits(sim.clock.pause_scale)},{"day",sim.clock.flat_day},{"seconds",sim.clock.seconds}}},
        {"economy",{{"cash",sim.cash},{"record_present",sim.record_present},{"record_day",sim.record_day},{"record_cash",sim.record_cash},{"record_income",sim.record_income},{"record_expense",sim.record_expense},{"line_counts",sim.line_counts},{"line_income",sim.line_income},{"logged_in_subscribers",sim.logged_in_subscribers}}},
        {"software",{{"rating_bits",bits(software.rating)},{"fun_bits",bits(software.fun)}}},{"tick_clocks",clocks},
        {"task_manager",{{"paused",read<uint8_t>(tasks+0xf8)},{"running",running_tasks},{"waiting",waiting_tasks},{"resume_before_step",resume_tasks}}},
        {"scheduler_pause_flags",{{"tick",read<uint8_t>(ticks+0x28)},{"update",read<uint8_t>(updates+0x68)}}},
        {"camera",{{"x_bits",bits(camera.x)},{"y_bits",bits(camera.y)},{"z_bits",bits(camera.z)},{"focus_x_bits",bits(camera.focus_x)},{"focus_y_bits",bits(camera.focus_y)},{"focus_z_bits",bits(camera.focus_z)},{"distance_bits",bits(camera.distance)},{"edit_weight_bits",bits(camera.edit_weight)}}}};
    out["local_evidence"]={{"game",game},{"rng",rng},{"task_manager",tasks},{"tick_manager",ticks},{"update_manager",updates},{"tick_clocks",local_clocks},{"fixture_path",fixture_path},{"fixture_sha256",fixture_sha},{"fixture_opened",fixture_opened.load()},{"thread",thread},{"initial_barrier",bootstrap_boundary::enabled?"load-completion Mode epilogue before ordinary Mode body; world-scene updates held; load callbacks retained":"first completed state4 outer frame; not proven before first simulation update"}};
    out["comparable"]["clock_internal"]=internal_clock.at("comparable");
    out["comparable"]["update_schedule"]=update_schedule.at("comparable");
    out["local_evidence"]["clock_internal"]=internal_clock.at("local_evidence");
    out["local_evidence"]["update_schedule"]=update_schedule.at("local_evidence");
    out["comparable"]["fixture_building_poses"]=fixture_buildings.at("comparable");
    out["comparable"]["development_project"]=development.at("comparable");
    out["comparable"]["task_manager"]["finished_count"]=finished.at("comparable").at("finished_count");
    out["local_evidence"]["fixture_building_poses"]=fixture_buildings.at("local_evidence");
    out["local_evidence"]["development_project"]=development.at("local_evidence");
    out["local_evidence"]["finished_tasks"]=finished.at("local_evidence");
    // Paused workers cannot mutate the captured state. Finished callbacks remain
    // queued for the next original Dispatch and are not executed by this reader.
    out["local_evidence"]["quiescent_scope"]="workers paused/drained; sampled bytes stable; finished callbacks retained";
    out["local_evidence"]["finished_callbacks_empty"]=finished.at("empty_observed");
    out["comparable"]["developers"]=developers.at("comparable");
    out["comparable"]["tutorial"]=tutorial.at("comparable");
    out["local_evidence"]["developers"]=developers.at("local_evidence");
    out["local_evidence"]["tutorial"]=tutorial.at("local_evidence");
    out["comparable"]["subscribers"]=subscribers.at("comparable");
    out["local_evidence"]["subscribers"]=subscribers.at("local_evidence");
    out["local_evidence"]["motion_probe"]=motion_probe::snapshot(subscribers.at("local_evidence").at("subscribers"));
    out["local_evidence"]["presentation_probe"]=presentation_probe::snapshot();
    if(!npcs.is_null()){out["comparable"]["npcs"]=npcs.at("comparable");out["local_evidence"]["npcs"]=npcs.at("local_evidence");}
    if(!path_nodes.is_null()){out["comparable"]["fixture_path_nodes"]=path_nodes.at("comparable");out["local_evidence"]["fixture_path_nodes"]=path_nodes.at("local_evidence");}
    out["coverage"]={{"complete",false},{"missing",gaps}};
    out["quiescent"]=running_tasks==0&&waiting_tasks==0;
    return out;
}
void publish_failure(const std::string& cause){rng_trace::stop();presentation_rng::cancel();control.fail(cause);auto failed=metadata();if(retained.contains("comparable"))failed["prior_witness"]=retained;failed["coverage"]={{"complete",false},{"missing",Json::array({"witness capture failed"})}};retained=std::move(failed);report(cause);}
void opened(const char* name,void* handle)noexcept{
    if(!watch_open.load(std::memory_order_acquire)||!name||"user/"+fixture_path!=name)return;
    try{
        if(!handle)throw std::runtime_error("Determinism selected fixture original open failed");
        const auto path=physical_save(name,false);
        if(path.empty()||file_digest(path)!=fixture_sha)throw std::runtime_error("Determinism actual opened fixture hash differs");
        fixture_opened=true;
    }catch(const std::exception& problem){std::lock_guard lock(observation_gate);observation_error=problem.what();}
    catch(...){std::lock_guard lock(observation_gate);observation_error="Determinism fixture observer raised; original open result retained";}
}
void pump(){
    diagnostics();
    SDL_Event event{};unsigned count=0;
    while(count++<256&&original_poll(&event)){
        if(event.type==SDL_EVENT_QUIT||(event.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED&&window&&event.window.windowID==SDL_GetWindowID(window))){
            std::string cause;if(!native::request_exit_game(cause))publish_failure("Determinism owned close failed: "+cause);
        }
    }
}
void go(void* game){
    if(!control.active()){original_go(game);return;}
    try{
        isolated();if(!thread)thread=GetCurrentThreadId();if(!owner)owner=reinterpret_cast<uintptr_t>(game);
        if(owner!=reinterpret_cast<uintptr_t>(game))throw std::runtime_error("Determinism outer receiver changed");
        if(control.phase==mt2::determinism::Gate::Phase::Arming){establish_seed_barrier();retained=metadata();return;}
        if(control.held()){pump();if(control.held()){wait_render::held();Sleep(1);}return;}
        const bool stepping=control.phase==mt2::determinism::Gate::Phase::Stepping;
        if(!control.enter())return;
        timer_step::begin(owner,control.dt_bits);
        if(stepping)bootstrap_boundary::before_step();
        write(owner+0x1c,control.dt_bits);
        if(held_tasks)release_workers();
        terrain_probe::begin();
        try{original_go(game);timer_step::finish();terrain_probe::finish();}catch(...){terrain_probe::abandon();timer_step::abandon();control.raised("Original outer Go or fixed timestep boundary raised; attempted step has unknown effects; no replay");throw;}

        control.returned();
        {std::lock_guard lock(observation_gate);if(!observation_error.empty())throw std::runtime_error(observation_error);}
        if(control.phase==mt2::determinism::Gate::Phase::Loading){
            ++loading_frames;
            native::WorldStatus state;std::string cause;if(!native::read_world_status(state,cause))throw std::runtime_error(cause);
            if(state.active_mode_index==3&&state.in_game_state==4&&state.world_present&&!state.saves_in_progress&&!state.edit_operations){
                task_boundary::require_loaded();bootstrap_boundary::require_loaded();pause_workers();timer_step::restore_held();rng_trace::stop();control.loaded();retained=witness();}
        }else if(stepping){pause_workers();timer_step::restore_held();retained=witness();}
    }catch(const std::exception& problem){timer_step::abandon();control.entered=false;publish_failure(problem.what());}
    catch(...){timer_step::abandon();control.entered=false;publish_failure("Determinism native outer frame raised; no replay");}
}
bool allowed(const std::string& op){return !control.active()||op.starts_with("determinism_")||op=="status"||op=="engine_status"||op=="exit"||(control.phase==mt2::determinism::Gate::Phase::Preload&&op=="load");}
Json request(const std::string& op,const Json& args){
    if(!installed)throw std::runtime_error("Private determinism marker/hook not installed");isolated();
    if(op=="determinism_arm"){
        native::WorldStatus world;std::string cause;if(!native::read_world_status(world,cause)||world.active_mode_index!=2)throw std::runtime_error("Determinism arm requires native title mode: "+cause);
        const auto path=args.at("fixture_path").get<std::string>();const auto expected=args.at("fixture_sha256").get<std::string>();
        if(!path.starts_with("save/")||path.find("..")!=std::string::npos||expected.size()!=64||expected.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("Determinism fixture path/hash invalid");
        const auto actual=physical_save(("user/"+path).c_str(),false);if(actual.empty()||file_digest(actual)!=expected)throw std::runtime_error("Determinism fixture is not the expected private mounted file");
        if(!args.at("seed").is_number_unsigned())throw std::runtime_error("Determinism seed must be an unsigned 64-bit JSON integer");
        const auto requested_seed=args.at("seed").get<uint64_t>();
        building_receipt::validate_pin(expected);npc_receipt::validate_pin(expected);
        control.arm(args.at("experiment_id").get<std::string>(),args.at("dt_bits").get<uint32_t>());
        seed_input=requested_seed;
        fixture_path=path;fixture_sha=expected;fixture_opened=false;watch_open.store(true,std::memory_order_release);owner=0;thread=GetCurrentThreadId();building_receipt::arm();npc_receipt::arm();retained=metadata();
    }else if(op=="load"){
        try{
            if(control.phase!=mt2::determinism::Gate::Phase::Preload||load_attempted||args.at("path").get<std::string>()!=fixture_path)
                throw std::runtime_error("Determinism original load requires the seeded barrier and exact fixture once");
            verify_seed_barrier();load_attempted=true;
            loading_dispatch::ordered_run::arm(held_tasks);rng_trace::start();auto result=mt2::diagnostic(op,args);load_confirmed=true;control.start_loading();return result;
        }catch(const std::exception& problem){publish_failure(std::string("Determinism original load failed; no replay: ")+problem.what());throw;}
    }else if(op=="determinism_step"){
        if(control.phase!=mt2::determinism::Gate::Phase::Held)throw std::runtime_error("Step requires held barrier");
        auto before=witness();if(!before.at("quiescent").get<bool>())throw std::runtime_error("Step rejected: pending native tasks at start barrier");
        control.step(args.at("token").get<std::string>(),args.at("expected_step").get<uint64_t>(),args.at("count").get<uint32_t>());
    }
    else if(op=="determinism_time"){
        const auto token=args.at("token").get<std::string>();
        const bool paused=args.at("paused").get<bool>();const auto speed=args.at("speed").get<float>();
        if(control.phase!=mt2::determinism::Gate::Phase::Held||
           args.at("expected_step").get<uint64_t>()!=control.completed||
           !mt2::determinism::Gate::identifier(token)||time_tokens.contains(token)||time_tokens.size()>=16||
           (speed!=1.0f&&speed!=3.0f&&speed!=9.0f))
            throw std::runtime_error("Time command requires exact held step, original speed and unused bounded token");
        witness();time_tokens.insert(token);
        try{
            std::string cause;native::ClockState clock;
            if(!native::set_clock(paused,speed,cause)||!native::read_clock(clock,cause))throw std::runtime_error(cause);
            if(clock.time_scale!=(paused?0.0f:speed))throw std::runtime_error("Original time command readback differs");
            return {{"accepted",true},{"token",token},{"completed_steps",control.completed},
                {"scale_bits",bits(clock.time_scale)},{"pause_scale_bits",bits(clock.pause_scale)},
                {"day",clock.flat_day},{"seconds",clock.seconds}};
        }catch(const std::exception& problem){publish_failure(std::string("Time command failed; no replay: ")+problem.what());throw;}
    }
    else if(op=="determinism_camera_focus"){
        if(control.phase!=mt2::determinism::Gate::Phase::Held||camera_attempted||
           args.at("expected_step").get<uint64_t>()!=control.completed)
            throw std::runtime_error("Camera perturbation requires the exact held step and one attempt");
        auto before=witness();native::CameraState camera;std::string cause;
        if(!native::read_camera_state(camera,cause))throw std::runtime_error(cause);
        float distance=std::clamp(camera.distance*0.5f,camera.minimum_distance,camera.maximum_distance);
        float x=camera.focus_x+256.0f,y=camera.focus_y,z=camera.focus_z;
        Json grounded=nullptr;
        if(args.contains("presentation_npc_key")){
            const auto target=presentation_probe::npc_target(before,args,camera.minimum_distance,camera.maximum_distance);
            x=target.x;y=target.y;z=target.z;distance=target.distance;
            grounded={{"saved_key",target.saved_key},{"region",target.region}};
        }
        camera_attempted=true;
        try{
            if(!native::diagnostic_focus_camera(x,y,z,distance,camera,cause))throw std::runtime_error(cause);
            auto after=witness();auto prior=before.at("comparable"),current=after.at("comparable");
            prior.erase("camera");current.erase("camera");
            if(prior!=current)throw std::runtime_error("Camera request changed observed simulation state at the held boundary");
            return {{"accepted",true},{"completed_steps",control.completed},
                {"target",{{"x",x},{"y",y},{"z",z},{"distance",distance}}},
                {"motion_pending",camera.focus_pending},{"simulation_unchanged",true},{"grounded_owner",grounded}};
        }catch(const std::exception& problem){publish_failure(std::string("Camera perturbation failed; no replay: ")+problem.what());throw;}
    }
    else if(op=="determinism_capture"){
        try{return wait_render::capture_request(args);}
        catch(const std::exception& problem){wait_render::error=std::string("Held image capture failed; no replay: ")+problem.what();publish_failure(wait_render::error);throw;}
        catch(...){wait_render::error="Held image capture failed; unknown cause; no replay";publish_failure(wait_render::error);throw;}
    }
    else if(op=="determinism_read"){
        if(control.phase!=mt2::determinism::Gate::Phase::Held)throw std::runtime_error("Witness requires a successful held barrier");
        retained=witness();return retained;
    }else if(op=="determinism_status"){auto out=metadata();out["last_witness"]=retained;return out;}
    else if(op=="determinism_cancel"){if(control.entered)throw std::runtime_error("Cannot cancel an entered outer frame");rng_trace::stop();presentation_rng::cancel();building_receipt::cancel();npc_receipt::cancel();loading_dispatch::cancel();bootstrap_boundary::cancel();release_workers();control.cancel();}
    else throw std::runtime_error("Unknown determinism operation");
    return metadata();
}
void install(){
    const auto marker=root/"diagnostics/determinism.json";if(!fs::exists(marker))return;
    bool owned=false;for(auto part:root){auto text=part.string();std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return char(std::tolower(c));});owned|=text=="_mptest";}
    const auto config=read_json(marker);
    if(!owned||config.value("schema_version",0)!=1||!config.value("enabled",false)||config.value("executable_sha256",std::string())!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")throw std::runtime_error("Private determinism marker requires exact owned _MPTest profile/build");
    image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    for(const auto& [offset,size,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,3>{{
        {0x526240,272,"63648e01ead82806b5d976287890d6653f1e5745387b356aaaeb629d1ea09f87"},
        {0x526350,160,"0acc0efdcbc9ed4445426d1bd10320d9b5827bf3781840eef1b2e467e2bc2c1a"},
        {0x9fc090,192,"7a7ab329d2eb747de62d61f791723eef44f3e850a468747f41708e6d08b981e5"}}}){
        std::string body(size,'\0');SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+offset),body.data(),size,&copied)||copied!=size||digest(body)!=sha)
            throw std::runtime_error("Exact original Task Pause/Resume or InitWithSeed body differs");
    }
    std::string body(336,'\0');SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+0x956730),body.data(),body.size(),&copied)||copied!=body.size()||digest(body)!="dafb71c7578812f1264c7121494d5ceff01d38931467babd20942bbec69eb994")throw std::runtime_error("Exact original coreGame::Go body differs");
    timer_step::install();
    motion_probe::install(config);
    loading_dispatch::install(config);
    task_boundary::install(config);
    bootstrap_boundary::install(config);
    presentation_probe::install(config);
    terrain_probe::install(config);
    presentation_rng::install(config);
    wait_render::enabled=config.value("wait_render",false);
    building_receipt::install(config);npc_receipt::install(config);rng_trace::install(config);
    original_pause=reinterpret_cast<Go>(image+0x526240);original_resume=reinterpret_cast<Go>(image+0x526350);
    original_seed=reinterpret_cast<Seed>(image+0x9fc090);
    hook(reinterpret_cast<void*>(image+0x956730),reinterpret_cast<void*>(&go),reinterpret_cast<void**>(&original_go));installed=true;
}
}
