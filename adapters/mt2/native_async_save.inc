// Only the original SaveTask and its original stream writer may run while a
// prepared async capture holds the world. Dispatch keeps native completion on
// the main thread; FinishAllTasks cannot satisfy the writer's dependency.
namespace async_save {
using Queue=void(*)(void*,void*);
Queue original_queue=nullptr;
Go original_run=nullptr,original_finish=nullptr,original_writer_run=nullptr,original_writer_finish=nullptr,original_pump=nullptr,dispatch=nullptr;
constexpr uintptr_t SaveTask=0x16dbca0,WriterTask=0x1733bb0;
struct Capture {
    uintptr_t game=0,mode=0,tasks=0;DWORD thread=0;bool absolute=false;
    std::string filename,target;
    MT2FW_MT2_SnapshotRequest certificate{};
    mt2::PreparedSaveState save;
    std::atomic<uintptr_t> task{0},writer{0};
    std::atomic<DWORD> worker{0};
    std::atomic<unsigned> run_entered{0},run_returned{0},writer_completed{0},writer_run_entered{0},writer_run_returned{0};
    uintptr_t writer_file=0;
    unsigned finish_entered=0,finish_returned=0,writer_finish_returned=0;
    bool entry_returned=false,pumping=false,unlock_seen=false,complete=false;
    uintptr_t copied_allocation=0,copied_task=0;
    unsigned copied_finish_before=0;bool copied_noop_finish=false;
};
std::unique_ptr<Capture> retained;
std::atomic<Capture*> published{nullptr};
thread_local Capture* worker_scope=nullptr;
std::string text_at(uintptr_t object){
    const auto data=read<uintptr_t>(object),size=read<uint64_t>(object+8);
    require(data&&size&&size<=4096,"Async Save native string header differs");
    std::string value(size,'\0');SIZE_T copied=0;
    require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(data),value.data(),size,&copied)&&copied==size&&
        value.find('\0')==std::string::npos,"Async Save native string bytes differ");return value;
}
Capture& held_owner(){
    auto* value=published.load(std::memory_order_acquire);
    require(value&&retained.get()==value&&value->thread==thread&&thread==GetCurrentThreadId(),"Async Save lacks its retained main-thread owner");
    require(value->game==owner&&read<uintptr_t>(owner+0x28)==value->mode&&read<uintptr_t>(value->mode)==image+0x16e6730&&
        value->tasks==held_tasks&&tasks()==value->tasks,"Async Save native Game/Mode/Task owner changed");
    const auto state=read<uintptr_t>(value->mode+0x58),clock=read<uintptr_t>(image+0x13df9a0);
    require(state&&clock&&read<uintptr_t>(state+0x158)==clock&&read<uintptr_t>(clock)==image+0x17499d0&&
        read<uint32_t>(clock+0x58)==0&&read<uint32_t>(clock+0x5c)==0,"Async Save prepared Clock owner/selected/pause scale differs");
    held();return *value;
}
void begin(void* mode,const void* filename,bool absolute){
    snapshot_prepare::capture_owner();auto& scope=*mt2::PreparedSaveScope::current;
    require(!published.load()&&!retained&&scope.saves==0&&scope.flushes==0&&scope.resumes==0,
        "Async Save certificate already owns a native capture");
    auto value=std::make_unique<Capture>();value->game=owner;value->mode=reinterpret_cast<uintptr_t>(mode);
    value->tasks=held_tasks;value->thread=thread;value->absolute=absolute;
    require(value->mode==scope.mode,"Async Save Mode differs from prepared capture");
    value->filename=text_at(reinterpret_cast<uintptr_t>(filename));
    value->target=(absolute?std::string{}:text_at(value->mode+0x2e0)+"/")+value->filename+".vrz";
    value->certificate=snapshot_prepare::certificate;scope.asynchronous=true;
    retained=std::move(value);published.store(retained.get(),std::memory_order_release);
}
void entry_returned(){
    auto& value=held_owner();const auto* scope=mt2::PreparedSaveScope::current;
    require(scope&&scope->asynchronous&&scope->saves==1&&scope->flushes==1&&value.task.load()&&!value.entry_returned,
        "Original async Save did not queue exactly one task after its admitted preparation");
    value.entry_returned=true;
}
void save_task(Capture& value,uintptr_t task){
    require(task&&read<uintptr_t>(task)==image+SaveTask&&read<int32_t>(task+8)==0&&
        read<uintptr_t>(task+0x10)==value.mode&&text_at(task+0x18)==value.filename&&
        read<uint8_t>(task+0x38)==uint8_t(value.absolute)&&read<uint8_t>(task+0xc)==0&&read<uint8_t>(task+0xd)==0,
        "Async SaveTask receiver, filename or original absolute option differs");
}
uintptr_t live_writer_file(Capture& value,uintptr_t task){
    const auto file=read<uintptr_t>(task+0x10);
    require(file,"Async stream writer live file is null");
    require(read<uintptr_t>(file)==image+0x1742fd0,"Async stream writer live file address point differs");
    require(text_at(file+8)==value.target,"Async stream writer exact original target differs");
    const auto mode=read<int32_t>(file+0x70);
    require(mode==4,"Async stream writer file mode differs: actual="+std::to_string(mode)+" required=4");
    return file;
}
void writer_task(Capture& value,uintptr_t task){
    loading_dispatch::save_completion::writer(task);
    const auto retained_task=value.writer.load(std::memory_order_acquire);
    if(!retained_task){
        require(!value.writer_file&&!value.writer_run_entered&&!value.writer_run_returned,
            "Async stream writer file admission repeated");
        value.writer_file=live_writer_file(value,task);return;
    }
    require(retained_task==task&&value.writer_file,"Async stream writer retained task/file owner differs");
    // Original Run deletes the file and clears task+0x10. Its return receipt
    // permits reading that cleared slot, never the retired file allocation.
    if(value.writer_run_returned.load(std::memory_order_acquire))
        require(value.writer_run_returned==1&&value.writer_run_entered==1&&read<uintptr_t>(task+0x10)==0,
            "Async stream writer returned without exact native file retirement");
}
void run_writer(void* task){
    auto* value=published.load(std::memory_order_acquire);
    if(!value||value->writer.load(std::memory_order_acquire)!=reinterpret_cast<uintptr_t>(task)){
        original_writer_run(task);return;
    }
    require(GetCurrentThreadId()!=value->thread&&value->writer_run_entered.fetch_add(1)==0,
        "Async stream writer Run owner/reentry differs");
    loading_dispatch::save_completion::writer(reinterpret_cast<uintptr_t>(task));
    require(live_writer_file(*value,reinterpret_cast<uintptr_t>(task))==value->writer_file,
        "Async stream writer Run changed its admitted file");
    original_writer_run(task);
    require(read<uintptr_t>(reinterpret_cast<uintptr_t>(task)+0x10)==0,
        "Async stream writer original Run did not retire its file");
    value->writer_run_returned.fetch_add(1,std::memory_order_release);
}
void queue(void* manager,void* task,uintptr_t caller){
    auto* value=published.load(std::memory_order_acquire);
    if(value&&caller==image+0x4a052d){
        auto& owned=held_owner();require(&owned==value&&reinterpret_cast<uintptr_t>(manager)==value->tasks,"Async SaveTask queue manager differs");
        save_task(*value,reinterpret_cast<uintptr_t>(task));uintptr_t absent=0;
        require(value->task.compare_exchange_strong(absent,reinterpret_cast<uintptr_t>(task)),"Async SaveTask queued more than once");
    }else if(value&&worker_scope==value){
        require(caller==image+0x881d48&&reinterpret_cast<uintptr_t>(manager)==value->tasks&&GetCurrentThreadId()==value->worker.load(),
            "Async Save worker queued a foreign task/caller");
        writer_task(*value,reinterpret_cast<uintptr_t>(task));uintptr_t absent=0;
        require(value->writer.compare_exchange_strong(absent,reinterpret_cast<uintptr_t>(task)),"Async Save queued more than one stream writer");
    }
    original_queue(manager,task);
}
__declspec(noinline) void queue_entry(void* manager,void* task){queue(manager,task,reinterpret_cast<uintptr_t>(_ReturnAddress()));}
void run(void* task){
    auto* value=published.load(std::memory_order_acquire);
    if(!value||value->task.load()!=reinterpret_cast<uintptr_t>(task)){original_run(task);return;}
    require(!worker_scope&&GetCurrentThreadId()!=value->thread&&value->run_entered.fetch_add(1)==0,"Async SaveTask Run owner/reentry differs");
    save_task(*value,reinterpret_cast<uintptr_t>(task));value->worker=GetCurrentThreadId();worker_scope=value;
    struct Clear{~Clear(){worker_scope=nullptr;}}clear;
    original_run(task);require(value->writer_completed.load()==1,"Async SaveTask returned before its exact writer completed");
    ++value->run_returned;
}
bool writer_complete(void* task,uintptr_t caller,Go original){
    auto* value=published.load(std::memory_order_acquire);
    if(!value||worker_scope!=value||caller!=image+0x8819fb)return false;
    require(value->writer.load()==reinterpret_cast<uintptr_t>(task)&&GetCurrentThreadId()==value->worker.load()&&
        value->run_entered==1&&value->run_returned==0&&value->writer_completed==0,"Async writer Complete owner/count differs");
    writer_task(*value,reinterpret_cast<uintptr_t>(task));original(task);
    require(read<uint8_t>(reinterpret_cast<uintptr_t>(task)+0xe)==1&&value->writer_run_returned==1,
        "Async writer Complete returned without native Run and completion");
    ++value->writer_completed;return true;
}
void finish(void* task,uintptr_t caller){
    auto* value=published.load(std::memory_order_acquire);
    if(!value||value->task.load()!=reinterpret_cast<uintptr_t>(task)){original_finish(task);return;}
    auto& owned=held_owner();require(&owned==value&&value->pumping&&value->unlock_seen&&caller==image+0x526c55&&
        value->copied_task==reinterpret_cast<uintptr_t>(task)&&value->run_returned==1&&value->writer_completed==1&&
        value->finish_entered++==0,"Async SaveTask Finish lacks its exact completed copied task");
    save_task(*value,reinterpret_cast<uintptr_t>(task));original_finish(task);++value->finish_returned;
}
__declspec(noinline) void finish_entry(void* task){finish(task,reinterpret_cast<uintptr_t>(_ReturnAddress()));}
void finish_writer(void* task,uintptr_t caller){
    auto* value=published.load(std::memory_order_acquire);
    if(!value||value->writer.load()!=reinterpret_cast<uintptr_t>(task)){original_writer_finish(task);return;}
    auto& owned=held_owner();require(&owned==value&&value->pumping&&value->unlock_seen&&caller==image+0x526c55&&
        value->copied_task==reinterpret_cast<uintptr_t>(task)&&value->writer_finish_returned==0&&value->writer_run_returned==1,
        "Async writer Finish lacks its exact copied task");
    writer_task(*value,reinterpret_cast<uintptr_t>(task));original_writer_finish(task);++value->writer_finish_returned;
}
__declspec(noinline) void writer_finish_entry(void* task){finish_writer(task,reinterpret_cast<uintptr_t>(_ReturnAddress()));}
bool before_unlock(void* mutex){
    auto* scope=mt2::PreparedSaveScope::current;if(!scope||!scope->asynchronous)return false;
    auto& value=held_owner();require(value.pumping&&!value.unlock_seen&&reinterpret_cast<uintptr_t>(mutex)==value.tasks+0xf0,
        "Async Save native pump mutex/snapshot owner differs");
    std::set<uintptr_t> unique;
    for(size_t offset:{size_t(0x28),size_t(0x40),size_t(0x58),size_t(0x70),size_t(0x88),size_t(0xa0),size_t(0xb8),size_t(0xd0)}){
        const auto count=read<int32_t>(value.tasks+offset+8),capacity=read<int32_t>(value.tasks+offset+12);
        const auto data=read<uintptr_t>(value.tasks+offset);
        require(count>=0&&count<=2&&capacity>=count&&capacity<=1000000&&(!count||data),"Async Save queue bounds differ offset="+std::to_string(offset));
        if(offset!=0x28&&offset!=0x70&&offset!=0xd0)require(count==0,"Async Save has unrelated live queue offset="+std::to_string(offset));
        if(offset==0xd0){
            require(count<=1&&capacity==count,"Async Save copied callback batch violates writer-before-SaveTask dependency");
            value.copied_allocation=data;
        }
        for(int32_t i=0;i<count;++i){const auto task=read<uintptr_t>(data+size_t(i)*8);
            require(unique.insert(task).second,"Async Save task appears in multiple live queues");
            if(task==value.task.load())save_task(value,task);
            else {require(task==value.writer.load(),"Async Save contains foreign task");writer_task(value,task);}
            if(offset==0xd0){
                value.copied_task=task;value.copied_noop_finish=task==value.writer.load();
                value.copied_finish_before=value.copied_noop_finish?value.writer_finish_returned:value.finish_returned;
                if(value.copied_noop_finish){
                    // Pump compares this exact slot with the base no-op and
                    // publishes completion without calling Finish at all.
                    require(read<uintptr_t>(read<uintptr_t>(task)+0x18)==image+0xc367a0&&
                        read<uint8_t>(task+0xe)==0&&value.writer_run_entered==1&&value.writer_run_returned==1&&
                        value.writer_finish_returned==0,
                        "Async Save copied writer lacks its exact native no-op Finish branch");
                }
            }
        }
    }
    value.unlock_seen=true;return true;
}
void pump_native(void* manager){
    const auto* scope=mt2::PreparedSaveScope::current;
    if(!scope||!scope->asynchronous||loading_dispatch::save_cache::current){original_pump(manager);return;}
    auto& value=held_owner();require(!value.pumping&&reinterpret_cast<uintptr_t>(manager)==value.tasks,"Async Save pump reentered or changed manager");
    value.pumping=true;value.unlock_seen=false;value.copied_task=value.copied_allocation=0;value.copied_noop_finish=false;
    struct Clear{Capture& value;~Clear(){value.pumping=false;}}clear{value};
    original_pump(manager);require(value.unlock_seen,"Async Save pump returned without its native copied-array boundary");
    auto& completion=loading_dispatch::save_completion::consumed;
    if(!value.copied_task){loading_dispatch::save_completion::clear_consumed();return;}
    require(value.copied_task==(value.copied_noop_finish?value.writer.load():value.task.load()),
        "Async Save copied callback retained identity changed");
    const auto finishes=value.copied_noop_finish?value.writer_finish_returned:value.finish_returned;
    const auto required=value.copied_finish_before+(value.copied_noop_finish?0u:1u);
    require(finishes==required,"Async Save copied callback return count differs: actual="+std::to_string(finishes)+
        " required="+std::to_string(required)+" native_noop="+std::to_string(value.copied_noop_finish));
    // Pump's pinned loop returned after its exact Finish/no-op branch and
    // completion publication. The worker may now delete the writer; retain
    // only its pointer value and never dereference that retired allocation.
    completion={value.game,value.mode,value.tasks,value.copied_allocation,value.copied_task,value.thread};
    require(loading_dispatch::save_completion::consumed_snapshot(value.tasks),"Async Save consumed callback allocation changed");
}
bool resume(){
    const auto* scope=mt2::PreparedSaveScope::current;if(!scope||!scope->asynchronous)return false;
    auto& value=held_owner();require(value.pumping&&value.unlock_seen&&value.copied_task==value.task.load()&&
        value.finish_entered==1&&value.finish_returned==0&&value.writer_completed==1,"Async Save Resume is outside original SaveTask Finish");
    for(size_t offset:{size_t(0x28),size_t(0x40),size_t(0x58),size_t(0x70),size_t(0x88),size_t(0xa0),size_t(0xb8)})
        require(read<int32_t>(value.tasks+offset+8)==0,"Async Save Resume has live work offset="+std::to_string(offset));
    require(read<uintptr_t>(value.tasks+0xd0)==value.copied_allocation&&read<int32_t>(value.tasks+0xd8)==1&&
        read<uintptr_t>(value.copied_allocation)==value.copied_task,"Async Save Resume copied task changed");return true;
}
bool pending(){return published.load()!=nullptr&&!retained->complete;}
bool started(){return published.load()!=nullptr&&retained->entry_returned;}
void retain_scope(const mt2::PreparedSaveScope& scope){
    auto& value=held_owner();value.save=scope;require(scope.asynchronous,"Async Save capture lost its original async scope");
}
bool pump(){
    if(!published.load())return true;auto& value=held_owner();if(value.complete)return true;
    mt2::PreparedSaveScope scope(value.save);mt2::BlockingSaveScope native(value.mode,value.thread);
    if(!value.task.load()){
        require(!value.entry_returned&&!value.run_entered&&read<int32_t>(value.mode+0x30c)==0,
            "Failed async Save has no queued task but retains native ownership; no retirement");
        snapshot_prepare::empty_tasks();value.complete=true;return true;
    }
    require(dispatch,"Async Save original Dispatch is unavailable");dispatch(reinterpret_cast<void*>(value.tasks));
    const auto saves=read<int32_t>(value.mode+0x30c);
    require(saves==0||saves==1,"Async Save count changed outside its single capture");if(saves)return false;
    require(value.run_entered==1&&value.run_returned==1&&value.writer_run_entered==1&&value.writer_run_returned==1&&value.writer_completed==1&&value.finish_entered==1&&value.finish_returned==1&&
        scope.saves==1&&scope.flushes==1&&scope.resumes==1,"Async Save native completion receipts are incomplete");
    snapshot_prepare::settle_cache();
    snapshot_prepare::empty_tasks();value.complete=true;return true;
}
void retire(){
    if(!retained)return;require(retained->complete,"Async Save cannot retire ownership before native completion");
    published.store(nullptr,std::memory_order_release);retained.reset();
}

