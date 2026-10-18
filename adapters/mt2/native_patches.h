#pragma once
#include "sdk/native_patches.h"
#include "mt2fw/core.h"
#include <mutex>
#include <stdexcept>

namespace mt2fw::mt2 {
class NativePatches {
public:
    using Thread = std::function<uint64_t()>;
    using WorldProof = std::function<bool(uint64_t&,std::string&)>;
    NativePatches(Runtime&,Thread,WorldProof);
    void set_game_thread(uint64_t thread);
    MT2FW_MT2_Result query(const MT2FW_Host*,uint32_t,MT2FW_MT2_PatchesV1*,uint32_t);
    bool retire(Plugin&);
    bool busy() const;
    bool authorize(const std::string&,std::string&);
    Json status() const;
private:
    struct Owner {
        NativePatches* service=nullptr;
        Plugin* plugin=nullptr;
        bool active=true;
        size_t inflight=0;
        std::map<std::string,bool> patches;
    };
    struct Provider {
        Owner* owner=nullptr;
        MT2FW_MT2_SessionProvider callback=nullptr;
        void* user=nullptr;
        uint64_t token=0;
        bool active=false;
    };
    Runtime& runtime_;
    Thread thread_;
    WorldProof world_proof_;
    mutable std::recursive_mutex gate_;
    uint64_t game_thread_=0,next_token_=1;
    std::map<Plugin*,std::unique_ptr<Owner>> owners_;
    Provider provider_;
    uint32_t last_role_=MT2FW_MT2_SESSION_UNKNOWN;
    std::string last_reason_="Session authority has not been checked";
    static constexpr const char* bridge_id="doodesch.multiplayer";
    bool on_thread() const;
    static NativePatches& service(void* context){auto owner=static_cast<Owner*>(context);if(!owner||!owner->service)throw std::runtime_error("Native patch context is absent");return *owner->service;}
    Owner& admit(void*);
    static MT2FW_MT2_Result receipt(MT2FW_MT2_PatchReceipt*,MT2FW_MT2_Result,const std::string&);
    template<class F>static MT2FW_MT2_Result boundary(MT2FW_MT2_PatchReceipt* out,F function) noexcept {
        if(!out||out->size<sizeof(*out))return MT2FW_MT2_INVALID;
        try{return receipt(out,function(),"");}
        catch(const std::exception& error){return receipt(out,MT2FW_MT2_FAILED,error.what());}
        catch(...){return receipt(out,MT2FW_MT2_FAILED,"Unknown native patch service exception");}
    }
    MT2FW_MT2_Result add(Owner&,const MT2FW_MT2_PatchSpec*);
    MT2FW_MT2_Result remove(Owner&,const char*);
    MT2FW_MT2_Result bind(Owner&,MT2FW_MT2_SessionProvider,void*,uint64_t*);
    MT2FW_MT2_Result unbind(Owner&,uint64_t);
    bool authority_result(uint32_t,const std::string&,std::string&);
};
}
