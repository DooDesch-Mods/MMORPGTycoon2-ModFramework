// Read-only first-step evidence for the C22 differing Subscriber identities.
namespace motion_probe {
using Update=void(*)(void*,float);
Update original_map_entity=nullptr,original_travel=nullptr;
bool enabled=false;
Json events=Json::array();
std::string first_error;
std::mutex evidence_gate;
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
bool tracing(){return enabled&&control.phase==mt2::determinism::Gate::Phase::Stepping&&control.completed==0;}
size_t append(Json entry){std::lock_guard lock(evidence_gate);require(events.size()<256,"Motion probe first-step receipt limit reached");const auto index=events.size();events.push_back(std::move(entry));return index;}
void returned(size_t index){std::lock_guard lock(evidence_gate);events.at(index)["original_returned"]=true;}
void after(size_t index,Json state){std::lock_guard lock(evidence_gate);events.at(index)["after"]=std::move(state);}
void failure(const char* cause){std::lock_guard lock(evidence_gate);if(first_error.empty())first_error=cause;}
int32_t subscriber_id(uintptr_t toon){
    if(!toon||read<uintptr_t>(toon)!=image+0x1744e90)return -1;
    const auto subscriber=read<uintptr_t>(toon+0x940);
    require(subscriber&&read<uintptr_t>(subscriber)==image+0x16e75f0&&read<uintptr_t>(subscriber+0x80)==toon,"Motion probe Subscriber/Toon ownership differs");
    const auto id=read<int32_t>(subscriber+0x20);require(id>=0,"Motion probe negative Subscriber ID");return id;
}
Json words(uintptr_t receiver,std::initializer_list<uintptr_t> offsets){Json out=Json::object();for(auto offset:offsets)out[std::to_string(offset)]=read<uint32_t>(receiver+offset);return out;}
Json sample(uintptr_t toon){
    const auto id=subscriber_id(toon);require(id>=0,"Motion probe target is not a Subscriber Toon");
    const auto region=read<uintptr_t>(toon+0x178);
    require(region&&read<uintptr_t>(region)==image+0x1787930,"Motion probe Region address point differs");
    const auto manager=read<uintptr_t>(region+0x9c0);
    require(manager&&read<uintptr_t>(manager)==image+0x16fde30,"Motion probe ToonManager address point differs");
    Json flags=Json::object();for(uintptr_t offset:{0x268,0x269,0x26a,0x26b,0x26c,0x310,0x360,0x46c,0x478,0x479,0x47b,0x47c,0x47d,0x47e,0x47f,0x480})flags[std::to_string(offset)]=read<uint8_t>(toon+offset);
    const auto path_task=read<uintptr_t>(toon+0x308);
    Json task={{"present",path_task!=0}};if(path_task){task["type"]=read<int32_t>(path_task+8);task["cancelled"]=read<uint8_t>(path_task+0xc);task["orphaned"]=read<uint8_t>(path_task+0xd);task["completed"]=read<uint8_t>(path_task+0xe);}
    return {{"saved_id",id},{"region",read<int32_t>(region+0x834)},
        {"pose",words(toon,{0x100,0x104,0x108,0x10c,0x110,0x114,0x118})},
        {"motion",words(toon,{0x230,0x234,0x238,0x23c,0x244,0x248,0x24c,0x250,0x254,0x258,0x260,0x264,0x270,0x2b0,0x2b4,0xad4})},
        {"manager",words(manager,{0xbc,0xc0,0xc4})},{"flags",flags},{"path_node_present",read<uintptr_t>(toon+0x2e8)!=0},{"path_task",task}};
}
Json task_owner(uintptr_t task){
    require(task&&read<uintptr_t>(task)==image+0x17037f0,"Motion probe deferred task type differs");
    const auto toon=read<uintptr_t>(task+0x10),subscriber=read<uintptr_t>(task+0x20);
    const auto id=subscriber_id(toon);
    require(id>=0&&read<uintptr_t>(toon+0x940)==subscriber,"Motion probe task owner differs");
    return {{"saved_id",id},{"task",task},{"toon",toon},{"subscriber",subscriber},
        {"cancelled",read<uint8_t>(task+0xc)},{"orphaned",read<uint8_t>(task+0xd)},{"completed",read<uint8_t>(task+0xe)}};
}
size_t finish_before(uintptr_t task){
    if(!tracing())return SIZE_MAX;
    require(thread==GetCurrentThreadId()&&timer_step::frame.active,"Motion probe Finish lacks admitted game thread");
    Json entry={{"kind","plan_finish"},{"frame",timer_step::frame.entered},{"owner",task_owner(task)},{"original_returned",false}};
    const auto toon=read<uintptr_t>(task+0x10);const auto id=subscriber_id(toon);
    if(id==46||id==56)entry["before"]=sample(toon);
    return append(std::move(entry));
}
void finish_after(size_t index,uintptr_t task){
    if(index==SIZE_MAX)return;
    int32_t id=-1;{std::lock_guard lock(evidence_gate);events.at(index)["original_returned"]=true;id=events.at(index).at("owner").at("saved_id").get<int32_t>();}
    if(id==46||id==56)after(index,sample(read<uintptr_t>(task+0x10)));
}
void update_call(void* receiver,float dt,uintptr_t caller,bool travel){
    auto original=travel?original_travel:original_map_entity;
    if(!tracing()){original(receiver,dt);return;}
    try{
        const auto toon=reinterpret_cast<uintptr_t>(receiver);const auto id=subscriber_id(toon);
        if(id!=46&&id!=56){original(receiver,dt);return;}
        require(control.entered,"Motion probe Update lacks admitted outer scope");
        const auto index=append({{"kind",travel?"travel":"map_entity"},{"saved_id",id},{"step",control.completed+1},
            {"main_thread",thread==GetCurrentThreadId()},{"thread_id",GetCurrentThreadId()},
            {"caller_rva",rva(caller)},{"dt_bits",bits(dt)},{"dt_stage",travel?"TravelUpdate entry":"MapEntity.EntityUpdate entry before entity+0x270 scaling"},{"before",sample(toon)},{"original_returned",false}});
        original(receiver,dt);returned(index);after(index,sample(toon));
    }catch(const std::exception& e){failure(e.what());throw;}
    catch(...){failure("Motion probe original Update/read raised; no replay");throw;}
}
__declspec(noinline) void map_entity(void* receiver,float dt){update_call(receiver,dt,reinterpret_cast<uintptr_t>(_ReturnAddress()),false);}
__declspec(noinline) void travel(void* receiver,float dt){update_call(receiver,dt,reinterpret_cast<uintptr_t>(_ReturnAddress()),true);}
Json snapshot(const Json& subscribers){
    if(!enabled)return nullptr;
    Json rows=Json::array();for(const auto& value:subscribers){const auto subscriber=value.get<uintptr_t>();const auto id=read<int32_t>(subscriber+0x20);
        if(id==46||id==56){const auto toon=read<uintptr_t>(subscriber+0x80);require(subscriber_id(toon)==id,"Motion probe snapshot owner differs");rows.push_back(sample(toon));}}
    require(rows.size()==2,"Motion probe snapshot lacks both retained divergent Subscriber IDs");
    const auto tasks=read<uintptr_t>(image+0x13df340);
    require(held_tasks==tasks&&tasks&&read<uint8_t>(tasks+0xf8)==1,"Motion probe snapshot requires held workers");
    const auto count=read<int32_t>(tasks+0xc0);const auto data=read<uintptr_t>(tasks+0xb8);
    require(count>=0&&count<=256&&read<int32_t>(tasks+0xc4)>=count&&(!count||data),"Motion probe finished queue bounds differ");
    Json queue=Json::array();for(int32_t i=0;i<count;++i){const auto task=read<uintptr_t>(data+size_t(i)*8);
        if(read<uintptr_t>(task)==image+0x17037f0)queue.push_back(task_owner(task));
        else queue.push_back({{"task",task},{"class_rva",rva(read<uintptr_t>(task))}});}
    return {{"rows",rows},{"finished_queue",queue}};
}
Json status(){std::lock_guard lock(evidence_gate);return {{"enabled",enabled},{"events",events},{"first_error",first_error}};}
void install(const Json& config){
    enabled=config.value("motion_probe",false);if(!enabled)return;
    require(config.value("defer_loading_plans",false),"Motion probe requires deferred loading plans");
    for(const auto& [offset,size,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,2>{{
        {0x3b8cb0,656,"185345c94b7929f1a8c53058c1d8bc6d7738cea2cb0d61bcd0de0ae6195777c1"},
        {0x3b78b0,5120,"5227570ec1fc062edf107ffbb456d83234e4fba1e5a70e270e2e107fc2d58500"}}}){
        std::string body(size,'\0');SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+offset),body.data(),size,&copied)||copied!=size||digest(body)!=sha)
            throw std::runtime_error("Motion probe exact native body differs");
    }
    hook(reinterpret_cast<void*>(image+0x3b8cb0),reinterpret_cast<void*>(&map_entity),reinterpret_cast<void**>(&original_map_entity));
    hook(reinterpret_cast<void*>(image+0x3b78b0),reinterpret_cast<void*>(&travel),reinterpret_cast<void**>(&original_travel));
}
}
