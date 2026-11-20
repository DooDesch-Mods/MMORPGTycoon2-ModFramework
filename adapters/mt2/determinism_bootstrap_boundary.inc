// Included inside lab after timer_step. This boundary is private and opt-in.
namespace bootstrap_boundary {
using Enable=void(*)(void*,bool,bool);
using Scenes=void(*)(void*,bool);
using GetScene=void*(*)(void*,int);
Enable original_enable=nullptr;
using Draw=void(*)(void*);
Draw original_draw=nullptr;
uint16_t render_requested=0,render_consumed=0;
bool render_captured=false,render_returned=false,render_restored=false;
Scenes game_scenes=nullptr;
GetScene get_scene=nullptr;
bool enabled=false,pending=false,branch_changed=false,uncertain=false;
uintptr_t load_mode=0;
uint64_t deferred=0,resumed=0,load_frame=0;
std::string first_error;
constexpr uintptr_t Branch=0x4a7acc,Next=Branch+5,Continue=0x4a7126,Epilogue=0x4a74cf;
constexpr int32_t OriginalDelta=int32_t(int64_t(Continue)-int64_t(Next)),HeldDelta=int32_t(int64_t(Epilogue)-int64_t(Next));
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
#include "determinism_reveal_boundary.inc"
void branch_is(int32_t delta){
    require(read<uint8_t>(image+Branch)==0xe9&&read<int32_t>(image+Branch+1)==delta,"Bootstrap Mode branch changed");
}
void change_branch(int32_t before,int32_t after){
    branch_is(before);uncertain=true;
    auto* operand=reinterpret_cast<void*>(image+Branch+1);DWORD protection=0,discarded=0;
    if(!VirtualProtect(operand,sizeof after,PAGE_EXECUTE_READWRITE,&protection))
        throw std::runtime_error("Bootstrap Mode branch protection failed win32="+std::to_string(GetLastError()));
    // This is the displacement of an existing jump, not a new call frame.
    // Only the admitted game thread can be inside this Mode.Update body.
    std::memcpy(operand,&after,sizeof after);
    const bool restored=VirtualProtect(operand,sizeof after,protection,&discarded)!=0;
    const auto protection_error=restored?0:GetLastError();
    const bool flushed=FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(image+Branch),5)!=0;
    const auto flush_error=flushed?0:GetLastError();
    if(!restored||!flushed)throw std::runtime_error("Bootstrap Mode branch publication failed protection="+std::to_string(protection_error)+" cache="+std::to_string(flush_error));
    branch_is(after);uncertain=false;
}
uintptr_t current_mode(){
    require(thread==GetCurrentThreadId(),"Bootstrap game thread changed");
    require(control.entered&&timer_step::frame.active,"Bootstrap lacks admitted original outer frame");
    timer_step::current_game();
    const auto mode=read<uintptr_t>(owner+0x28);
    require(mode&&mode==read<uintptr_t>(image+0x13eb320)&&read<uintptr_t>(mode)==image+0x16ecad0,"Bootstrap Mode owner/address point changed");
    return mode;
}
void scene_state(bool value){
    require(read<uint8_t>(owner+0x33d)==uint8_t(value),"Bootstrap Game scene-enable readback differs");
    const auto system=read<uintptr_t>(image+0x1c55b40);
    require(system,"Bootstrap System is absent");
    const auto screen=read<uintptr_t>(system+0x70);
    require(screen,"Bootstrap Screen is absent");
    // Native Game enable excludes 7, 8 and 19. The following UI enable
    // overwrites 12, 13, 14, 17, 18 and 19, including five Game entries.
    // Check the composed original calls, retaining untouched scenes 7 and 8.
    for(int i=0;i<26;++i){if(i==7||i==8)continue;
        const bool ui=i==12||i==13||i==14||i==17||i==18||i==19;
        const auto expected=uint8_t(ui||value);
        const auto scene=reinterpret_cast<uintptr_t>(get_scene(reinterpret_cast<void*>(screen),i));
        if(!scene)throw std::runtime_error("Bootstrap scene is absent index="+std::to_string(i));
        const auto actual=read<uint8_t>(scene+0xb1);
        if(actual!=expected)throw std::runtime_error("Bootstrap scene update flag differs index="+std::to_string(i)+" pointer="+std::to_string(scene)+" expected="+std::to_string(expected)+" actual="+std::to_string(actual));
    }
}
uint16_t render_requests(){
    const auto first=read<uint8_t>(owner+0x438),second=read<uint8_t>(owner+0x439);
    if(first>1||second>1)throw std::runtime_error("Bootstrap render request flags invalid first="+std::to_string(first)+" second="+std::to_string(second));
    return uint16_t(first|(uint16_t(second)<<8));
}
void draw(void* game){
    if(!enabled||!control.active()||!pending||control.phase!=mt2::determinism::Gate::Phase::Loading){original_draw(game);return;}
    try{
        require(thread==GetCurrentThreadId(),"Bootstrap deferred draw game thread changed");
        require(current_mode()==load_mode&&reinterpret_cast<uintptr_t>(game)==owner&&load_frame==timer_step::frame.entered,
            "Bootstrap deferred draw owner/frame differs");
        require(!render_captured&&!render_returned,"Bootstrap deferred draw repeated");
        scene_state(false);
        render_requested=render_requests();render_captured=true;
        original_draw(game);
        render_consumed=uint16_t(render_requested&uint16_t(~render_requests()));render_returned=true;
    }catch(const std::exception& problem){if(first_error.empty())first_error=problem.what();throw;}
    catch(...){if(first_error.empty())first_error="Bootstrap deferred original draw raised; no replay";throw;}
}
void restore_render_requests(){
    require(pending&&resumed==1&&render_captured&&render_returned&&!render_restored,
        "Bootstrap render requests lack one completed deferred draw");
    scene_state(true);
    const auto current=render_requests(),desired=uint16_t(current|render_consumed);
    // DrawFrame consumed these native refresh requests while their scenes were
    // disabled. Reissue only those bits and retain any newer native requests.
    if(desired!=current){
        write(owner+0x438,uint8_t(desired&0xff));write(owner+0x439,uint8_t(desired>>8));
        require(render_requests()==desired,"Bootstrap render request restore readback differs");
    }
    render_restored=true;
}

