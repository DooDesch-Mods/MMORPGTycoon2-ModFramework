// Shared ordered preparation preserves the game's deferred-time flush. Capture
// consumes its exact held boundary without executing that flush a second time.
namespace snapshot_prepare {
using Request=MT2FW_MT2_SnapshotRequest;
enum class Phase {Idle,Armed,Prepared,Sealed,Consumed,Released,Failed};
Phase phase=Phase::Idle;
Request certificate{};
Go original_flush=nullptr,cache_finish=nullptr;
using ClockSet=void(*)(void*,float);
ClockSet set_pause=nullptr;
std::function<void(void*)> finish_all;
std::function<bool(void*)> all_done;
uint64_t last_order=0,flush_attempts=0,flush_returns=0,captures=0;
bool capture_returned=false,client_cache_attempted=false,client_cache_returned=false;
uintptr_t prepared_mode=0,prepared_tasks=0;
bool installed=false;
struct RecoveryOwner {uint64_t lease=0,interruption=0,generation=0;char nonce[37]{};} recovery_owner;

bool empty_hash(const Request& request){return std::all_of(std::begin(request.witness_sha256),std::end(request.witness_sha256),[](auto b){return b==0;});}
void shape(const Request& request){
    require(request.size==sizeof(request)&&!request.reserved&&request.lease&&request.session&&request.world_epoch&&request.tick&&request.order,
        "Snapshot request size/reserved/identity is invalid");
    require(request.nonce[36]==0,"Snapshot nonce is not terminated UUID");
    for(size_t i=0;i<36;++i){const char c=request.nonce[i];
        require((i==8||i==13||i==18||i==23)?c=='-':((c>='0'&&c<='9')||(c>='a'&&c<='f')),"Snapshot nonce is not canonical UUID");}
    require(request.peer_mask&&request.peer_mask<=15&&(request.peer_mask&1)&&!(request.ack_mask&~request.peer_mask),"Snapshot peer mask is invalid");
}
uintptr_t mode(){
    timer_step::current_game();
    require(thread==GetCurrentThreadId()&&owner==timer_step::frame.game,"Snapshot native Game/thread differs");
    const auto value=read<uintptr_t>(owner+0x28);
    require(value&&value==bootstrap.mode&&read<uintptr_t>(value)==image+0x16e6730,"Snapshot active Mode differs");
    require(read<int32_t>(value+0x30c)==0,"Snapshot requires idle native Save count actual="+std::to_string(read<int32_t>(value+0x30c))+" required=0");
    return value;
}
uintptr_t clock(){
    const auto value=read<uintptr_t>(image+0x13df9a0);
    const auto state=read<uintptr_t>(mode()+0x58);
    require(value&&read<uintptr_t>(value)==image+0x17499d0&&read<uintptr_t>(state+0x158)==value,"Snapshot Clock current owner differs");
    require(read<uint32_t>(value+0x58)==0,"Snapshot requires explicitly paused selected Clock scale");
    return value;
}
#include "native_snapshot_queue_diagnostic.inc"
void empty_tasks(){
    held();
    for(size_t offset:{size_t(0x28),size_t(0x40),size_t(0x58),size_t(0x70),size_t(0x88),size_t(0xa0),size_t(0xb8)}){
        const auto count=read<int32_t>(held_tasks+offset+8),capacity=read<int32_t>(held_tasks+offset+12);
        if(count!=0||capacity<0||capacity>1000000)reject_queue("Snapshot requires empty native task queue offset="+std::to_string(offset)+
            " actual="+std::to_string(count)+" required=0");
    }
    const auto count=read<int32_t>(held_tasks+0xd8),capacity=read<int32_t>(held_tasks+0xdc);
    if(count<0||capacity<count||capacity>1000000)reject_queue("Snapshot native callback snapshot bounds differ actual="+
        std::to_string(count)+" capacity="+std::to_string(capacity));
    if(count!=0&&!loading_dispatch::save_completion::consumed_snapshot(held_tasks)&&
        !loading_dispatch::completed_callbacks::matches(held_tasks))reject_queue(
        "Snapshot callback snapshot lacks completed native drain or original Save receipt actual="+std::to_string(count)+" required=0 or exact completed drain/writer");
    require(!loading_dispatch::pending&&!loading_dispatch::uncertain,"Snapshot has deferred or uncertain task callbacks");
}
void same(const Request& request){
    require(request.lease==certificate.lease&&request.session==certificate.session&&request.world_epoch==certificate.world_epoch&&
        request.tick==certificate.tick&&request.order==certificate.order&&request.peer_mask==certificate.peer_mask&&
        std::memcmp(request.nonce,certificate.nonce,sizeof(request.nonce))==0,"Snapshot certificate identity differs");
    require(!request.invocation,"Snapshot held service must not borrow a boundary invocation");
    require(!control.entered&&!timer_step::frame.active&&control.phase==Control::Phase::Held,"Snapshot service is not at the prepared hold");
    require(mode()==prepared_mode&&held_tasks==prepared_tasks,"Snapshot prepared native owners changed");
    const auto current=native_steps->snapshot();
    require(current.lease==certificate.lease&&current.session==certificate.session&&current.world_epoch==certificate.world_epoch&&
        current.completed_tick==certificate.tick&&current.phase==MT2FW_MT2_STEP_HELD&&current.workers_held==1,
        "Snapshot prepared completed tick changed; no capture");
    empty_tasks();require(read<uint32_t>(clock()+0x5c)==0,"Snapshot prepared pause scale changed");
}
void before_step(){
    require(phase==Phase::Idle||phase==Phase::Released||phase==Phase::Armed,"Snapshot holds the next original Go until acknowledged release");
    if(phase==Phase::Armed){const auto current=native_steps->snapshot();
        require(current.tick==certificate.tick&&current.invocation==certificate.invocation&&current.lease==certificate.lease,
            "Snapshot armed Go differs from its exact borrowed boundary");}
    loading_dispatch::save_completion::clear_consumed();
}
void prepare_native(const std::function<void(void*)>& finish=finish_all);
void after_go(){
    if(phase!=Phase::Armed)return;
    // The normal Go and its existing task drain have returned. Native Save's
    // force-update intentionally consumes accumulated Region/Toon time.
    require(control.entered&&timer_step::frame.active&&!held_tasks&&control.phase==Control::Phase::Stepping,
        "Snapshot preparation lacks the completed original Go scope");
    const auto current=native_steps->snapshot();
    require(current.tick==certificate.tick&&current.invocation==certificate.invocation&&current.lease==certificate.lease,
        "Snapshot preparation original tick differs");
    prepare_native();
}
void prepare_native(const std::function<void(void*)>& finish){
    const auto active=mode();const auto native_clock=clock();
    const auto shards=read<uintptr_t>(active+0x30);
    require(shards&&read<uintptr_t>(shards)==image+0x1702b00,"Snapshot ShardManager original owner differs");
    const auto tasks_now=tasks();
    require(read<uint8_t>(tasks_now+0xf8)==0&&finish_all&&all_done,"Snapshot preparation scheduler is not released");
    const auto context=SDL_GL_GetCurrentContext();const auto window=SDL_GL_GetCurrentWindow();
    require(context&&window,"Snapshot preparation has no original GL context/window");
    loading_dispatch::completed_callbacks::clear();
    phase=Phase::Failed;++flush_attempts;
    loading_dispatch::StepDrain drain;
    original_flush(reinterpret_cast<void*>(shards));++flush_returns;
    set_pause(reinterpret_cast<void*>(native_clock),0.0f);
    finish(reinterpret_cast<void*>(tasks_now));
    require(all_done(reinterpret_cast<void*>(tasks_now))&&read<int32_t>(tasks_now+0xc0)==0,
        "Snapshot original preparation left unfinished task work; no repeated drain");
    require(SDL_GL_GetCurrentContext()==context&&SDL_GL_GetCurrentWindow()==window,"Snapshot preparation changed original GL ownership");
    require(mode()==active&&clock()==native_clock&&read<uint32_t>(native_clock+0x5c)==0,"Snapshot original preparation owner/Clock readback differs");
    loading_dispatch::completed_callbacks::record(tasks_now);
    prepared_mode=active;prepared_tasks=tasks_now;phase=Phase::Prepared;
}
void capture_owner(){
    if(recovery_owner.lease){
        const auto* recovery=native_steps->recovery_snapshot();
        require(recovery&&recovery->phase==MT2FW_MT2_RECOVERY_CONSUMED&&recovery->capture_attempted==1&&
            recovery->lease==recovery_owner.lease&&recovery->interruption==recovery_owner.interruption&&
            recovery->generation==recovery_owner.generation&&std::memcmp(recovery->recovery_nonce,recovery_owner.nonce,37)==0&&
            certificate.lease==recovery->lease&&certificate.session==recovery->session&&certificate.world_epoch==recovery->world_epoch&&
            certificate.tick==recovery->completed_tick&&std::memcmp(certificate.nonce,recovery->recovery_nonce,37)==0&&
            std::memcmp(certificate.witness_sha256,recovery->witness_sha256,32)==0,
            "Recovery Save scope lacks its exact independently consumed certificate");
    }
    const auto* prepared=mt2::PreparedSaveScope::current;
    require(prepared&&phase==Phase::Consumed&&prepared->mode==prepared_mode&&prepared->tasks==prepared_tasks&&
        prepared->thread==thread&&thread==GetCurrentThreadId(),"Snapshot capture lacks its single-use native owner");
    require(!control.entered&&!timer_step::frame.active&&held_tasks==prepared_tasks&&control.phase==Control::Phase::Held,
        "Snapshot capture escaped the prepared held service");
    held();
}
void settle_cache(){
    if(!mt2::PreparedSaveScope::current)return;
    capture_owner();
    loading_dispatch::save_cache::settle(original_resume,original_pause,cache_finish);
    empty_tasks();
    runtime->log("steps","info","Snapshot original Save cache settled order="+std::to_string(certificate.order)+
        " tick="+std::to_string(certificate.tick)+" attempted="+std::to_string(loading_dispatch::save_cache::attempts)+
        " returned="+std::to_string(loading_dispatch::save_cache::returns));
}
void flush_call(void* receiver,uintptr_t caller){
    if(!mt2::PreparedSaveScope::current||caller!=image+0x4a0435){original_flush(receiver);return;}
    capture_owner();auto& prepared=*mt2::PreparedSaveScope::current;
    const auto* blocking=mt2::BlockingSaveScope::current;
    require(blocking&&!blocking->previous&&blocking->mode==prepared_mode&&blocking->thread==thread,
        "Snapshot repeated flush lacks exact original blocking Save ownership");
    require(read<int32_t>(prepared_mode+0x30c)==0&&read<uintptr_t>(prepared_mode+0x30)==reinterpret_cast<uintptr_t>(receiver)&&
        read<uintptr_t>(reinterpret_cast<uintptr_t>(receiver))==image+0x1702b00,"Snapshot repeated flush native receiver or save count differs");
    require(prepared.saves==0&&prepared.flushes==0,"Snapshot capture attempted more than one original Save");
    ++prepared.saves;++prepared.flushes;
}
__declspec(noinline) void flush_entry(void* receiver){flush_call(receiver,reinterpret_cast<uintptr_t>(_ReturnAddress()));}
bool preserve_resume(void* receiver,uintptr_t caller){
    if(!mt2::PreparedSaveScope::current||caller!=image+0x49f7c9)return false;
    capture_owner();auto& prepared=*mt2::PreparedSaveScope::current;
    require(prepared.saves==1,"Snapshot original Save Resume capture saves differs actual="+std::to_string(prepared.saves)+" required=1");
    require(prepared.flushes==1,"Snapshot original Save Resume prepared flush count differs actual="+std::to_string(prepared.flushes)+" required=1");
    require(prepared.resumes==0,"Snapshot original Save Resume repeated completion actual="+std::to_string(prepared.resumes)+" required=0");
    const auto actual_tasks=reinterpret_cast<uintptr_t>(receiver);
    require(actual_tasks==prepared_tasks,"Snapshot original Save Resume Task receiver differs actual="+std::to_string(actual_tasks)+
        " required="+std::to_string(prepared_tasks));
    const auto saves=read<int32_t>(prepared_mode+0x30c);
    require(saves==0,"Snapshot original Save Resume native save count differs actual="+std::to_string(saves)+" required=0");
    if(!async_save::resume())empty_tasks();++prepared.resumes;return true;
}
void operation(const Request& request,MT2FW_MT2_SnapshotCaptureFn callback,void* user){
    require(!recovery_owner.lease,"Ordinary snapshot operation cannot use a Recovery certificate");
    shape(request);
    if(request.operation==MT2FW_MT2_SNAPSHOT_ARM){
        require(!callback&&!user&&(phase==Phase::Idle||phase==Phase::Released)&&request.order>last_order&&request.invocation&&
            !request.ack_mask&&empty_hash(request),"Snapshot ARM is stale, repeated, or carries premature agreement");
        require(!control.entered&&!timer_step::frame.active&&control.phase==Control::Phase::Held,"Snapshot ARM requires held ordered command");
        mode();clock();empty_tasks();certificate=request;last_order=request.order;capture_returned=false;client_cache_attempted=client_cache_returned=false;phase=Phase::Armed;return;
    }
    if(request.operation==MT2FW_MT2_SNAPSHOT_PUMP){
        require(!callback&&!user&&phase==Phase::Consumed&&request.lease==certificate.lease&&request.session==certificate.session&&
            request.world_epoch==certificate.world_epoch&&request.tick==certificate.tick&&request.order==certificate.order&&
            request.peer_mask==certificate.peer_mask&&request.ack_mask==certificate.peer_mask&&!request.invocation&&
            std::memcmp(request.nonce,certificate.nonce,sizeof(request.nonce))==0&&
            std::equal(std::begin(certificate.witness_sha256),std::end(certificate.witness_sha256),request.witness_sha256),
            "Snapshot PUMP lacks its consumed exact capture certificate");
        const auto receipt=native_steps->snapshot();
        require(receipt.lease==certificate.lease&&receipt.session==certificate.session&&receipt.world_epoch==certificate.world_epoch&&
            receipt.completed_tick==certificate.tick&&receipt.actual_role==MT2FW_MT2_SESSION_HOST&&receipt.workers_held==1&&
            (receipt.phase==MT2FW_MT2_STEP_HELD||receipt.phase==MT2FW_MT2_STEP_FAILED),"Snapshot PUMP current native owner differs");
        if(async_save::pump()&&receipt.phase==MT2FW_MT2_STEP_HELD)capture_returned=true;
        return;
    }
    same(request);
    if(request.operation==MT2FW_MT2_SNAPSHOT_SEAL){
        require(!callback&&!user&&phase==Phase::Prepared&&!request.ack_mask&&!empty_hash(request),"Snapshot SEAL lacks one completed preparation and local witness");
        std::copy(std::begin(request.witness_sha256),std::end(request.witness_sha256),std::begin(certificate.witness_sha256));phase=Phase::Sealed;return;
    }
    require(request.ack_mask==certificate.peer_mask&&std::equal(std::begin(certificate.witness_sha256),std::end(certificate.witness_sha256),request.witness_sha256),
        "Snapshot peer acknowledgment mask or canonical covered witness differs");
    if(request.operation==MT2FW_MT2_SNAPSHOT_CLIENT_CACHE){
        require(!callback&&!user&&phase==Phase::Sealed&&!client_cache_attempted&&!client_cache_returned&&
            native_steps->snapshot().actual_role==MT2FW_MT2_SESSION_CLIENT,
            "Client Save cache requires its unused sealed publication certificate");
        require(read<uintptr_t>(image+loading_dispatch::save_cache::GlobalTask)==0,
            "Client Save cache already has native work; no duplicate rebuild");
        client_cache_attempted=true;
        loading_dispatch::save_cache::ExportScope scope(prepared_mode,prepared_tasks,thread);
        reinterpret_cast<void(*)()>(image+0x7a1560)();
        loading_dispatch::save_cache::settle(original_resume,original_pause,cache_finish);
        same(request);client_cache_returned=true;
        runtime->log("steps","info","Client Save publication cache completed order="+std::to_string(certificate.order)+
            " tick="+std::to_string(certificate.tick));return;
    }
    if(request.operation==MT2FW_MT2_SNAPSHOT_RELEASE){
        const auto role=native_steps->snapshot().actual_role;
        require(!callback&&!user&&((role==MT2FW_MT2_SESSION_HOST&&phase==Phase::Consumed&&capture_returned)||
            (role==MT2FW_MT2_SESSION_CLIENT&&phase==Phase::Sealed&&(!client_cache_attempted||client_cache_returned))),"Snapshot release lacks completed Host capture or sealed Client");
        async_save::retire();phase=Phase::Released;return;
    }
    require(request.operation==MT2FW_MT2_SNAPSHOT_CAPTURE&&callback&&phase==Phase::Sealed&&
        native_steps->snapshot().actual_role==MT2FW_MT2_SESSION_HOST,"Snapshot capture requires unconsumed Host certificate");
    phase=Phase::Consumed;++captures;
    mt2::PreparedSaveScope scope(prepared_mode,prepared_tasks,thread);
    char cause[1024]{};
    MT2FW_MT2_Result result=MT2FW_MT2_FAILED;
    try{result=callback(user,cause,sizeof(cause));}
    catch(...){if(scope.asynchronous)async_save::retain_scope(scope);throw;}
    require(std::memchr(cause,0,sizeof(cause))!=nullptr,"Snapshot callback cause is not terminated");
    if(scope.asynchronous)async_save::retain_scope(scope);
    require(result==MT2FW_MT2_OK,"Snapshot capture callback failed result="+std::to_string(result)+" cause="+cause);
    if(scope.asynchronous){
        require(async_save::started()&&scope.saves==1&&scope.flushes==1&&scope.resumes<=1,"Snapshot callback did not start exactly one original async Save");return;
    }
    require(scope.saves==1&&scope.flushes==1&&scope.resumes==1,"Snapshot callback did not complete exactly one original blocking Save");
    same(request);capture_returned=true;
}
void reset(){
    require(!mt2::PreparedSaveScope::current,"Snapshot reset during native capture");
    loading_dispatch::save_completion::clear_consumed();loading_dispatch::completed_callbacks::clear();
    async_save::retire();phase=Phase::Idle;certificate={};recovery_owner={};last_order=0;capture_returned=false;client_cache_attempted=client_cache_returned=false;prepared_mode=prepared_tasks=0;
}
void install(){
    if(installed)return;
    for(const auto& [offset,size,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,5>{{
        {0x4a1b40,112,"c80fa0bc8b35c00fab500b78432fd2d09b3ba3075e3b09ac79c1851c3d58017f"},
        {0x4a03a0,240,"c6d3f9d5af47b7fe217d9c22a65ccc584ef802fd11ffbec2172dc9c132e2e29d"},
        {0x509a50,80,"f894bf9c1351b89ccda495a4d7b68b08969beef8e75371ccdb3f64a038aa285a"},
        {0x259b70,176,"e04b7a6083ecc1404c416b50fd098114afd5a6f69d270f8190f0f24cda619efd"},
        {0x49f6f0,352,"1d6dbe7daea4a371deb7e108f1284a0840c5f5d3cf3bcd925c1a2618b7c5ef5f"}
    }}){
        std::string cause;const bool valid=native_routes->verify_body(offset,size,sha,cause);
        require(valid,"Snapshot original body pin differs rva="+std::to_string(offset)+" bytes="+std::to_string(size)+": "+cause);
    }
    native_routes->catalog("framework.save.prepare-flush",0x509a50,true);native_services->reserve("framework.save.prepare-flush");
    const auto target=reinterpret_cast<void*>(image+0x509a50);
    const auto detour=reinterpret_cast<void*>(&flush_entry);
    hook(target,detour,reinterpret_cast<void**>(&original_flush));
    // Configuration runs after startup's one-time activation and publication.
    const auto enabled=MH_EnableHook(target);
    require(enabled==MH_OK,std::string("Snapshot prepare flush hook enable failed: ")+MH_StatusToString(enabled));
    std::string cause;
    const bool published=native_routes->publish_framework_hook(target,detour,reinterpret_cast<void*>(original_flush),cause);
    require(published,"Snapshot prepare flush hook publication failed: "+cause);
    {std::string cause;const bool valid=native_routes->verify_body(0x7a1560,128,"4c368f3bc7df3f53042d01ee8864e301b1b2e664b80e13c29c0c4322b3bc48f7",cause);
        require(valid,"Save cache native body pin differs rva=0x7a1560: "+cause);}
    {std::string cause;const bool valid=native_routes->verify_body(0x7a2780,15024,"946864dffc3375187db8cede00297d5f82f97763385df383dcb582b05fd8c0f0",cause);
        require(valid,"Save cache native body pin differs rva=0x7a2780: "+cause);}
    {std::string cause;const bool valid=native_routes->verify_body(0xc367a0,16,"499f1f307c1cb989f968a6b7fcaec591e1828877223d0b0e7e8e8b76cde8c9ca",cause);
        require(valid,"Save cache native body pin differs rva=0xc367a0: "+cause);}
    {std::string cause;const bool valid=native_routes->verify_body(0x526d50,480,"666f4cac19d8d04df16bdff53afbcc87899ad708f157aeae5674cf284a4540fa",cause);
        require(valid,"Save cache native body pin differs rva=0x526d50: "+cause);}
    {std::string cause;const bool valid=native_routes->verify_body(0x526b00,432,"12ca52a501a074d6459b697bf9df2d8c4a7322bed76270b0cfe9dc33b91bd016",cause);
        require(valid,"Save cache native body pin differs rva=0x526b00: "+cause);}
    cache_finish=reinterpret_cast<Go>(image+0x526d50);
    set_pause=reinterpret_cast<ClockSet>(image+0x259b70);installed=true;
}
}