void install(){
    for(const auto& [rva,bytes,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,10>{{
        {0xc15ac0,2672,"4dd93e72eae28e7493bf970db61c2320e205b28b8adafce2dfaff70b50a5d668"},
        {0x881ac0,112,"7c058de8178a5b5e3ac91bd4d64c5ae9276983a6863f184e8f98d09cc15a3284"},
        {0x5258e0,384,"34f36fc926028624a1fb7e1a5ac55effdad48942080b88d408a8a67a3d2c0ae3"},
        {0xb1f260,208,"30caad5ea0dad7bb968e8b0b4f5584379232fc62053140abe538b836c1c3d2a1"},
        {0xb1f330,32,"8ec6771455dea60801b4c54c815ab21b02f5da3632a9efc39fe7f847149c2148"},
        {0xc367a0,16,"499f1f307c1cb989f968a6b7fcaec591e1828877223d0b0e7e8e8b76cde8c9ca"},
        {0x526b00,432,"12ca52a501a074d6459b697bf9df2d8c4a7322bed76270b0cfe9dc33b91bd016"},
        {0x4a0490,240,"605ae7ba90817d201b07235fee899416b8a69f16316bb6ce55bab1caa5fcfea2"},
        {0x881b30,672,"f0dea32c427373cf7bfc89c1d1dea5183fddd2f452b03d038427559489202303"},
        {0x526ce0,112,"82d0a5a46e233f07dfe6924a98cfef8c7c1ae2a4d850fe2e65363655205fc142"}
    }}){std::string cause;const bool valid=native_routes->verify_body(rva,bytes,sha,cause);
        require(valid,"Async Save original body pin differs rva="+std::to_string(rva)+": "+cause);}
    dispatch=reinterpret_cast<Go>(image+0x526ce0);
    require(read<uintptr_t>(image+SaveTask+0x10)==image+0xb1f260&&read<uintptr_t>(image+SaveTask+0x18)==image+0xb1f330,
        "Async SaveTask original virtual slots differ");
    const auto attach=[](const char* key,uintptr_t rva,void* detour,void** original){
        native_routes->catalog(key,rva,true);native_services->reserve(key);auto* target=reinterpret_cast<void*>(image+rva);
        hook(target,detour,original);const auto result=MH_EnableHook(target);
        require(result==MH_OK,std::string("Async Save hook enable failed: ")+key+" "+MH_StatusToString(result));
        std::string cause;const bool published=native_routes->publish_framework_hook(target,detour,*original,cause);
        require(published,std::string("Async Save hook publication failed: ")+key+" "+cause);
    };
    attach("framework.save.async-queue",0x5258e0,reinterpret_cast<void*>(&queue_entry),reinterpret_cast<void**>(&original_queue));
    attach("framework.save.async-run",0xb1f260,reinterpret_cast<void*>(&run),reinterpret_cast<void**>(&original_run));
    attach("framework.save.async-finish",0xb1f330,reinterpret_cast<void*>(&finish_entry),reinterpret_cast<void**>(&original_finish));
    attach("framework.save.async-writer-run",0xc15ac0,reinterpret_cast<void*>(&run_writer),reinterpret_cast<void**>(&original_writer_run));
    attach("framework.save.async-writer-finish",0xc367a0,reinterpret_cast<void*>(&writer_finish_entry),reinterpret_cast<void**>(&original_writer_finish));
    attach("framework.save.async-pump",0x526b00,reinterpret_cast<void*>(&pump_native),reinterpret_cast<void**>(&original_pump));
}
}
