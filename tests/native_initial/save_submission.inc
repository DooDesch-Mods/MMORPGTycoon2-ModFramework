struct InitialSaveScope;
thread_local InitialSaveScope* initial_save_scope=nullptr;
struct InitialSaveScope {
    InitialSaveScope* previous=initial_save_scope;
    std::string folder,cause;
    Json metadata;
    bool consumed=false;
    InitialSaveScope(){initial_save_scope=this;}
    ~InitialSaveScope(){initial_save_scope=previous;}
    InitialSaveScope(const InitialSaveScope&)=delete;
    InitialSaveScope& operator=(const InitialSaveScope&)=delete;
};
void save_command(void* mode,const void* filename,bool absolute) {
    if(!running){original_save_command(mode,filename,absolute);return;}
    if(prepare_save())original_save_command(mode,filename,absolute);
}
__declspec(noinline) void save_blocking(void* mode,const void* filename,bool absolute) {
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(!running){original_blocking_save(mode,filename,absolute);return;}
    const auto expected=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))+0x4a2c3a;
    if(caller!=expected){if(prepare_save())original_blocking_save(mode,filename,absolute);return;}
    InitialSaveScope scope;
    try {
        std::lock_guard lock(gate);
        if(scope.previous || !runtime || !engine_thread || engine_thread!=GetCurrentThreadId())
            throw std::runtime_error("Initial save submission requires the bound game thread without a nested save");
        const char* bytes=nullptr;size_t length=0;
        auto code=command_view(filename,bytes,length);std::string stem;
        if(!code){stem.resize(length);code=command_copy(bytes,stem.data(),length);}
        if(code || absolute || stem!="initial_state")
            throw std::runtime_error("Initial save requires original relative initial_state submission; exception="+std::to_string(code));
        std::string error;
        if(!mt2mp::game::native::read_initial_save_world(mode,scope.folder,error))throw std::runtime_error(error);
        scope.metadata=runtime->capture_initial_save(scope.folder);
    } catch(const std::exception& error) {
        scope.cause=error.what();std::lock_guard lock(gate);
        report("Initial save metadata capture failed: "+scope.cause);
    }
    // Native saving keeps its original failure behavior if metadata cannot be represented.
    original_blocking_save(mode,filename,absolute);
}
Json capture_save_metadata(const char* name,const fs::path& path) {
    if(!initial_save_scope)return runtime->capture_save(path_text(path.parent_path().filename()));
    auto& initial=*initial_save_scope;
    if(!initial.cause.empty())throw std::runtime_error("Initial save metadata unavailable: "+initial.cause);
    if(initial.consumed || !mt2fw::mt2::initial_save_path(name,initial.folder))
        throw std::runtime_error("Initial save metadata path or one-write ownership differs");
    initial.consumed=true;return initial.metadata;
}
