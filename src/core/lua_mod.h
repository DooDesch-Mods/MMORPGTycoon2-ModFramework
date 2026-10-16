#pragma once
#include "mt2fw/core.h"
struct lua_State;
namespace mt2fw {
class LuaMod {
public:
    explicit LuaMod(Plugin& plugin);
    ~LuaMod();
    void load(const fs::path& entry);
    void invoke(int reference, const Json& value);
    lua_State* state() const { return state_; }
private:
    Plugin& plugin_;
    lua_State* state_ = nullptr;
    size_t allocated_ = 0;
    int instructions_ = 0;
    static void* allocate(void* user, void* pointer, size_t before, size_t after);
    void budget();
    static int on(lua_State* state);
    static int off(lua_State* state);
    static int call(lua_State* state);
    friend struct LuaBridge;
};
}
