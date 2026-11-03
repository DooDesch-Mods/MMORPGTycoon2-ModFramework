namespace {
void preview_cause(char* output,const char* input) noexcept {
    size_t count=0;if(input)while(count<159&&input[count]){output[count]=input[count];++count;}output[count]=0;
}
MT2FW_MT2_Result preview_callback_cpp(MT2FW_MT2_CharacterPreviewIntSelector callback,void* user,
    const MT2FW_MT2_CharacterPreviewIntCall* call,MT2FW_MT2_CharacterPreviewIntSelection* selection){
    try{return callback(user,call,selection);}
    catch(const std::exception& failure){preview_cause(selection->cause,failure.what());return MT2FW_MT2_FAILED;}
    catch(...){preview_cause(selection->cause,"Character preview selector raised an unknown C++ exception");return MT2FW_MT2_FAILED;}
}
MT2FW_MT2_Result preview_callback_pod(MT2FW_MT2_CharacterPreviewIntSelector callback,void* user,
    const MT2FW_MT2_CharacterPreviewIntCall* call,MT2FW_MT2_CharacterPreviewIntSelection* selection,DWORD* exception){
    __try{return preview_callback_cpp(callback,user,call,selection);}
    __except(EXCEPTION_EXECUTE_HANDLER){*exception=GetExceptionCode();return MT2FW_MT2_FAILED;}
}
bool preview_storage(void* pointer,uintptr_t native,uintptr_t slot,char* cause) noexcept {
    const auto address=reinterpret_cast<uintptr_t>(pointer);constexpr size_t bytes=MT2FW_MT2_PREVIEW_RANDOM_BYTES;
    if(!address||address%alignof(uint64_t)||address>UINTPTR_MAX-bytes){preview_cause(cause,"Character preview private source is null, unaligned or overflows");return false;}
    if((address<native+bytes&&native<address+bytes)||(address<slot+sizeof(uintptr_t)&&slot<address+bytes)){
        preview_cause(cause,"Character preview private source overlaps shared RNG storage");return false;}
    for(auto cursor=address;cursor<address+bytes;){MEMORY_BASIC_INFORMATION info{};
        if(!VirtualQuery(reinterpret_cast<void*>(cursor),&info,sizeof(info))||info.State!=MEM_COMMIT||
           (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(info.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))){
            preview_cause(cause,"Character preview private source is not committed writable storage");return false;}
        const auto end=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;
        if(end<=cursor){preview_cause(cause,"Character preview private source memory region does not advance");return false;}cursor=end;
    }
    unsigned char copy[bytes]{};SIZE_T count=0;
    if(!ReadProcessMemory(GetCurrentProcess(),pointer,copy,bytes,&count)||count!=bytes){preview_cause(cause,"Character preview private source read is incomplete");return false;}
    return true;
}
}
void NativeServices::character_preview_backend(uintptr_t image,PreviewProof proof){
    std::lock_guard lock(preview_gate_);
    if(preview_image_||preview_.owner)throw std::runtime_error("Character preview backend already configured");
    if(!image||!proof)throw std::runtime_error("Character preview backend requires image and owned hook proof");
    preview_proof_=std::move(proof);preview_image_.store(image,std::memory_order_release);
}
MT2FW_MT2_Result MT2FW_CALL NativeServices::character_preview_bind(void* context,
    MT2FW_MT2_CharacterPreviewIntSelector callback,void* user,uint64_t* token){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;
    auto& service=*value->service;
    if(!callback||!token)return MT2FW_MT2_INVALID;
    if(value->plugin->manifest.id!="doodesch.multiplayer"||!value->plugin->active||value->plugin->staging||
       !service.preview_thread_.load()||thread_id()!=service.preview_thread_.load())return MT2FW_MT2_UNAVAILABLE;
    try{
        if(!service.preview_image_||!service.preview_proof_)return MT2FW_MT2_UNAVAILABLE;
        std::string cause;if(!service.preview_proof_(cause)){error(*value->plugin,"Character preview hook proof failed: "+cause);return MT2FW_MT2_FAILED;}
        std::lock_guard lock(service.preview_gate_);
        if(service.preview_.owner)return MT2FW_MT2_BUSY;
        if(service.next_token_==UINT64_MAX)return MT2FW_MT2_FAILED;
        const auto id=service.next_token_++;service.preview_={};service.preview_.owner=value;
        service.preview_.token=id;service.preview_.selector=callback;service.preview_.user=user;service.preview_.active=true;*token=id;return MT2FW_MT2_OK;
    }catch(const std::exception& failure){error(*value->plugin,std::string("Character preview bind failed: ")+failure.what());return MT2FW_MT2_FAILED;}
    catch(...){error(*value->plugin,"Character preview bind raised an unknown exception");return MT2FW_MT2_FAILED;}
}
MT2FW_MT2_Result MT2FW_CALL NativeServices::character_preview_unbind(void* context,uint64_t token){
    auto value=owner(context);if(!value)return MT2FW_MT2_UNAVAILABLE;auto& service=*value->service;
    if(!service.preview_thread_.load()||thread_id()!=service.preview_thread_.load())return MT2FW_MT2_UNAVAILABLE;
    std::lock_guard lock(service.preview_gate_);
    if(!token||service.preview_.owner!=value||service.preview_.token!=token)return MT2FW_MT2_INVALID;
    service.preview_.active=false;
    if(service.preview_.inflight)return MT2FW_MT2_BUSY;
    service.preview_={};return MT2FW_MT2_OK;
}
void NativeServices::character_preview_retire(Owner& value){
    std::lock_guard lock(preview_gate_);
    if(preview_.owner!=&value)return;
    preview_.active=false;
    // Logical detachment succeeds now; the existing native pin defers shutdown.
    if(!preview_.inflight)preview_={};
}
void NativeServices::character_preview_release(CharacterPreviewLease* lease) noexcept {
    if(!lease||!lease->owner)return;
    auto* value=static_cast<Owner*>(lease->owner);const auto token=lease->token;lease->owner=nullptr;lease->token=0;
    try{
        {std::lock_guard lock(preview_gate_);
            if(preview_.owner!=value||preview_.token!=token||!preview_.inflight)throw std::runtime_error("Character preview callback lease identity or count differs");
            --preview_.inflight;
            if(!preview_.inflight&&!value->alive.load(std::memory_order_acquire))preview_={};
        }
        value->plugin->runtime->release_native(*value->plugin);
    }catch(const std::exception& failure){std::fprintf(stderr,"error: Character preview lease release failed: %s\n",failure.what());}
    catch(...){std::fprintf(stderr,"error: Character preview lease release raised an unknown exception\n");}
}
[[noreturn]] void NativeServices::character_preview_fail(CharacterPreviewLease* lease,const char* cause){
    Owner* value=nullptr;char retained[160]{};bool first=false;
    {std::lock_guard lock(preview_gate_);value=preview_.owner;
        if(!preview_.failed){preview_cause(preview_.cause,cause);preview_.failed=true;first=true;}
        preview_.active=false;preview_cause(retained,preview_.cause);}
    character_preview_release(lease);
    if(first&&value){try{error(*value->plugin,std::string("Character preview aborted: ")+retained);}
        catch(const std::exception& failure){std::fprintf(stderr,"error: Character preview aborted: %s; logging failed: %s\n",retained,failure.what());}
        catch(...){std::fprintf(stderr,"error: Character preview aborted: %s; logging raised an unknown exception\n",retained);}}
    RaiseException(MT2FW_MT2_CHARACTER_PREVIEW_ABORT,EXCEPTION_NONCONTINUABLE,0,nullptr);
    std::terminate();
}
void NativeServices::character_preview_select(uintptr_t caller,void* receiver,int32_t limit,CharacterPreviewLease* lease){
    if(!lease)throw std::runtime_error("Character preview requires an output lease");
    *lease={nullptr,0,receiver};
    const auto thread=preview_thread_.load(std::memory_order_acquire),image=preview_image_.load(std::memory_order_acquire);
    if(!image||!thread||thread_id()!=thread||caller<image)return;
    const auto rva=caller-image;
    if((rva!=0x9061c&&rva!=0x321ac0)||reinterpret_cast<uintptr_t>(receiver)!=image+0x1c4e7a0)return;
    MT2FW_MT2_CharacterPreviewIntSelector callback=nullptr;void* user=nullptr;bool recursive=false,failed=false;
    {std::lock_guard lock(preview_gate_);
        if(!preview_.owner||!preview_.owner->alive.load()||!preview_.owner->plugin->active)return;
        failed=preview_.failed;recursive=preview_.in_selector;
        if(!failed&&!recursive){
            if(!preview_.active)return;
            callback=preview_.selector;user=preview_.user;
            lease->owner=preview_.owner;lease->token=preview_.token;++preview_.inflight;++preview_.owner->plugin->native_pins;
            preview_.in_selector=true;
        }
    }
    if(failed)character_preview_fail(lease,"Character preview registration remains failed");
    if(recursive)character_preview_fail(lease,"Character preview selector reentered");
    uintptr_t actual=0;SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+0x164b250),&actual,sizeof(actual),&copied)||copied!=sizeof(actual)||actual!=reinterpret_cast<uintptr_t>(receiver)){
        {std::lock_guard lock(preview_gate_);preview_.in_selector=false;}
        character_preview_fail(lease,"Character preview Default pointer slot differs");
    }
    const MT2FW_MT2_CharacterPreviewIntCall call{sizeof(call),0,rva,thread,receiver,limit,0};
    MT2FW_MT2_CharacterPreviewIntSelection selection{sizeof(selection),0,nullptr,{}};DWORD exception=0;
    const auto result=preview_callback_pod(callback,user,&call,&selection,&exception);
    {std::lock_guard lock(preview_gate_);preview_.in_selector=false;failed=preview_.failed;}
    if(failed)character_preview_fail(lease,"Character preview selector retained a nested failure");
    if(exception){char cause[160]{};std::snprintf(cause,sizeof(cause),"Character preview selector raised SEH 0x%08lX",static_cast<unsigned long>(exception));character_preview_fail(lease,cause);}
    if(result==MT2FW_MT2_UNAVAILABLE)return;
    if(result!=MT2FW_MT2_OK){char cause[160]{};
        if(std::memchr(selection.cause,0,sizeof(selection.cause))&&selection.cause[0])preview_cause(cause,selection.cause);
        else std::snprintf(cause,sizeof(cause),"Character preview selector failed result=%u",unsigned(result));
        character_preview_fail(lease,cause);
    }
    if(selection.size!=sizeof(selection)||selection.source_bytes!=MT2FW_MT2_PREVIEW_RANDOM_BYTES||!std::memchr(selection.cause,0,sizeof(selection.cause)))
        character_preview_fail(lease,"Character preview selector returned malformed OK output");
    char cause[160]{};
    if(!preview_storage(selection.source,actual,image+0x164b250,cause))character_preview_fail(lease,cause);
    lease->source=selection.source;
}
