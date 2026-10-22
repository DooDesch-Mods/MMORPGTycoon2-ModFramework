#pragma once
#include "sdk/native_routes.h"
#include "sdk/native_ownership.h"
#include "mt2fw/core.h"
#include <array>
#include <atomic>
#include <mutex>
#include <cstdio>

namespace mt2fw::mt2 {
class NativeRoutes {
public:
    using Bytes=std::array<unsigned char,32>;
    struct Backend {
        std::function<bool(uint64_t,uint32_t,void*,size_t,std::string&)> pristine;
        std::function<bool(uintptr_t,void*,size_t,std::string&)> read;
        std::function<bool(void*,void*,void**,std::string&)> create;
        std::function<bool(void*,void*,std::string&)> enable;
        std::function<bool(void*,void*,std::string&)> prove;
        std::function<bool(void*,std::string&)> remove;
        std::function<bool(void*,std::string&)> disable;
    };
    using CreateOriginal=void*(*)(void*,const void*,const void*,const void*,void*,void*,void*,bool);
    using RemoveOriginal=void(*)(void*,void*);
    explicit NativeRoutes(std::function<Plugin*(const MT2FW_Host*)> lookup);
    void configure(uintptr_t image,Backend backend);
    void set_game_thread(uint64_t thread){game_thread_.store(thread);}
    void set_patch_runtime(Runtime* runtime){std::lock_guard lock(gate_);patch_runtime_=runtime;}
    bool detach_patch_runtime(){std::lock_guard lock(gate_);patch_runtime_=nullptr;return patch_inflight_==0;}
    void catalog(const std::string& name,uint64_t rva,bool shared=false);
    bool verify_body(uint64_t rva,size_t bytes,const std::string& sha256,std::string& cause);
    bool publish_shared(const std::string& name,void* detour,void* trampoline,std::string& cause);
    bool publish_framework_hook(void* target,void* detour,void* trampoline,std::string& cause);
    MT2FW_MT2_Result query(const MT2FW_Host*,uint32_t,MT2FW_MT2_RoutesV1*,uint32_t);
    void* create(const MT2FW_MT2_CreateCall&,CreateOriginal);
    void remove(const MT2FW_MT2_RemoveCall&,RemoveOriginal);
    bool retire(Plugin&);
    bool busy() const;
    bool has_retention() const;
    bool retirement_blocked() const;
    MT2FW_MT2_Result disable(Plugin&,const char*,MT2FW_MT2_RouteReceipt*);
    MT2FW_MT2_Result resume(Plugin&,const char*,MT2FW_MT2_RouteReceipt*);
    std::vector<std::pair<Plugin*,std::string>> drain_errors();
private:
    struct Owner {NativeRoutes* service=nullptr;Plugin* plugin=nullptr;std::atomic<bool> active{true};size_t inflight=0;std::string last_error;uint64_t suppressed_errors=0;};
    struct CreatePair {uint64_t token;Owner* owner;MT2FW_MT2_CreateBefore before;MT2FW_MT2_CreateAfter after;void* user;bool active=true;};
    struct Authority {uint64_t token=0;Owner* owner=nullptr;MT2FW_MT2_RemoveAuthority callback=nullptr;void* user=nullptr;bool active=false;};
    // current retains only this entry's patch. Receipts compose acknowledged
    // neighbours without changing this owner's generation or pristine bytes.
    struct Target {uint64_t rva;bool shared;uint32_t state=MT2FW_MT2_ROUTE_PRISTINE;uint64_t generation=0;Owner* owner=nullptr;void* detour=nullptr;void* trampoline=nullptr;Bytes pristine{},current{};std::string cause;bool created=false,retained=false;uint32_t patch_size=0;uint64_t patch_generation=0;};
    struct Continuation {void* manager;void* array;RemoveOriginal original;bool attempted=false,returned=false,duplicate=false;std::exception_ptr exception;};
    std::function<Plugin*(const MT2FW_Host*)> lookup_;
    Backend backend_;
    uintptr_t image_=0;
    mutable std::recursive_mutex gate_;
    std::map<Plugin*,std::unique_ptr<Owner>> owners_;
    std::map<std::string,Target> targets_;
    std::vector<CreatePair> create_pairs_;
    Authority authority_;
    std::vector<std::pair<Plugin*,std::string>> errors_;
    uint64_t next_token_=1;
    std::atomic<uint64_t> next_invocation_{1},game_thread_{0};
    Runtime* patch_runtime_=nullptr;
    size_t patch_inflight_=0;
    struct PatchLease {
        NativeRoutes& service;
        Runtime* runtime=nullptr;
        explicit PatchLease(NativeRoutes&);
        ~PatchLease();
        PatchLease(const PatchLease&)=delete;
        PatchLease& operator=(const PatchLease&)=delete;
    };
    static Owner* owner(void*);
    void report(Owner&,const std::string&);
    static void boundary_error(void* context,const char* cause) noexcept {
        auto value=static_cast<Owner*>(context);
        try{if(value)value->service->report(*value,cause);else std::fprintf(stderr,"Native routes: %s\n",cause);}
        catch(const std::exception& logging){std::fprintf(stderr,"Native routes: %s; logging failed: %s\n",cause,logging.what());}
        catch(...){std::fprintf(stderr,"Native routes: %s; logging failed with unknown native exception\n",cause);}
    }
    template<class Result,class Function>static Result boundary(void* context,Result failure,Function function) noexcept {
        try{return function();}catch(const std::exception& problem){boundary_error(context,problem.what());return failure;}
        catch(...){boundary_error(context,"Unknown native route exception");return failure;}
    }
    bool validate(Target&,std::string&);
    bool compose(const Target&,Bytes&,std::string&,const Target* omitted=nullptr,const Target* retiring=nullptr) const;
    bool accept_enabled(Target&,const Bytes&,std::string&);
    bool accept_restored(Target&,std::string&);
    void receipt(const Target&,MT2FW_MT2_RouteReceipt&);
    static uint64_t MT2FW_CALL create_subscribe(void*,MT2FW_MT2_CreateBefore,MT2FW_MT2_CreateAfter,void*);
    static MT2FW_MT2_Result MT2FW_CALL create_unsubscribe(void*,uint64_t);
    static MT2FW_MT2_Result MT2FW_CALL remove_bind(void*,MT2FW_MT2_RemoveAuthority,void*,uint64_t*);
    static MT2FW_MT2_Result MT2FW_CALL remove_unbind(void*,uint64_t);
    static MT2FW_MT2_Result MT2FW_CALL next_remove(void*);
    static MT2FW_MT2_Result MT2FW_CALL hook_prepare(void*,const char*,void*,MT2FW_MT2_RouteReceipt*);
    static MT2FW_MT2_Result MT2FW_CALL hook_enable(void*,const char*,MT2FW_MT2_RouteReceipt*);
    static MT2FW_MT2_Result MT2FW_CALL hook_remove(void*,const char*,MT2FW_MT2_RouteReceipt*);
    static MT2FW_MT2_Result MT2FW_CALL route_read(void*,const char*,MT2FW_MT2_RouteReceipt*);
    static MT2FW_MT2_Result MT2FW_CALL code_verify(void*,const char*,uint32_t,const void*,uint32_t,MT2FW_MT2_RouteReceipt*);
};
}
