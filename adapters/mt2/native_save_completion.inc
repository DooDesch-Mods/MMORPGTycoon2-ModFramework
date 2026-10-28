// Native Save pauses normal work while its type-0 writer completes.
namespace save_completion {
Finish original=nullptr;
constexpr uintptr_t Writer=0x1733bb0,WriterReturn=0x8819fb;
struct Context {uintptr_t task=0,tasks=0,finished_snapshot=0;};
struct ConsumedSnapshot {uintptr_t game=0,mode=0,tasks=0,allocation=0,task=0;uint32_t thread=0;};
ConsumedSnapshot consumed;
void clear_consumed(){consumed={};}
bool consumed_snapshot(uintptr_t tasks){
    // Only the array pointer value is read here. The completed writer has been
    // deleted by its original stream destructor after Complete returned.
    return consumed.tasks==tasks&&consumed.game==owner&&consumed.thread==thread&&thread==GetCurrentThreadId()&&
        consumed.mode==read<uintptr_t>(owner+0x28)&&consumed.allocation&&consumed.task&&
        read<uintptr_t>(tasks+0xd0)==consumed.allocation&&read<int32_t>(tasks+0xd8)==1&&read<int32_t>(tasks+0xdc)==1&&
        read<uintptr_t>(consumed.allocation)==consumed.task;
}
thread_local Context* current=nullptr;
void validate_owner(){
    const auto* scope=mt2::BlockingSaveScope::current;
    require(scope&&!scope->previous&&scope->thread==thread&&thread==GetCurrentThreadId(),"Save completion lacks exact blocking owner thread");
    const auto* prepared=mt2::PreparedSaveScope::current;
    const bool prepared_owner=prepared&&prepared->mode==scope->mode&&prepared->thread==thread&&prepared->tasks==held_tasks&&
        !control.entered&&!timer_step::frame.active&&control.phase==mt2::determinism::Gate::Phase::Held;
    require(prepared_owner||(control.entered&&timer_step::frame.active&&!held_tasks),"Save completion lacks admitted outer frame or prepared held capture");
    timer_step::current_game();
    require(scope->mode!=0,"Save completion Mode owner is null actual=0 required=nonzero");
    const auto active_mode=read<uintptr_t>(owner+0x28);
    require(active_mode==scope->mode,"Save completion active Mode differs actual="+std::to_string(active_mode)+" required="+std::to_string(scope->mode));
    // The native object stores the address point after the two ABI header words.
    const auto mode_address_point=read<uintptr_t>(scope->mode);
    require(mode_address_point==image+0x16e6730,"Save completion Mode address point differs actual="+std::to_string(mode_address_point)+
        " required="+std::to_string(image+0x16e6730)+" required_rva=0x16e6730");
    const auto saves=read<int32_t>(scope->mode+0x30c);
    require(saves==1,"Save completion active save count differs actual="+std::to_string(saves)+" required=1");
    require(!uncertain&&!pending,"Save completion has deferred or uncertain gameplay callbacks");
}
void writer(uintptr_t task){
    require(task&&read<uintptr_t>(task)==image+Writer&&read<int32_t>(task+8)==0&&read<uint8_t>(task+0xc)==0&&read<uint8_t>(task+0xd)==0&&
        read<uintptr_t>(image+Writer+0x10)==image+0xc15ac0&&read<uintptr_t>(image+Writer+0x18)==image+0xc367a0,
        "Save completion receiver is not the original live stream writer");
}
void before_unlock(void* mutex){
    validate_owner();const auto tasks=read<uintptr_t>(image+0x13df340);
    require(current&&tasks==current->tasks&&read<uintptr_t>(owner+0x68)==tasks&&read<uintptr_t>(tasks)==image+0x16fdaf0&&
        reinterpret_cast<uintptr_t>(mutex)==tasks+0xf0,"Save completion Task owner/mutex differs");
    require(read<uint8_t>(tasks+0xf8)==1,"Save completion requires original Save pause");
    writer(current->task);require(read<uint8_t>(current->task+0xe)==0,"Save completion writer already completed before native pump");
    current->finished_snapshot=0;
    size_t live=0;
    for(const auto offset:{size_t(0x28),size_t(0x40),size_t(0x58),size_t(0x70),size_t(0x88),size_t(0xa0),size_t(0xb8),size_t(0xd0)}){
        const auto count=read<int32_t>(tasks+offset+8),capacity=read<int32_t>(tasks+offset+12);const auto data=read<uintptr_t>(tasks+offset);
        require(count>=0&&capacity>=count&&capacity<=1000000&&count<=1&&(!count||data),"Save completion native queue bounds differ offset="+std::to_string(offset));
        if(!count)continue;
        require((offset==0x28||offset==0x70||offset==0xd0)&&read<uintptr_t>(data)==current->task&&++live==1,
            "Save completion encountered unrelated or duplicate work offset="+std::to_string(offset));
        if(offset==0xd0){
            require(capacity==1,"Save completion native callback snapshot capacity differs actual="+std::to_string(capacity)+" required=1");
            current->finished_snapshot=data;
        }
    }
    require(live==1,"Save completion lost original writer ownership");
}
void record_consumed(const Context& context){
    if(!mt2::PreparedSaveScope::current)return;
    validate_owner();writer(context.task);
    require(context.tasks==held_tasks&&context.tasks==read<uintptr_t>(image+0x13df340)&&
        context.tasks==read<uintptr_t>(owner+0x68)&&read<uintptr_t>(context.tasks)==image+0x16fdaf0&&
        read<uint8_t>(context.tasks+0xf8)==1,"Save completion returned with changed Task owner or pause");
    require(read<uint8_t>(context.task+0xe)==1,"Save completion cannot acknowledge unfinished writer");
    for(const auto offset:{size_t(0x28),size_t(0x40),size_t(0x58),size_t(0x70),size_t(0x88),size_t(0xa0),size_t(0xb8)}){
        const auto count=read<int32_t>(context.tasks+offset+8),capacity=read<int32_t>(context.tasks+offset+12);
        require(count==0&&capacity>=0&&capacity<=1000000,"Save completion returned with live task queue offset="+
            std::to_string(offset)+" actual="+std::to_string(count)+" required=0");
    }
    require(context.finished_snapshot&&read<uintptr_t>(context.tasks+0xd0)==context.finished_snapshot&&
        read<int32_t>(context.tasks+0xd8)==1&&read<int32_t>(context.tasks+0xdc)==1&&
        read<uintptr_t>(context.finished_snapshot)==context.task,"Save completion consumed callback snapshot differs");
    consumed={owner,mt2::PreparedSaveScope::current->mode,context.tasks,context.finished_snapshot,context.task,thread};
}
void call(void* task,uintptr_t caller){
#ifdef MT2FW_ORDERED_ASYNC_SAVE
    if(async_save::writer_complete(task,caller,original))return;
#endif
    if(!enabled||!control.active()||!mt2::BlockingSaveScope::current||caller!=image+WriterReturn){original(task);return;}
    validate_owner();require(!current,"Save completion reentered");clear_consumed();writer(reinterpret_cast<uintptr_t>(task));
    Context context{reinterpret_cast<uintptr_t>(task),read<uintptr_t>(image+0x13df340)};current=&context;
    struct Clear{~Clear(){current=nullptr;}} clear;
    original(task);
    require(read<uint8_t>(context.task+0xe)==1,"Save completion original returned without writer completion");
    record_consumed(context);
}
__declspec(noinline) void entry(void* task){call(task,reinterpret_cast<uintptr_t>(_ReturnAddress()));}
}
