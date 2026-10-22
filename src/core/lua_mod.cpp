#include "lua_mod.h"
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
int mt2fw_lua_budget_error(lua_State* state);
}
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <fstream>

namespace mt2fw {
namespace {
LuaMod& self(lua_State* state) { return **static_cast<LuaMod**>(lua_getextraspace(state)); }
void push(lua_State* state,const Json& value) {
    if(value.is_null())lua_pushnil(state);
    else if(value.is_boolean())lua_pushboolean(state,value.get<bool>());
    else if(value.is_number_integer())lua_pushinteger(state,value.get<int64_t>());
    else if(value.is_number())lua_pushnumber(state,value.get<double>());
    else if(value.is_string()){auto text=value.get<std::string>();lua_pushlstring(state,text.data(),text.size());}
    else {lua_newtable(state);if(value.is_array()){int index=1;for(auto& item:value){push(state,item);lua_rawseti(state,-2,index++);}}else for(auto it=value.begin();it!=value.end();++it){push(state,it.value());lua_setfield(state,-2,it.key().c_str());}}
}
Json pull(lua_State* state,int index,int depth=0) {
    if(depth>32)throw std::runtime_error("Lua value exceeds maximum JSON depth (32), or contains a cycle");
    index=lua_absindex(state,index);
    switch(lua_type(state,index)) {
    case LUA_TNIL:return nullptr;
    case LUA_TBOOLEAN:return bool(lua_toboolean(state,index));
    case LUA_TNUMBER:if(lua_isinteger(state,index))return lua_tointeger(state,index);else {auto value=lua_tonumber(state,index);if(!std::isfinite(value))throw std::runtime_error("API numbers must be finite");return value;}
    case LUA_TSTRING:{size_t length=0;auto text=lua_tolstring(state,index,&length);return std::string(text,length);}
    case LUA_TTABLE:{
        size_t length=lua_rawlen(state,index), count=0;bool array=true;Json object=Json::object();
        lua_pushnil(state);
        while(lua_next(state,index)) {
            try {
                if(lua_isinteger(state,-2)) {auto key=lua_tointeger(state,-2);if(key<1||uint64_t(key)>length)throw std::runtime_error("Lua arrays must have contiguous positive integer keys");}
                else if(lua_type(state,-2)==LUA_TSTRING)array=false;
                else throw std::runtime_error("JSON keys must be strings or array indices");
                ++count; if(count>16384)throw std::runtime_error("Lua table exceeds API item budget (16384)");
                if(lua_type(state,-2)==LUA_TSTRING)object[lua_tostring(state,-2)]=pull(state,-1,depth+1);
            }catch(...){lua_pop(state,2);throw;}
            lua_pop(state,1);
        }
        if(!array) {if(count!=object.size())throw std::runtime_error("Mixed arrays and object keys are unsupported");return object;}
        if(!length)return Json::object();
        Json result=Json::array();for(size_t i=1;i<=length;++i){lua_rawgeti(state,index,i);try{result.push_back(pull(state,-1,depth+1));}catch(...){lua_pop(state,1);throw;}lua_pop(state,1);}return result;
    }
    default:throw std::runtime_error("API values must be JSON-compatible");
    }
}
const std::string library=R"lua(
local invoke = _mt2_call
local patch_add = _mt2_patch_add
local patch_remove = _mt2_patch_remove
local require_local = require
require = function(name)
    local module, problem = require_local(name)
    if problem then error(problem, 2) end
    return module
end
local function call(operation, arguments)
    local result, problem = invoke(operation, arguments or {})
    if problem then error(problem, 2) end
    return result
end
mt2 = {events = {on = _mt2_on, off = _mt2_off}}
mt2.log = {}
for _, level in ipairs({'debug','info','warn','error'}) do
    mt2.log[level] = function(message) return call('log',{level=level,message=tostring(message)}) end
end
print = function(...) local parts={} for i=1,select('#',...) do parts[i]=tostring(select(i,...)) end mt2.log.info(table.concat(parts,'\t')) end
mt2.config = {
    define=function(key,schema) return call('config.define',{key=key,schema=schema}) end,
    get=function(key) return call('config.get',{key=key}) end,
    set=function(key,value) return call('config.set',{key=key,value=value}) end
}
mt2.storage = {
    get=function(key,default,scope) return call('storage.get',{key=key,default=default,scope=scope or 'profile'}) end,
    set=function(key,value,scope) return call('storage.set',{key=key,value=value,scope=scope or 'profile'}) end
}
mt2.world = {
    status=function() return call('world.status') end,
    region=function(x,z) return call('world.region',{x=x,z=z}) end,
    simulation=function() return call('world.simulation') end,
    software=function() return call('world.software') end
}
mt2.objects = {
    list=function(limit) return call('objects.list',{limit=limit or 100}) end,
    get=function(handle) return call('objects.get',{handle=handle}) end,
    building=function(handle) return call('objects.building',{handle=handle}) end,
    selected=function() return call('objects.selected') end
    ,persist=function(handle) return call('objects.persist',{handle=handle}) end
    ,resolve=function(id) return call('objects.resolve',{id=id}) end
}
mt2.camera = {
    get=function() return call('camera.get') end,
    focus=function(x,y,z,distance) return call('camera.focus',{x=x,y=y,z=z,distance=distance}) end
}
mt2.time = {
    get=function() return call('time.get') end,
    set=function(paused,speed) return call('time.set',{paused=paused,speed=speed or 1}) end
}
mt2.actions = {
    place=function(definition,x,y,z) return call('actions.place',{definition=definition,x=x,y=y,z=z}) end,
    move=function(handle,x,y,z) return call('actions.move',{handle=handle,x=x,y=y,z=z}) end,
    remove=function(handle) return call('actions.remove',{handle=handle}) end,
    status=function(id) return call('actions.status',{id=id}) end
}
mt2.input = {
    bind=function(name,scancode,callback)
        call('input.bind',{key=name,scancode=scancode})
        return mt2.events.on('input',function(event) if event.name==name and event.owner==_mt2_owner then callback(event) end end)
    end
}
mt2.ui = {
    app=function(options) return call('ui.app',options) end,
    native=function(options) return call('ui.native',options) end,
    document=function(document) return call('ui.document',{document=document}) end,
    text=function(text) return call('ui.text',{text=tostring(text)}) end,
    button=function(label) return call('ui.button',{label=label}) end,
    checkbox=function(label,value) return call('ui.checkbox',{label=label,value=value}) end,
    number=function(label,value,min,max) return call('ui.number',{label=label,value=value,min=min,max=max}) end,
    window=function(title,draw)
        return mt2.events.on('render',function()
            local visible=call('ui.begin',{title=title})
            local ok,problem=true,nil
            if visible then ok,problem=pcall(draw) end
            call('ui.end')
            if not ok then error(problem,0) end
        end)
    end
}
mt2.content = {register=function(definitions) return call('content.register',{definitions=definitions}) end}
mt2.patches = {
    add = function(spec,callbacks)
        local result,problem=patch_add(spec,callbacks)
        if problem then error(problem,2) end
        return result
    end,
    remove = function(id)
        local result,problem=patch_remove(id)
        if problem then error(problem,2) end
        return result
    end,
    targets = function() return call('patches.targets') end,
    status = function() return call('patches.status') end
}
)lua" +
#include "gameplay_lua.inc"
+ R"lua(
_mt2_call=nil; _mt2_on=nil; _mt2_off=nil; _mt2_patch_add=nil; _mt2_patch_remove=nil
)lua";
}
struct LuaBridge {
    struct Invocation{LuaMod* mod;int reference;const Json* value;};
    static int invoke(lua_State* state){auto& call=*static_cast<Invocation*>(lua_touserdata(state,1));lua_rawgeti(state,LUA_REGISTRYINDEX,call.reference);push(state,*call.value);lua_call(state,1,0);return 0;}
    static int invoke_value(lua_State* state){auto& call=*static_cast<Invocation*>(lua_touserdata(state,1));lua_rawgeti(state,LUA_REGISTRYINDEX,call.reference);push(state,*call.value);lua_call(state,1,1);return 1;}
    static void hook(lua_State* state,lua_Debug*) {
        auto& mod=self(state); mod.instructions_-=10000;
        if(mod.instructions_<=0)mt2fw_lua_budget_error(state);
    }
    static int require(lua_State* state) {
        const char* module=luaL_checkstring(state,1);std::string problem;
        try {
            auto& mod=self(state); std::string name(module);
            if(name.empty()||name.size()>128||name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.")!=std::string::npos||name.find("..")!=std::string::npos)
                throw std::runtime_error("require expects a local dotted module name");
            lua_getfield(state,LUA_REGISTRYINDEX,"mt2fw.modules");lua_getfield(state,-1,module);
            if(!lua_isnil(state,-1))return 1;lua_pop(state,1);
            std::replace(name.begin(),name.end(),'.','/'); auto path=confined(mod.plugin_.directory,name+".lua");
            if(fs::file_size(path)>1024*1024)throw std::runtime_error("Lua module exceeds 1 MiB");
            std::ifstream stream(path,std::ios::binary);std::string source(std::istreambuf_iterator<char>{stream},{});
            if(!stream&&!stream.eof())throw std::runtime_error("Lua module read failed");
            auto chunk="@"+path_text(path);
            if(luaL_loadbufferx(state,source.data(),source.size(),chunk.c_str(),"t")!=LUA_OK||lua_pcall(state,0,1,0)!=LUA_OK)throw std::runtime_error(lua_tostring(state,-1));
            if(lua_isnil(state,-1)){lua_pop(state,1);lua_pushboolean(state,true);}
            lua_pushvalue(state,-1);lua_setfield(state,-3,module);return 1;
        }catch(const std::exception& error){problem=error.what();}
        lua_pushnil(state);lua_pushlstring(state,problem.data(),problem.size());return 2;
    }
};
void* LuaMod::allocate(void* user,void* pointer,size_t before,size_t after) {
    auto& mod=*static_cast<LuaMod*>(user);if(!pointer)before=0;
    if(after==0){mod.allocated_-=before;std::free(pointer);return nullptr;}
    if(after>64*1024*1024 || mod.allocated_-before>64*1024*1024-after)return nullptr;
    void* result=std::realloc(pointer,after);if(result)mod.allocated_=mod.allocated_-before+after;return result;
}
LuaMod::LuaMod(Plugin& plugin):plugin_(plugin) {
    state_=lua_newstate(allocate,this);if(!state_)throw std::runtime_error("Cannot allocate Lua state");
    *static_cast<LuaMod**>(lua_getextraspace(state_))=this;
    const luaL_Reg libraries[]={{LUA_GNAME,luaopen_base},{LUA_TABLIBNAME,luaopen_table},{LUA_STRLIBNAME,luaopen_string},{LUA_MATHLIBNAME,luaopen_math},{LUA_UTF8LIBNAME,luaopen_utf8},{nullptr,nullptr}};
    for(auto* lib=libraries;lib->name;++lib){luaL_requiref(state_,lib->name,lib->func,1);lua_pop(state_,1);}
    for(const char* name:{"dofile","loadfile","load"}){lua_pushnil(state_);lua_setglobal(state_,name);}
    lua_newtable(state_);lua_setfield(state_,LUA_REGISTRYINDEX,"mt2fw.modules");
    lua_pushcfunction(state_,LuaBridge::require);lua_setglobal(state_,"require");
    lua_pushcfunction(state_,on);lua_setglobal(state_,"_mt2_on");
    lua_pushcfunction(state_,off);lua_setglobal(state_,"_mt2_off");
    lua_pushcfunction(state_,call);lua_setglobal(state_,"_mt2_call");
    lua_pushcfunction(state_,patch_add);lua_setglobal(state_,"_mt2_patch_add");
    lua_pushcfunction(state_,patch_remove);lua_setglobal(state_,"_mt2_patch_remove");
    lua_pushstring(state_,plugin.manifest.id.c_str());lua_setglobal(state_,"_mt2_owner");
    budget();
    if(luaL_loadbufferx(state_,library.data(),library.size(),"@mt2fw/api.lua","t")!=LUA_OK||lua_pcall(state_,0,0,0)!=LUA_OK){auto error=std::string(lua_tostring(state_,-1));lua_close(state_);state_=nullptr;throw std::runtime_error(error);}
}
LuaMod::~LuaMod(){if(state_)lua_close(state_);}
void LuaMod::budget(){
    {std::lock_guard lock(references_gate_);for(auto reference:released_references_)luaL_unref(state_,LUA_REGISTRYINDEX,reference);released_references_.clear();}
    instructions_=1000000;lua_sethook(state_,LuaBridge::hook,LUA_MASKCOUNT,10000);
}
void LuaMod::load(const fs::path& path) {
    if(fs::file_size(path)>1024*1024)throw std::runtime_error("Lua entry exceeds 1 MiB");
    std::ifstream stream(path,std::ios::binary);std::string source(std::istreambuf_iterator<char>{stream},{});
    if(!stream&&!stream.eof())throw std::runtime_error("Lua entry read failed: "+path_text(path));
    budget();auto chunk="@"+path_text(path);
    if(luaL_loadbufferx(state_,source.data(),source.size(),chunk.c_str(),"t")!=LUA_OK||lua_pcall(state_,0,0,0)!=LUA_OK){auto error=std::string(lua_tostring(state_,-1));lua_pop(state_,1);throw std::runtime_error(error);}
}
void LuaMod::invoke(int reference,const Json& value) {
    int top=lua_gettop(state_);budget();if(!lua_checkstack(state_,3))throw std::runtime_error("Lua callback stack memory budget exceeded");
    LuaBridge::Invocation call{this,reference,&value};lua_pushcfunction(state_,LuaBridge::invoke);lua_pushlightuserdata(state_,&call);
    if(lua_pcall(state_,1,0,0)!=LUA_OK){auto text=lua_tostring(state_,-1);auto error=std::string(text?text:"Lua callback failed with a non-string error value");lua_settop(state_,top);throw std::runtime_error(error);}
    lua_settop(state_,top);
}
Json LuaMod::invoke_value(int reference,const Json& value) {
    int top=lua_gettop(state_);budget();if(!lua_checkstack(state_,3))throw std::runtime_error("Lua callback stack memory budget exceeded");
    LuaBridge::Invocation call{this,reference,&value};lua_pushcfunction(state_,LuaBridge::invoke_value);lua_pushlightuserdata(state_,&call);
    if(lua_pcall(state_,1,1,0)!=LUA_OK){auto text=lua_tostring(state_,-1);auto error=std::string(text?text:"Lua patch callback failed with a non-string error value");lua_settop(state_,top);throw std::runtime_error(error);}
    try{auto value=pull(state_,-1);lua_settop(state_,top);return value;}catch(...){lua_settop(state_,top);throw;}
}
void LuaMod::release_reference(int reference){std::lock_guard lock(references_gate_);released_references_.push_back(reference);}
bool LuaMod::patch_thread(const Json& value) {
    if(value.value("lua_allowed",value.value("target",std::string())=="api.time.set"))return true;
    if(!patch_thread_warned_.exchange(true))std::fprintf(stderr,"warn: Lua patch %s skipped: native invocation is outside the established game thread\n",plugin_.manifest.id.c_str());
    return false;
}
namespace {
struct LuaPatchReference {
    LuaMod* mod;
    int reference;
    ~LuaPatchReference(){mod->release_reference(reference);}
};
}
int LuaMod::patch_add(lua_State* state) {
    luaL_checktype(state,1,LUA_TTABLE);luaL_checktype(state,2,LUA_TTABLE);auto& mod=self(state);
    {
    std::string problem;
    try{
        auto spec=PatchSpec::parse(pull(state,1));PatchCallbacks callbacks;
        lua_pushnil(state);while(lua_next(state,2)){
            bool known=lua_type(state,-2)==LUA_TSTRING&&(std::strcmp(lua_tostring(state,-2),"before")==0||std::strcmp(lua_tostring(state,-2),"after")==0);
            lua_pop(state,1);if(!known){lua_pop(state,1);throw std::runtime_error("PATCH_CALLBACK: unknown callback field");}
        }
        for(const char* phase:{"before","after"}){
            lua_getfield(state,2,phase);
            if(lua_isnil(state,-1)){lua_pop(state,1);continue;}
            if(!lua_isfunction(state,-1)){lua_pop(state,1);throw std::runtime_error(std::string("PATCH_CALLBACK: ")+phase+" must be a function");}
            auto reference=std::make_shared<LuaPatchReference>();reference->mod=&mod;reference->reference=luaL_ref(state,LUA_REGISTRYINDEX);
            auto label="Patch "+mod.plugin_.manifest.id+":"+spec.id+" "+spec.target;
            if(std::strcmp(phase,"before")==0)callbacks.before=[reference,label,mode=spec.mode](const PatchCall& call)->Json{
                if(!reference->mod->patch_thread(call.data))return nullptr;
                if(!reference->mod->plugin_.active)return nullptr;
                try{auto value=reference->mod->invoke_value(reference->reference,call.data);PatchRegistry::validate_values(mode,value);return value;}
                catch(const std::exception& error){auto& plugin=reference->mod->plugin_;plugin.runtime->disable(plugin,label+" before failed: "+error.what());throw;}
            };
            else callbacks.after=[reference,label](const PatchCall& call){if(reference->mod->patch_thread(call.data)&&reference->mod->plugin_.active)try{reference->mod->invoke(reference->reference,call.data);}
                catch(const std::exception& error){auto& plugin=reference->mod->plugin_;plugin.runtime->disable(plugin,label+" after failed: "+error.what());throw;}
            };
        }
        auto id=spec.id;mod.plugin_.runtime->add_patch(mod.plugin_,std::move(spec),std::move(callbacks));lua_pushlstring(state,id.data(),id.size());return 1;
    }catch(const std::exception& error){problem=error.what();}
    lua_pushnil(state);lua_pushlstring(state,problem.data(),problem.size());
    }
    return 2;
}
int LuaMod::patch_remove(lua_State* state) {
    const char* id=luaL_checkstring(state,1);auto& mod=self(state);
    {
    std::string problem;
    try{lua_pushboolean(state,mod.plugin_.runtime->remove_patch(mod.plugin_,id));return 1;}catch(const std::exception& error){problem=error.what();}
    lua_pushnil(state);lua_pushlstring(state,problem.data(),problem.size());
    }
    return 2;
}
int LuaMod::on(lua_State* state) {
    const char* event=luaL_checkstring(state,1);luaL_checktype(state,2,LUA_TFUNCTION);
    auto& mod=self(state);
    {
    std::string error;
    try{lua_pushvalue(state,2);int reference=luaL_ref(state,LUA_REGISTRYINDEX);auto token=mod.plugin_.runtime->subscribe(mod.plugin_,event,[&mod,reference](const Json& value){mod.invoke(reference,value);});lua_pushinteger(state,token);return 1;}
    catch(const std::exception& problem){error=problem.what();}
    lua_pushlstring(state,error.data(),error.size());
    }
    return lua_error(state);
}
int LuaMod::off(lua_State* state){auto token=luaL_checkinteger(state,1);auto& mod=self(state);mod.plugin_.runtime->unsubscribe(mod.plugin_,token);return 0;}
int LuaMod::call(lua_State* state) {
    const char* op=luaL_checkstring(state,1);auto& mod=self(state);
    try {auto result=mod.plugin_.runtime->request(mod.plugin_,op,pull(state,2));push(state,result);return 1;}
    catch(const std::exception& error){lua_pushnil(state);lua_pushstring(state,error.what());return 2;}
}
}
