#pragma once
#include "sdk/native_services.h"
#include "mt2fw/core.h"
#include <array>
#include <atomic>
#include <mutex>

namespace mt2fw::mt2 {
class NativeServices {
public:
    using Lookup = std::function<Plugin*(const MT2FW_Host*)>;
    using Install = std::function<bool(void*,void*,void**,std::string&)>;
    using Remove = std::function<bool(void*,std::string&)>;
    NativeServices(fs::path game,fs::path profile,Lookup lookup);
    void backend(Install install,Remove remove);
    void reserve(const std::string& name);
    void target(const std::string& name,void* address);
    MT2FW_MT2_Result query(const MT2FW_Host*,uint32_t,MT2FW_MT2_ServiceV1*,uint32_t);
    bool retire(Plugin& owner);
    void begin_frame(void* game,float dt);
    void end_frame(bool original_returned);
    bool in_frame() const { return frame_depth_!=0; }
    bool has_hooks() const { return !hooks_.empty(); }
private:
    struct Owner {
        NativeServices* service;
        Plugin* plugin;
        std::atomic<bool> alive{true};
        std::string game,profile,game_data,directory,data;
    };
    struct Pair {uint64_t token;Owner* owner;MT2FW_MT2_FrameBegin begin;MT2FW_MT2_FrameEnd end;void* user;};
    struct FrameRun {MT2FW_MT2_Frame frame{sizeof(MT2FW_MT2_Frame)};std::array<Pair,128> entered{};size_t count=0;};
    struct Target {void* address=nullptr;bool reserved=false;};
    struct Hook {uint64_t token;Owner* owner;std::string name;};
    struct Section {Owner* owner=nullptr;std::string name;};
    fs::path game_,profile_;
    Lookup lookup_;
    Install install_;
    Remove remove_;
    std::map<Plugin*,std::unique_ptr<Owner>> owners_;
    std::map<std::string,Target> targets_;
    std::vector<Pair> pairs_;
    std::vector<Hook> hooks_;
    uint64_t next_token_=1;
    std::vector<std::unique_ptr<FrameRun>> frame_slots_;
    size_t frame_depth_=0;
    uint64_t next_frame_=0;
    std::array<Section,4096> sections_;
    std::atomic<uint32_t> section_count_{0};
    std::atomic<uint64_t> active_capture_{0},next_scope_{1},dropped_{0},current_frame_{0};
    std::mutex trace_gate_;
    std::vector<MT2FW_MT2_TraceEvent> trace_;
    size_t trace_head_=0,trace_count_=0;
    uint64_t capture_epoch_=0,sequence_=0;
    Owner* capture_owner_=nullptr;
    void record(uint32_t kind,uint32_t section,uint64_t token,uint64_t epoch) noexcept;
    static Owner* owner(void* context);
    static uint64_t MT2FW_CALL frame_subscribe(void*,MT2FW_MT2_FrameBegin,MT2FW_MT2_FrameEnd,void*);
    static MT2FW_MT2_Result MT2FW_CALL frame_unsubscribe(void*,uint64_t);
    static MT2FW_MT2_Result MT2FW_CALL hook_install(void*,const char*,void*,void**,uint64_t*);
    static MT2FW_MT2_Result MT2FW_CALL hook_remove(void*,uint64_t);
    static uint32_t MT2FW_CALL section_register(void*,const char*);
    static MT2FW_MT2_Scope MT2FW_CALL scope_begin(void*,uint32_t);
    static void MT2FW_CALL scope_end(void*,uint32_t,MT2FW_MT2_Scope);
    static MT2FW_MT2_Result MT2FW_CALL capture_start(void*,uint32_t,uint64_t*);
    static MT2FW_MT2_Result MT2FW_CALL capture_read(void*,MT2FW_MT2_TraceEvent*,uint32_t,MT2FW_MT2_TraceStatus*);
    static MT2FW_MT2_Result MT2FW_CALL capture_stop(void*);
    static MT2FW_MT2_Result MT2FW_CALL section_read(void*,uint32_t,MT2FW_MT2_Section*);
};
}
