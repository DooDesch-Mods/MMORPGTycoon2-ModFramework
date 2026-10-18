#pragma once
#include "mt2fw/core.h"
#include <atomic>
struct lua_State;
namespace mt2fw {
class LuaMod {
public:
    explicit LuaMod(Plugin& plugin);
    ~LuaMod();
    void load(const fs::path& entry);
    void invoke(int reference, const Json& value);
    Json invoke_value(int reference,const Json& value);
    void release_reference(int reference);
    lua_State* state() const { return state_; }
private:
    Plugin& plugin_;
    lua_State* state_ = nullptr;
    size_t allocated_ = 0;
    int instructions_ = 0;
    std::atomic<bool> patch_thread_warned_{false};
    std::mutex references_gate_;
    std::vector<int> released_references_;
    bool patch_thread(const Json&);
    static void* allocate(void* user, void* pointer, size_t before, size_t after);
    void budget();
    static int on(lua_State* state);
    static int off(lua_State* state);
    static int call(lua_State* state);
    static int patch_add(lua_State*);
    static int patch_remove(lua_State*);
    friend struct LuaBridge;
};
}
