// Included inside loading_dispatch. Every field below is protected by native +f0.
namespace ordered_run {
Unlock original_lock=nullptr,post=nullptr;
std::atomic<uintptr_t> bound{0};
bool open=false,masked=false,transfer_uncertain=false;
uintptr_t running=0,view_data=0;
int32_t exposed=0;
std::vector<uintptr_t> retained,eligible,batch;
size_t cursor=0;
uint64_t batches=0,admitted=0,completed=0,blocked=0;
constexpr uintptr_t RetrieveLock=0x528d67,RetrieveUnlock=0x528d9b,PausedUnlock=0x528ed2;
constexpr uintptr_t RetrieveUnwind=0x5291eb,CompleteUnlock=0x52709c;
struct Guard {
    void* mutex;
    explicit Guard(uintptr_t tasks):mutex(reinterpret_cast<void*>(tasks+0xf0)){original_lock(mutex);}
    ~Guard(){original_unlock(mutex);}
    Guard(const Guard&)=delete;Guard& operator=(const Guard&)=delete;
};
std::vector<uintptr_t> entries(uintptr_t tasks,size_t offset){
    const auto data=read<uintptr_t>(tasks+offset);
    const auto count=read<int32_t>(tasks+offset+8),capacity=read<int32_t>(tasks+offset+12);
    require(count>=0&&capacity>=count&&count<=1000000&&(!count||data),"Ordered Run native queue bounds differ");
    std::vector<uintptr_t> out;out.reserve(size_t(count));std::set<uintptr_t> unique;
    for(int32_t i=0;i<count;++i){const auto task=read<uintptr_t>(data+size_t(i)*8);
        require(task&&unique.insert(task).second,"Ordered Run duplicate or absent task ownership");out.push_back(task);}
    return out;
}
bool contains(const std::vector<uintptr_t>& values,uintptr_t value){return std::find(values.begin(),values.end(),value)!=values.end();}
bool is_plan(uintptr_t task){return read<uintptr_t>(task)==image+Plan;}
void arm(uintptr_t tasks){
    if(!enabled)return;
    require(thread==GetCurrentThreadId()&&tasks&&read<uint8_t>(tasks+0xf8)==1,"Ordered Run arm requires held owner thread");
    Guard guard(tasks);require(!bound.load()&&!open&&!masked&&!running&&!transfer_uncertain,"Ordered Run admission already armed or uncertain");
    for(const auto task:entries(tasks,0xa0))require(!is_plan(task),"Ordered Run arm found a running ToonPlan");
    bound.store(tasks,std::memory_order_release);
}
void cancel(){
    const auto tasks=bound.load(std::memory_order_acquire);if(!tasks)return;
    require(thread==GetCurrentThreadId(),"Ordered Run cancel changed owner thread");
    Guard guard(tasks);require(!masked&&!open&&!running&&!transfer_uncertain,"Ordered Run cannot cancel active or uncertain native admission");
    const auto pending=entries(tasks,0x58);
    bound.store(0,std::memory_order_release);batch.clear();cursor=0;
    // Closed retrieval consumed original queue tokens. Restore wakeups before
    // native teardown resumes its workers and takes ownership of all work.
    for(size_t i=0;i<pending.size();++i)post(reinterpret_cast<void*>(tasks+0xe0));
}
void begin(uintptr_t tasks){
    if(!enabled)return;
    require(manager()==tasks&&bound.load(std::memory_order_acquire)==tasks,"Ordered Run drain owner differs");
    Guard guard(tasks);require(!open&&!masked&&!running&&!transfer_uncertain,"Ordered Run drain reentered or retained an active/uncertain plan");
    const auto pending=entries(tasks,0x58);std::map<int32_t,uintptr_t> owners;bool owners_read=false;
    std::vector<std::pair<int32_t,uintptr_t>> ordered;std::set<int32_t> ids;
    for(const auto task:pending){
        if(!is_plan(task)||read<uint8_t>(task+0xc))continue;
        if(!owners_read){owners=ordered_finish::subscribers();owners_read=true;}
        require(plan(task),"Ordered Run ToonPlan classification differs");
        const auto subscriber=read<uintptr_t>(task+0x20),toon=read<uintptr_t>(task+0x10);
        require(subscriber&&read<uintptr_t>(subscriber)==image+0x16e75f0,"Ordered Run Subscriber differs");
        const auto id=read<int32_t>(subscriber+0x20);const auto found=owners.find(id);
        require(found!=owners.end()&&found->second==subscriber&&ids.insert(id).second,"Ordered Run saved identity is absent or duplicated");
        const auto brain=read<uintptr_t>(subscriber+0x88);
        require(brain&&read<uintptr_t>(brain)==image+0x16fdd60&&read<uintptr_t>(brain+0x20)==task&&read<uintptr_t>(brain+0x28)==subscriber&&
            toon&&read<uintptr_t>(toon)==image+0x1744e90&&read<uintptr_t>(toon+0x940)==subscriber&&read<uintptr_t>(subscriber+0x80)==toon&&read<uintptr_t>(brain+0x18)==toon,
            "Ordered Run reciprocal current task/Toon differs");
        ordered.emplace_back(id,task);
    }
    std::sort(ordered.begin(),ordered.end());std::vector<uintptr_t> next;next.reserve(ordered.size());
    for(const auto& [id,task]:ordered)next.push_back(task);
    batch=std::move(next);cursor=0;open=true;if(!batch.empty()){++batches;post(reinterpret_cast<void*>(tasks+0xe0));}
}
void end(uintptr_t tasks){
    if(!enabled)return;
    Guard guard(tasks);require(open&&!masked&&!running&&!transfer_uncertain,"Ordered Run drain returned with an active/uncertain native Run");
    // Native Finish may already have queued Pathfinder children. Their existing
    // bounded settlement remains responsible for those non-ToonPlan entries.
    for(const auto offset:{size_t(0x58),size_t(0xa0)})
        for(const auto task:entries(tasks,offset))require(!is_plan(task),"Ordered Run drain returned with pending/running ToonPlans");
    open=false;batch.clear();cursor=0;
}
void select(uintptr_t tasks){
    require(!masked,"Ordered Run pending view reentered");
    const auto pending=entries(tasks,0x58);
    while(open&&cursor<batch.size()&&!contains(pending,batch[cursor]))++cursor;
    const auto next=open&&!running&&cursor<batch.size()?batch[cursor]:0;
    std::vector<uintptr_t> keep,hold;
    for(const auto task:pending){
        if(!is_plan(task)||read<uint8_t>(task+0xc)){keep.push_back(task);continue;}
        require(read<int32_t>(task+8)==2&&read<uint8_t>(task+0xe)==0,"Ordered Run pending ToonPlan layout differs");
        require(!open||contains(batch,task),"Ordered Run new ToonPlan arrived during frozen native drain");
        (task==next?keep:hold).push_back(task);
    }
    if(hold.empty()&&(!next||!contains(keep,next)))return;
    view_data=read<uintptr_t>(tasks+0x58);exposed=int32_t(keep.size());
    eligible=std::move(keep);retained=std::move(hold);
    // No native observer can acquire +f0 during this temporary view. Original
    // Retrieve alone removes its selected prefix entry and appends to running.
    masked=true;transfer_uncertain=true;
    for(size_t i=0;i<eligible.size();++i)write(view_data+i*8,eligible[i]);
    for(size_t i=0;i<retained.size();++i)write(view_data+(eligible.size()+i)*8,retained[i]);
    write(tasks+0x60,exposed);transfer_uncertain=false;if(!retained.empty())++blocked;
}
void restore(uintptr_t tasks,bool unwinding){
    if(!masked)return;
    const auto count=read<int32_t>(tasks+0x60);
    require(read<uintptr_t>(tasks+0x58)==view_data&&(count==exposed||count==exposed-1),"Ordered Run native retrieval changed its pending view");
    const auto selected=count==exposed-1?eligible.front():0;
    transfer_uncertain=true;
    for(size_t i=0;i<retained.size();++i)write(view_data+(size_t(count)+i)*8,retained[i]);
    write(tasks+0x60,int32_t(size_t(count)+retained.size()));
    masked=false;transfer_uncertain=false;retained.clear();eligible.clear();
    if(selected){
        // A native allocation failure after removal is an uncertain transfer.
        // Restore retained ownership, preserve the original exception, and do
        // not invent a Finish or replay the selected task.
        if(unwinding){transfer_uncertain=true;runtime->log("steps","error","Ordered Run native retrieval unwound after pending removal; selected ownership is uncertain; no replay");return;}
        require(contains(entries(tasks,0xa0),selected),"Ordered Run selected task lacks native running ownership");
        if(is_plan(selected)&&!read<uint8_t>(selected+0xc)){
            require(open&&!running&&cursor<batch.size()&&batch[cursor]==selected,"Ordered Run selected task lacks ordered admission");
            running=selected;++cursor;++admitted;
        }
    }
}
void completion(uintptr_t tasks){
    if(running){
        if(contains(entries(tasks,0xa0),running))return;
        require(contains(entries(tasks,0xb8),running),"Ordered Run completed task lost native finished ownership");
        running=0;++completed;
    }
    // Native TaskFinished does not post. One token admits the next frozen plan
    // even when other workers consumed all original tokens while it was held.
    if(open&&cursor<batch.size())post(reinterpret_cast<void*>(tasks+0xe0));
}
void before_unlock(void* mutex,uintptr_t caller){
    const auto tasks=bound.load(std::memory_order_acquire);if(!tasks||uintptr_t(mutex)!=tasks+0xf0)return;
    if(caller==image+RetrieveUnlock||caller==image+PausedUnlock||caller==image+RetrieveUnwind)restore(tasks,caller==image+RetrieveUnwind);
    else if(caller==image+CompleteUnlock)completion(tasks);
}
void lock_call(void* mutex,uintptr_t caller){
    original_lock(mutex);
    const auto tasks=bound.load(std::memory_order_acquire);
    if(caller!=image+RetrieveLock||!tasks||uintptr_t(mutex)!=tasks+0xf0)return;
    try{select(tasks);}
    catch(const std::exception& error){original_unlock(mutex);runtime->log("steps","error","Ordered Run retrieval admission failed: "+std::string(error.what()));throw;}
    catch(...){original_unlock(mutex);runtime->log("steps","error","Ordered Run retrieval admission raised an unknown exception; no replay");throw;}
}
__declspec(noinline) void lock(void* mutex){lock_call(mutex,reinterpret_cast<uintptr_t>(_ReturnAddress()));}
Json status(){
    const auto tasks=bound.load(std::memory_order_acquire);if(!tasks)return {{"armed",false}};
    Guard guard(tasks);return {{"armed",true},{"open",open},{"masked",masked},{"uncertain",transfer_uncertain},{"running",running!=0},{"batches",batches},{"admitted",admitted},{"completed",completed},{"blocked",blocked}};
}
}