void enable_call(void* receiver,bool world,bool ui,uintptr_t caller){
    if(!enabled||!control.active()||caller!=image+0x4a78d7){original_enable(receiver,world,ui);return;}
    try{
        require(control.phase==mt2::determinism::Gate::Phase::Loading,"Bootstrap load transition outside Loading");
        require(world&&ui&&!pending&&!deferred&&!uncertain,"Bootstrap load scene enable repeated or has unexpected arguments");
        load_mode=current_mode();
        require(load_mode==reinterpret_cast<uintptr_t>(receiver)&&read<int32_t>(load_mode+0x2c4)==3,"Bootstrap load transition Mode state differs");
        const auto map=read<uintptr_t>(image+0x13eb8c0);
        require(map&&read<uintptr_t>(map)==image+0x1748c00&&read<uint8_t>(map+0x47f)==1,"Bootstrap Map is not ready at native scene enable");
        render_requested=render_consumed=0;render_captured=render_returned=render_restored=false;
        change_branch(OriginalDelta,HeldDelta);branch_changed=true;pending=true;
        load_frame=timer_step::frame.entered;++deferred;
        original_enable(receiver,false,ui);scene_state(false);
    }catch(const std::exception& problem){if(first_error.empty())first_error=problem.what();throw;}
    catch(...){if(first_error.empty())first_error="Bootstrap scene enable raised; no replay";throw;}
}
__declspec(noinline) void enable(void* receiver,bool world,bool ui){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());enable_call(receiver,world,ui,caller);
}
void require_loaded(){
    if(!enabled)return;
    require(pending&&branch_changed&&!uncertain&&deferred==1&&!resumed&&load_frame==timer_step::frame.entered,"Bootstrap initial barrier lacks its completed load-only frame");
    require(load_mode==read<uintptr_t>(owner+0x28)&&read<int32_t>(load_mode+0x2c4)==4,"Bootstrap loaded Mode changed");
    branch_is(HeldDelta);scene_state(false);
}
void before_step(){
    if(!enabled||!pending)return;
    try{
        require(control.phase==mt2::determinism::Gate::Phase::Stepping&&control.completed==0&&!resumed&&!uncertain,"Bootstrap first-step admission differs");
        require(current_mode()==load_mode&&read<int32_t>(load_mode+0x2c4)==4,"Bootstrap first-step Mode changed");
        require(branch_changed&&deferred==1,"Bootstrap first-step lacks one deferred enable");scene_state(false);
        reveal::arm();
        change_branch(HeldDelta,OriginalDelta);branch_changed=false;
        // Original enable only changes scene admission flags. The first full
        // admitted Go then runs its original managers, Mode and scene updates.
        ++resumed;game_scenes(reinterpret_cast<void*>(owner),true);scene_state(true);restore_render_requests();pending=false;
    }catch(const std::exception& problem){if(first_error.empty())first_error=problem.what();throw;}
    catch(...){if(first_error.empty())first_error="Bootstrap first-step scene restore raised; no replay";throw;}
}
void cancel(){
    reveal::restore();
    if(!enabled||(!pending&&!branch_changed&&!uncertain))return;
    require(thread==GetCurrentThreadId()&&!control.entered&&!uncertain&&pending&&branch_changed&&!resumed,"Bootstrap cannot cancel an entered or uncertain load boundary");
    require(load_mode==read<uintptr_t>(owner+0x28)&&read<int32_t>(load_mode+0x2c4)==4,"Bootstrap cancellation lost its loaded Mode");
    scene_state(false);change_branch(HeldDelta,OriginalDelta);branch_changed=false;
    ++resumed;game_scenes(reinterpret_cast<void*>(owner),true);scene_state(true);restore_render_requests();pending=false;
}
Json status(){return {{"reveal_branch_changed",reveal::changed},{"reveal_branch_uncertain",reveal::uncertain},{"enabled",enabled},{"render_requested",render_requested},{"render_consumed",render_consumed},{"render_captured",render_captured},{"render_returned",render_returned},{"render_restored",render_restored},{"deferred",deferred},{"resumed",resumed},{"pending",pending},{"branch_changed",branch_changed},{"uncertain",uncertain},{"load_frame",load_frame},{"first_error",first_error}};}
void install(const Json& config){
    enabled=config.value("bootstrap_boundary",false);if(!enabled)return;
    for(const auto& [offset,size,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,11>{{
        {0x332920,5264,"55ca106d58de5c32fdc1fc661706a1ecf3662ef4144040141475375e8dbb43c4"},
        {0x468b20,480,"b88eeed9d89b938a0df3fe190dae394f2453c00a9f472555a7f61b06a8dabb58"},
        {0x467110,4896,"19ef1bbee72dae00858c5d2cb1c1c11280364531abb86ef57b73cb0fa70cf92c"},
        {0x9d2c30,880,"67927fcbd1b078e2a29d402a4af4c6096d592c2dc3029d17bb2be45854a48ae0"},
        {0x357bb0,800,"b02524d9b067e34f1875d2363122db019ce90ae4091f4214c0f04d2c91812acb"},
        {0x4a70a0,5088,"889f8e62727788ecc562f11485419b093cafb50de7164941e5bfc93aef24f757"},
        {0x464ab0,160,"d62d7da7076f4ec57f0f7adfef670a545328d181f0a63cb7aa7355f1823f41c2"},
        {0x4a4900,64,"4119e38e26a2a764078104484cf73d8b11d699ec149fcb473a6858efff323c6e"},
        {0x464a50,96,"dc3a0f5574b82a0b54b8162f93802f3b4f4779d8fcfe65e3e4e1cfa004445abc"},
        {0x9d3f60,80,"35b5eea29601158eebeb53137155f4b564a454e8231884781e246825df535a03"},
        {0x9d3300,912,"26a74f2482008368a315108428c18f1cb90995f7dda6e9556eeda4f9ea2234d3"}}}){
        std::string body(size,'\0');SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+offset),body.data(),size,&copied)||copied!=size||digest(body)!=sha)
            throw std::runtime_error("Bootstrap exact Mode/scene native body differs");
    }
    reveal::branch_is(0x72);branch_is(OriginalDelta);
    game_scenes=reinterpret_cast<Scenes>(image+0x464a50);
    get_scene=reinterpret_cast<GetScene>(image+0x9d3f60);
    hook(reinterpret_cast<void*>(image+0x4a4900),reinterpret_cast<void*>(&enable),reinterpret_cast<void**>(&original_enable));
    hook(reinterpret_cast<void*>(image+0x468b20),reinterpret_cast<void*>(&draw),reinterpret_cast<void**>(&original_draw));
}
}
