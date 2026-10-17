#pragma once
#include "native_routes.h"

namespace mt2fw::mt2 {
class NativeOwnership {
public:
    struct Snapshot {
        uintptr_t slot=0,current=0,malloc_slot=0,malloc_current=0;
        uint32_t protection=0;
        MT2FW_MT2_ModuleProof imported_crt{},free_provider{},malloc_provider{};
        MT2FW_MT2_ExportProof free_export{},malloc_export{};
    };
    struct Exchange {bool attempted=false,exchanged=false,protection_restored=false;uintptr_t observed=0;uint32_t before=0,win32=0,seh=0;};
    struct Backend {
        std::function<bool(Snapshot&,std::string&)> prepare;
        std::function<bool(Snapshot&,std::string&)> read;
        std::function<bool(uintptr_t,std::string&)> replacement;
        std::function<Exchange(uintptr_t,uintptr_t,uint32_t,bool)> exchange;
    };
    NativeOwnership(uintptr_t image,NativeRoutes& routes,std::function<Plugin*(const MT2FW_Host*)> lookup,Backend backend);
    MT2FW_MT2_Result query(const MT2FW_Host*,uint32_t,MT2FW_MT2_OwnershipV1*,uint32_t);
    bool retire(Plugin&);
    bool has_retention() const;
    bool retirement_blocked() const;
    void observe_command(const MT2FW_MT2_CommandCall&);
    void observe_input(const MT2FW_MT2_PolledEvent&);
    std::vector<std::pair<Plugin*,std::string>> drain_errors();
private:
    struct Owner {NativeOwnership* service;Plugin* plugin;bool active=true;size_t inflight=0;};
    struct Command {uint64_t token;Owner* owner;MT2FW_MT2_CommandObserver callback;void* user;bool active=true;};
    struct Input {uint64_t token;Owner* owner;MT2FW_MT2_InputObserver callback;void* user;bool active=true;};
    uintptr_t image_;
    NativeRoutes& routes_;
    std::function<Plugin*(const MT2FW_Host*)> lookup_;
    Backend backend_;
    mutable std::recursive_mutex gate_;
    std::map<Plugin*,std::unique_ptr<Owner>> owners_;
    std::vector<Command> commands_;
    std::vector<Input> inputs_;
    uint64_t next_observer_token_=1,next_command_invocation_=0,next_input_invocation_=0;
    Owner* free_owner_=nullptr;
    uint32_t state_=MT2FW_MT2_ROUTE_PRISTINE,initial_protection_=0;
    uint64_t generation_=0;
    uintptr_t replacement_=0;
    Snapshot proof_{};
    Exchange last_{};
    bool live_confirmed_=false,retained_=false;
    std::string cause_,reported_;
    uint64_t repeated_=0;
    std::vector<std::pair<Plugin*,std::string>> errors_;
    bool authenticate(Snapshot&,std::string&);
    void receipt(MT2FW_MT2_FreeReceipt&);
    MT2FW_MT2_Result failed(Owner&,const std::string&,MT2FW_MT2_FreeReceipt*);
    MT2FW_MT2_Result disable_free(Owner&,MT2FW_MT2_FreeReceipt*);
    void report(Owner&,const std::string&);
    static Owner* owner(void* context){auto result=static_cast<Owner*>(context);return result&&result->active?result:nullptr;}
    template<class Function>static MT2FW_MT2_Result boundary(void* context,Function function) noexcept {
        try{return function();}catch(const std::exception& problem){boundary_error(context,problem.what());return MT2FW_MT2_FAILED;}
        catch(...){boundary_error(context,"Unknown native ownership exception");return MT2FW_MT2_FAILED;}
    }
    static void boundary_error(void*,const char*) noexcept;
    static MT2FW_MT2_Result MT2FW_CALL free_prepare(void*,MT2FW_MT2_FreeFn,MT2FW_MT2_FreeReceipt*);
    static MT2FW_MT2_Result MT2FW_CALL free_enable(void*,MT2FW_MT2_FreeReceipt*);
    static MT2FW_MT2_Result MT2FW_CALL free_disable(void*,MT2FW_MT2_FreeReceipt*);
    static MT2FW_MT2_Result MT2FW_CALL free_read(void*,MT2FW_MT2_FreeReceipt*);
    static MT2FW_MT2_Result MT2FW_CALL free_remove(void*,MT2FW_MT2_FreeReceipt*);
    static uint64_t MT2FW_CALL command_subscribe(void*,MT2FW_MT2_CommandObserver,void*);
    static MT2FW_MT2_Result MT2FW_CALL command_unsubscribe(void*,uint64_t);
    static uint64_t MT2FW_CALL input_subscribe(void*,MT2FW_MT2_InputObserver,void*);
    static MT2FW_MT2_Result MT2FW_CALL input_unsubscribe(void*,uint64_t);
};
NativeOwnership::Backend windows_free_backend(uintptr_t image,const fs::path& executable);
}
