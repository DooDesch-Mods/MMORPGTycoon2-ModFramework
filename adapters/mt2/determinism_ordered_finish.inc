// Included inside loading_dispatch after the exact ToonPlan classifier.
namespace ordered_finish {
bool enabled=false;
uint64_t batches=0,admitted=0,moved_slots=0,stale_visits=0,last_frame=0;
uint64_t order_hash=14695981039346656037ull;
std::vector<int32_t> first_ids,last_ids;
std::map<int32_t,uintptr_t> subscribers(){
    const auto manager=read<uintptr_t>(image+0x13df690);
    require(manager&&read<uintptr_t>(manager)==image+0x1725f40,"Ordered Finish Subscriber manager differs");
    const auto data=read<uintptr_t>(manager+0x40);const auto count=read<int32_t>(manager+0x48),capacity=read<int32_t>(manager+0x4c),next_id=read<int32_t>(manager+0x68);
    require(count>=0&&capacity>=count&&count<=1000000&&next_id>=0&&(!count||data),"Ordered Finish Subscriber array bounds differ");
    std::map<int32_t,uintptr_t> result;
    for(int32_t i=0;i<count;++i){const auto subscriber=read<uintptr_t>(data+size_t(i)*8);
        require(subscriber&&read<uintptr_t>(subscriber)==image+0x16e75f0,"Ordered Finish Subscriber address point differs");
        const auto id=read<int32_t>(subscriber+0x20);
        require(id>=0&&id<next_id&&result.emplace(id,subscriber).second,"Ordered Finish saved Subscriber ID is invalid or duplicated");}
    return result;
}
void mix(uint64_t value){for(unsigned i=0;i<8;++i){order_hash^=uint8_t(value>>(i*8));order_hash*=1099511628211ull;}}
void order(uintptr_t tasks){
    if(!enabled)return;
    require(control.phase==mt2::determinism::Gate::Phase::Stepping,"Ordered Finish is outside an admitted step");
    const auto count=read<int32_t>(tasks+0xd8),capacity=read<int32_t>(tasks+0xdc);const auto data=read<uintptr_t>(tasks+0xd0);
    require(count>=0&&capacity==count&&(!count||data)&&read<int32_t>(tasks+0xc0)==0,"Ordered Finish native snapshot bounds differ");
    if(!count)return;
    struct Entry{int32_t id;uintptr_t task;};
    std::vector<Entry> current;std::vector<int32_t> slots;std::set<uintptr_t> unique_tasks;std::set<int32_t> unique_ids;
    std::map<int32_t,uintptr_t> owners;bool owners_read=false;uint64_t stale=0;
    for(int32_t i=0;i<count;++i){const auto task=read<uintptr_t>(data+size_t(i)*8);
        require(unique_tasks.insert(task).second,"Ordered Finish duplicate native task membership");
        if(!plan(task))continue;
        const auto subscriber=read<uintptr_t>(task+0x20);
        require(subscriber&&read<uintptr_t>(subscriber)==image+0x16e75f0,"Ordered Finish task Subscriber differs");
        const auto brain=read<uintptr_t>(subscriber+0x88);
        require(brain&&read<uintptr_t>(brain)==image+0x16fdd60&&read<uintptr_t>(brain+0x28)==subscriber,"Ordered Finish reciprocal Brain differs");
        // DeliverPlanResult takes its mismatch branch before any plan action.
        // Retired tasks retain their original native slots and cleanup order.
        if(read<uintptr_t>(brain+0x20)!=task){++stale;continue;}
        if(!owners_read){owners=subscribers();owners_read=true;}
        const auto id=read<int32_t>(subscriber+0x20);const auto found=owners.find(id);
        require(found!=owners.end()&&found->second==subscriber,"Ordered Finish current task lacks registered saved identity");
        const auto toon=read<uintptr_t>(task+0x10);
        require(toon&&read<uintptr_t>(toon)==image+0x1744e90&&read<uintptr_t>(toon+0x940)==subscriber&&read<uintptr_t>(subscriber+0x80)==toon&&read<uintptr_t>(brain+0x18)==toon,
            "Ordered Finish current task reciprocal Toon differs");
        require(unique_ids.insert(id).second,"Ordered Finish multiple current tasks share saved Subscriber ID");
        current.push_back({id,task});slots.push_back(i);
    }
    std::sort(current.begin(),current.end(),[](const Entry& a,const Entry& b){return a.id<b.id;});
    std::vector<int32_t> ids;ids.reserve(current.size());uint64_t moved=0;
    for(size_t i=0;i<current.size();++i){ids.push_back(current[i].id);moved+=read<uintptr_t>(data+size_t(slots[i])*8)!=current[i].task;}
    // Publish only a permutation under the original Pump mutex. Every task
    // remains within its original owned scratch count and original callback loop.
    if(moved){uncertain=true;for(size_t i=0;i<current.size();++i)write(data+size_t(slots[i])*8,current[i].task);uncertain=false;}
    stale_visits+=stale;
    if(!current.empty()){
        if(!batches)first_ids=ids;last_ids=std::move(ids);++batches;admitted+=current.size();moved_slots+=moved;last_frame=timer_step::frame.entered;
        mix(control.completed);mix(current.size());for(const auto& entry:current)mix(uint64_t(entry.id));
    }
}
Json status(){return {{"enabled",enabled},{"batches",batches},{"admitted",admitted},{"moved_slots",moved_slots},{"stale_visits",stale_visits},
    {"first_ids",first_ids},{"last_ids",last_ids},{"last_frame",last_frame},{"order_hash",order_hash}};}
void install(const Json& config){
    enabled=config.value("defer_loading_plans",false);if(!enabled)return;
    for(const auto& [offset,size,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,2>{{
        {0x391580,848,"841092772ae0a3c5bceb9b582c0fd3ab2851e2d56015b2bf2e74bf819302cae5"},
        {0x3918d0,240,"d0b7bc735e0738e216fe9e9695c1852710a2076e3702584594267af94d7474a2"}}}){
        std::string body(size,'\0');SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+offset),body.data(),size,&copied)||copied!=size||digest(body)!=sha)
            throw std::runtime_error("Ordered Finish exact native admission/delivery body differs");
    }
}
}
