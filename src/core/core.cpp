#include "mt2fw/core.h"
#include "mt2fw/save_metadata.h"
#include "lua_mod.h"
#include "crypto.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>

namespace mt2fw {
namespace {
using Clock = std::chrono::steady_clock;
constexpr size_t native_capacity = 4 * 1024 * 1024;
const std::set<std::string> allowed_capabilities = {"ui", "input", "world.read", "objects.read", "objects.write", "camera", "time.write", "content"};
const std::set<std::string> events = {"start", "stop", "world_ready", "world_leave", "saved", "update", "render", "input", "object_changed", "action_completed"};
struct Version {
    std::array<std::string,3> core;
    std::vector<std::string> prerelease;
    static int number(const std::string& a,const std::string& b) {if(a.size()!=b.size())return a.size()<b.size()?-1:1;return a.compare(b);}
    int compare(const Version& other)const {
        for(size_t i=0;i<3;++i)if(auto order=number(core[i],other.core[i]);order)return order;
        if(prerelease.empty()!=other.prerelease.empty())return prerelease.empty()?1:-1;
        for(size_t i=0;i<(std::min)(prerelease.size(),other.prerelease.size());++i) {
            const auto& a=prerelease[i];const auto& b=other.prerelease[i];
            bool an=a.find_first_not_of("0123456789")==std::string::npos,bn=b.find_first_not_of("0123456789")==std::string::npos;
            int order=an&&bn?number(a,b):an!=bn?(an?-1:1):a.compare(b);if(order)return order;
        }
        return prerelease.size()==other.prerelease.size()?0:prerelease.size()<other.prerelease.size()?-1:1;
    }
};
Version parse_version(const std::string& text) {
    static const std::regex pattern(R"((0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?(?:\+[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?)");
    std::smatch match;
    if (!std::regex_match(text, match, pattern)) throw std::runtime_error("Expected SemVer, got: " + text);
    Version result{{match[1],match[2],match[3]}, {}};
    if(match[4].matched){std::istringstream parts(match[4]);std::string part;while(std::getline(parts,part,'.')){
        if(part.size()>1&&part[0]=='0'&&part.find_first_not_of("0123456789")==std::string::npos)throw std::runtime_error("SemVer numeric prerelease identifier has a leading zero");
        result.prerelease.push_back(part);
    }}
    return result;
}
std::string text(const Json& object, const char* key) {
    if (!object.contains(key) || !object.at(key).is_string()) throw std::runtime_error(std::string("Missing string: ") + key);
    auto value = object.at(key).get<std::string>();
    if (value.empty() || value.size() > 256 || value.find('\0') != std::string::npos) throw std::runtime_error(std::string("Invalid string: ") + key);
    return value;
}
void valid_config(const Json& schema, const Json& value) {
    const auto type = schema.at("type").get<std::string>();
    bool valid = (type == "boolean" && value.is_boolean()) || (type == "string" && value.is_string()) ||
                 (type == "number" && value.is_number()) || (type == "integer" && value.is_number_integer());
    if (!valid) throw std::runtime_error("Setting has incorrect type: " + type);
    if (value.is_number() && ((schema.contains("min") && value < schema["min"]) || (schema.contains("max") && value > schema["max"])))
        throw std::runtime_error("Setting outside allowed range");
}
Json plugin_fingerprint(const Plugin& plugin) {
    Json files = Json::object();
    for (const auto& entry : fs::recursive_directory_iterator(plugin.directory)) {
        if (fs::is_symlink(entry.symlink_status())) throw std::runtime_error("Mod contains a symbolic link: " + path_text(entry.path()));
        if (entry.is_regular_file()) {
            auto relative = path_text(fs::relative(entry.path(), plugin.directory));
            if (!relative.starts_with(".vscode/") && !relative.starts_with(".git/")) files[relative] = file_digest(entry.path());
        }
    }
    return {{"id",plugin.manifest.id},{"version",plugin.manifest.version},{"sha256",digest(files.dump())},{"settings",plugin.config}};
}
size_t MT2FW_CALL native_request(void* context,const char* operation,const char* arguments,char* output,size_t capacity) {
    if (!output || capacity < native_capacity) return native_capacity;
    Json response;
    try {
        if (!context || !operation || !arguments) throw std::runtime_error("Native request requires context, operation and JSON arguments");
        auto& plugin = *static_cast<Plugin*>(context);
        response = {{"ok",true},{"result",plugin.runtime->request(plugin,operation,Json::parse(arguments))}};
    } catch (const std::exception& error) { response = {{"ok",false},{"error",error.what()}}; }
    auto encoded = response.dump();
    if (encoded.size()+1 > capacity) encoded = R"({"ok":false,"error":"Result exceeds ABI buffer; execution may have occurred; do not replay"})";
    std::memcpy(output, encoded.c_str(), encoded.size()+1);
    return encoded.size()+1;
}
uint64_t MT2FW_CALL native_subscribe(void* context,const char* event,MT2FW_EventCallback callback,void* user) {
    if (!context || !event || !callback) return 0;
    auto& plugin = *static_cast<Plugin*>(context);
    try { return plugin.runtime->subscribe(plugin,event,[callback,user](const Json& value) { auto bytes=value.dump(); callback(user,bytes.c_str(),bytes.size()); }); }
    catch (const std::exception& error) { plugin.runtime->log(plugin.manifest.id,"error",error.what()); return 0; }
}
void MT2FW_CALL native_unsubscribe(void* context,uint64_t token) {
    if (context) { auto& plugin=*static_cast<Plugin*>(context); plugin.runtime->unsubscribe(plugin,token); }
}
}
bool version_matches(const std::string& actual,const std::string& required) {
    auto current=parse_version(actual);
    if (required.starts_with("^")) {auto minimum=parse_version(required.substr(1));auto last=minimum.core[0]!="0"?0:minimum.core[1]!="0"?1:2;
        if(current.compare(minimum)<0)return false;for(int i=0;i<=last;++i)if(current.core[i]!=minimum.core[i])return false;return true;}
    if (required.starts_with(">=")) return current.compare(parse_version(required.substr(2)))>=0;
    return current.compare(parse_version(required))==0;
}
Manifest Manifest::parse(const Json& value) {
    if (!value.is_object()) throw std::runtime_error("mod.json must contain an object");
    Manifest m; m.source=value;
    m.id=text(value,"id"); m.name=text(value,"name"); m.version=text(value,"version");
    m.type=text(value,"type"); m.entry=text(value,"entry");
    static const std::regex id(R"([a-z][a-z0-9]*(\.[a-z0-9][a-z0-9_-]*)+)");
    if (!std::regex_match(m.id,id)) throw std::runtime_error("Mod ID must be lowercase and namespaced, for example author.my_mod");
    parse_version(m.version);
    if (!version_matches("1.1.0",">="+text(value,"api_version")) || parse_version(text(value,"api_version")).core[0]!="1")
        throw std::runtime_error("Mod requires an unsupported API version");
    if (m.type!="lua"&&m.type!="native"&&m.type!="content") throw std::runtime_error("Mod type must be lua, native or content");
    if (fs::path(m.entry).is_absolute() || m.entry.find(':')!=std::string::npos || m.entry.find("..")!=std::string::npos || m.entry.find('\\')!=std::string::npos)
        throw std::runtime_error("Entry must be a confined relative path with forward slashes");
    m.multiplayer=value.value("multiplayer", "singleplayer");
    if (m.multiplayer!="local"&&m.multiplayer!="shared"&&m.multiplayer!="singleplayer") throw std::runtime_error("Invalid multiplayer class");
    if (!value.contains("capabilities")||!value["capabilities"].is_array()) throw std::runtime_error("capabilities must be an array");
    for (const auto& item:value["capabilities"]) {
        auto cap=item.get<std::string>();
        if (!allowed_capabilities.contains(cap)) throw std::runtime_error("Unknown capability: "+cap);
        if (m.multiplayer=="local"&&(cap=="objects.write"||cap=="time.write"||cap=="content")) throw std::runtime_error("Local mods cannot change the shared world");
        m.capabilities.insert(cap);
    }
    if (value.contains("dependencies")) {
        if (!value["dependencies"].is_object()) throw std::runtime_error("dependencies must map IDs to versions");
        for (auto it=value["dependencies"].begin();it!=value["dependencies"].end();++it) {
            if (!std::regex_match(it.key(),id)) throw std::runtime_error("Invalid dependency ID: "+it.key());
            auto range=it.value().get<std::string>(); version_matches("1.0.0",range); m.dependencies[it.key()]=range;
        }
    }
    return m;
}
Plugin::Plugin(Runtime* owner):runtime(owner) { host={sizeof(MT2FW_Host),MT2FW_ABI_VERSION,this,native_request,native_subscribe,native_unsubscribe}; }
Plugin::~Plugin(){if(native_loaded&&native_shutdown&&!native_retirement_failed&&!native_retained&&!native_pins)native_shutdown();}
Runtime::Runtime(fs::path root,Adapter adapter):patches([this](const std::string& owner,const std::string& cause){queue_patch_error(owner,cause);}),root_(fs::absolute(root)),adapter_(std::move(adapter)) {
    fs::create_directories(root_/"mods");
    if (fs::exists(root_/"settings.json")) settings_=read_json(root_/"settings.json");
    if (!settings_.is_object()) throw std::runtime_error("settings.json must contain an object");
}
Runtime::~Runtime() { stop_plugins(); }
void Runtime::stop_plugins(){for(auto it=plugins_.rbegin();it!=plugins_.rend();++it)deactivate(**it);}
void Runtime::log(const std::string& mod,const std::string& level,const std::string& message) {
    auto bounded=message.substr(0,8192); logs_.push_back({mod,level,bounded}); if(logs_.size()>512)logs_.pop_front();
    fs::create_directories(root_/"logs"); auto path=root_/"logs"/"framework.jsonl";
    if (fs::exists(path)&&fs::file_size(path)>8*1024*1024) { auto old=root_/"logs"/"framework.previous.jsonl"; if(fs::exists(old))fs::remove(old); fs::rename(path,old); }
    std::ofstream stream(path,std::ios::app); stream<<Json({{"mod",mod},{"level",level},{"message",bounded}}).dump()<<'\n';
    if(!stream)throw std::runtime_error("Cannot append framework log: "+path_text(path));
}
std::unique_ptr<Plugin> Runtime::prepare(const Manifest& manifest,const fs::path& directory) {
    if(mod_notice)mod_notice(manifest,"Loading");
    auto p=std::make_unique<Plugin>(this); p->manifest=manifest; p->directory=directory;
    auto data=confined(root_,"data/"+manifest.id);
    if(fs::exists(data/"config.json"))p->config=read_json(data/"config.json");
    if(fs::exists(data/"profile.json"))p->profile_data=read_json(data/"profile.json");
    auto entry=confined(directory,manifest.entry);
    if(!fs::is_regular_file(entry))throw std::runtime_error("Entry file missing: "+path_text(entry));
    if(manifest.type=="lua") { p->lua=std::make_unique<LuaMod>(*p); p->lua->load(entry); }
    else if(manifest.type=="native") {
#ifdef _WIN32
        auto module=LoadLibraryExW(entry.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module)throw std::runtime_error("Native plugin load failed Win32="+std::to_string(GetLastError()));
        pinned_modules_.push_back(module); p->native_module=module;
        auto query=reinterpret_cast<MT2FW_QueryFn>(GetProcAddress(module,"MT2FW_PluginQuery"));
        auto load=reinterpret_cast<MT2FW_LoadFn>(GetProcAddress(module,"MT2FW_PluginLoad"));
        p->native_shutdown=reinterpret_cast<MT2FW_ShutdownFn>(GetProcAddress(module,"MT2FW_PluginShutdown"));
        if(!query||!load||!p->native_shutdown)throw std::runtime_error("Native plugin is missing required ABI exports");
        MT2FW_PluginInfo info{sizeof(info),MT2FW_ABI_VERSION,nullptr,nullptr};
        if(!query(&info)||info.size<sizeof(info)||info.abi_version!=MT2FW_ABI_VERSION)throw std::runtime_error("Native plugin info size or ABI version differs");
        if(!info.id||!info.version||manifest.id!=info.id||manifest.version!=info.version)throw std::runtime_error("Native metadata differs from mod.json");
        if(!load(&p->host)) { p->native_shutdown(); p->native_shutdown=nullptr; throw std::runtime_error("Native plugin initialization rejected"); }
        p->native_loaded=true;
#else
        throw std::runtime_error("Native plugins require Windows");
#endif
    } else if(!read_json(entry).is_array())throw std::runtime_error("Content entry must contain a list of definitions");
    p->active=true;
    dispatch(*p,"start",Json::object());
    p->active=false;
    if(!p->error.empty())throw std::runtime_error(p->error);
    p->fingerprint=plugin_fingerprint(*p);
    return p;
}
void Runtime::load() {
    if(!plugins_.empty())throw std::runtime_error("Runtime already loaded");
    std::map<std::string,std::vector<std::pair<Manifest,fs::path>>> found;
    for(const auto& item:fs::directory_iterator(root_/"mods")) {
        if(!item.is_directory())continue;
        try { auto directory=confined(root_/"mods",item.path().filename()); auto m=Manifest::parse(read_json(directory/"mod.json")); found[m.id].push_back({m,directory}); }
        catch(const std::exception& error){log(path_text(item.path().filename()),"error",error.what());}
    }
    std::set<std::string> complete;
    while(complete.size()<found.size()) {
        bool advanced=false;
        for(auto& [id,entries]:found) {
            if(complete.contains(id))continue;
            auto& [manifest,directory]=entries.front();
            bool waiting=false;
            for(auto& [dependency,range]:manifest.dependencies)if(found.contains(dependency)&&!complete.contains(dependency))waiting=true;
            if(waiting&&entries.size()==1)continue;
            auto failed=std::make_unique<Plugin>(this); failed->manifest=manifest; failed->directory=directory;
            try {
                if(entries.size()!=1)throw std::runtime_error("Duplicate mod ID in "+std::to_string(entries.size())+" directories");
                if(settings_.contains("disabled")&&settings_["disabled"].contains(id)&&settings_["disabled"][id]==true)throw std::runtime_error("Disabled; restart required to enable");
                if(multiplayer&&manifest.multiplayer=="singleplayer")throw std::runtime_error("Singleplayer mod cannot start in multiplayer");
                for(auto& [dependency,range]:manifest.dependencies) {
                    auto active=std::find_if(plugins_.begin(),plugins_.end(),[&](auto& p){return p->manifest.id==dependency&&p->active;});
                    if(active==plugins_.end()||!version_matches((*active)->manifest.version,range))throw std::runtime_error("Dependency unavailable: "+dependency+" "+range);
                }
                auto plugin=prepare(manifest,directory); activate(*plugin); plugins_.push_back(std::move(plugin));
            }catch(const std::exception& error){ failed->error=error.what(); log(id,"error",failed->error); plugins_.push_back(std::move(failed)); }
            complete.insert(id); advanced=true;
        }
        if(!advanced) {
            for(auto& [id,entries]:found)if(!complete.contains(id)) { auto p=std::make_unique<Plugin>(this); p->manifest=entries.front().first;p->directory=entries.front().second;p->error="Dependency cycle or dependency blocked by cycle";log(id,"error",p->error);plugins_.push_back(std::move(p));complete.insert(id); }
        }
    }
}
void Runtime::activate(Plugin& plugin) {
    patches.validate(&plugin,plugin.manifest.id,plugin.patch_registrations);
    plugin.staging=false; plugin.active=true;
    if(plugin.manifest.type=="content") request(plugin,"content.register",{{"definitions",read_json(confined(plugin.directory,plugin.manifest.entry))},{"directory",path_text(plugin.directory)}});
    else if(!plugin.content_definitions.empty())request(plugin,"content.register",{{"definitions",plugin.content_definitions}});
    atomic_json(confined(root_,"data/"+plugin.manifest.id+"/config.json"),plugin.config);
    if(!plugin.profile_data.empty())atomic_json(confined(root_,"data/"+plugin.manifest.id+"/profile.json"),plugin.profile_data);
    if(plugin.active&&!world_.empty())dispatch(plugin,"world_ready",{{"epoch",epoch},{"name",world_}});
    log(plugin.manifest.id,"info",plugin.active?"Mod started":"Mod start failed");
    if(mod_notice)mod_notice(plugin.manifest,plugin.active?"Active":"Failed");
    if(plugin.active){patches.replace(&plugin,plugin.manifest.id,plugin.patch_registrations);plugin.patch_staging=false;}
}
void Runtime::deactivate(Plugin& plugin) {
    if(plugin.active)dispatch(plugin,"stop",Json::object());
    plugin.active=false; plugin.subscriptions.clear(); plugin.hotkeys.clear();
    patches.retire(&plugin);plugin.patch_registrations.clear();
    if(native_retire)plugin.native_retirement_failed=!native_retire(plugin);
    if(plugin.native_loaded&&plugin.native_shutdown) {
        if(plugin.native_pins||plugin.native_retirement_failed||plugin.native_retained)plugin.native_shutdown_pending=true;
        else {plugin.native_shutdown();plugin.native_shutdown=nullptr;plugin.native_loaded=false;}
    }
}
void Runtime::disable(Plugin& plugin,const std::string& cause) {
    plugin.active=false; plugin.error=cause; plugin.subscriptions.clear(); plugin.hotkeys.clear(); patches.retire(&plugin);plugin.patch_registrations.clear();log(plugin.manifest.id,"error",cause);
    if(native_retire)plugin.native_retirement_failed=!native_retire(plugin);
    if(!plugin.staging&&!plugin.provisional)for(auto& other:plugins_)if(other->active&&other->manifest.dependencies.contains(plugin.manifest.id))disable(*other,"Dependency failed: "+plugin.manifest.id);
}
Plugin* Runtime::native_plugin(const MT2FW_Host* host) const {
    for(auto& plugin:plugins_)if(&plugin->host==host&&plugin->native_loaded&&plugin->active&&!plugin->staging)return plugin.get();
    return nullptr;
}
void Runtime::release_native(Plugin& plugin) {
    if(!plugin.native_pins)throw std::runtime_error("Native callback lease underflow: "+plugin.manifest.id);
    --plugin.native_pins;
    if(!plugin.native_pins&&plugin.native_shutdown_pending&&!plugin.native_retirement_failed&&!plugin.native_retained&&plugin.native_loaded&&plugin.native_shutdown){
        plugin.native_shutdown();plugin.native_shutdown=nullptr;plugin.native_loaded=false;plugin.native_shutdown_pending=false;
    }
}
uint64_t Runtime::subscribe(Plugin& plugin,const std::string& event,std::function<void(const Json&)> callback) {
    if(!events.contains(event))throw std::runtime_error("Unknown event: "+event);
    if(plugin.subscriptions.size()>=2048)throw std::runtime_error("Subscription budget exceeded (2048)");
    auto token=next_token_++;plugin.subscriptions.push_back({token,event,std::move(callback)});return token;
}
void Runtime::unsubscribe(Plugin& plugin,uint64_t token) { std::erase_if(plugin.subscriptions,[&](const auto& sub){return sub.token==token;}); }
void Runtime::dispatch(Plugin& plugin,const std::string& event,const Json& args) {
    std::vector<uint64_t> tokens;
    for(auto& sub:plugin.subscriptions)if(sub.event==event)tokens.push_back(sub.token);
    for(auto token:tokens) {
        if(!plugin.active)break;
        auto found=std::find_if(plugin.subscriptions.begin(),plugin.subscriptions.end(),[&](auto& s){return s.token==token;});
        if(found==plugin.subscriptions.end())continue;
        auto callback=found->callback;auto begin=Clock::now();
        try {callback(args);}catch(const std::exception& error){disable(plugin,error.what());}
        plugin.last_ms=std::chrono::duration<double,std::milli>(Clock::now()-begin).count(); plugin.total_ms+=plugin.last_ms;++plugin.calls;
    }
}
void Runtime::emit(const std::string& event,const Json& args) { for(auto& p:plugins_)if(p->active&&(event!="input"||args.value("owner",std::string())==p->manifest.id))dispatch(*p,event,args); }
void Runtime::request_reload(const std::string& id) { if(std::find(pending_reload_.begin(),pending_reload_.end(),id)==pending_reload_.end())pending_reload_.push_back(id); }
void Runtime::end_frame() {
    std::vector<std::pair<std::string,std::string>> errors;
    {std::lock_guard lock(patch_errors_gate_);errors=std::move(patch_errors_);patch_errors_.clear();}
    for(auto& [owner,cause]:errors)log(owner,"error",cause);
    auto pending=std::move(pending_reload_);pending_reload_.clear();for(auto& id:pending)reload(id);
}
bool Runtime::reload(const std::string& id) {
    auto old=std::find_if(plugins_.begin(),plugins_.end(),[&](auto& p){return p->manifest.id==id;});
    if(old==plugins_.end())return false;
    if((*old)->manifest.type!="lua"||(*old)->manifest.capabilities.contains("content")) {log(id,"warn","Content and native plugins require restart");return false;}
    if(patches.busy(old->get())){log(id,"warn","Reload deferred: patch callback still owns the current instance");request_reload(id);return false;}
    try {
        auto manifest=Manifest::parse(read_json((*old)->directory/"mod.json"));
        if(manifest.source!=(*old)->manifest.source)throw std::runtime_error("Manifest changed; restart required");
        auto candidate=prepare(manifest,(*old)->directory); candidate->world_data=(*old)->world_data;
        candidate->provisional=true;
        try {
            activate(*candidate);if(!candidate->active)throw std::runtime_error(candidate->error);
        }catch(...) {
            candidate->active=false;candidate->subscriptions.clear();candidate->hotkeys.clear();patches.retire(candidate.get());
            atomic_json(confined(root_,"data/"+id+"/config.json"),(*old)->config);
            atomic_json(confined(root_,"data/"+id+"/profile.json"),(*old)->profile_data);
            throw;
        }
        (*old)->provisional=true;deactivate(**old);candidate->provisional=false;*old=std::move(candidate);return true;
    }catch(const std::exception& error){log(id,"error",std::string("Reload rejected; previous instance retained: ")+error.what());return false;}
}
bool Runtime::set_enabled(const std::string& id,bool enabled) {
    settings_["disabled"][id]=!enabled;atomic_json(root_/"settings.json",settings_);
    for(auto& p:plugins_)if(p->manifest.id==id){
        if(p->manifest.type!="lua"||p->manifest.capabilities.contains("content")){p->error="Activation setting saved; restart required";continue;}
        if(!enabled){deactivate(*p);p->error="Disabled; restart required to enable";}
        else if(!p->active)p->error="Enabled for next start; restart or reload required";
    }
    return true;
}
bool Runtime::enabled(const std::string& id)const {return !settings_.contains("disabled")||!settings_["disabled"].value(id,false);}
void Runtime::add_patch(Plugin& plugin,PatchSpec spec,PatchCallbacks callbacks) {
    auto capability=spec.mode=="modify"?"time.write":"world.read";
    if(!plugin.manifest.capabilities.contains(capability))throw std::runtime_error(std::string("Missing capability: ")+capability);
    auto registrations=plugin.patch_registrations;registrations.push_back({std::move(spec),std::move(callbacks)});
    patches.validate(&plugin,plugin.manifest.id,registrations);
    if(!plugin.patch_staging)patches.replace(&plugin,plugin.manifest.id,registrations);
    plugin.patch_registrations=std::move(registrations);
}
bool Runtime::remove_patch(Plugin& plugin,const std::string& id) {
    auto registrations=plugin.patch_registrations;
    if(!std::erase_if(registrations,[&](auto& registration){return registration.spec.id==id;}))throw std::runtime_error("PATCH_NOT_FOUND: patch ID is not owned: "+id);
    if(!plugin.patch_staging)patches.replace(&plugin,plugin.manifest.id,registrations);
    plugin.patch_registrations=std::move(registrations);return !patches.busy(&plugin);
}
void Runtime::set_patch_authority(std::function<bool(const std::string&,std::string&)> guard){patch_authority_=std::move(guard);}
void Runtime::queue_patch_error(const std::string& owner,const std::string& cause){std::lock_guard lock(patch_errors_gate_);if(patch_errors_.size()<512)patch_errors_.emplace_back(owner,cause);else std::fprintf(stderr,"Patch error queue full: %s %s\n",owner.c_str(),cause.c_str());}
bool Runtime::patch_authorized(const std::string& operation,std::string& reason) const {
    reason.clear();
    if(!patch_authority_){reason="PATCH_AUTHORITY_UNKNOWN: no acknowledged session authority";return false;}
    auto allowed=patch_authority_(operation,reason);if(!allowed&&reason.empty())reason="PATCH_AUTHORITY_DENIED: operation denied by session authority";return allowed;
}
Json Runtime::patch_status() const {
    auto result=patches.status();std::string reason;bool allowed=patch_authorized("api.time.set",reason);
    result["authority"]={{"api.time.set",{{"allowed",allowed},{"reason",reason}}}};return result;
}
Json Runtime::request(Plugin& p,const std::string& op,const Json& args) {
    if(!args.is_object())throw std::runtime_error("API arguments must be an object");
    if(op=="patches.targets")return patches.targets();
    if(op=="patches.status")return patch_status();
    if(patches.in_callback()&&((op.starts_with("actions.")&&op!="actions.status")||op=="time.set"||op=="content.register"))throw std::runtime_error("PATCH_READ_ONLY: patch callbacks cannot call shared-world write APIs; modifiers return writable values");
    if(op=="log") {auto level=args.value("level","info");if(level!="debug"&&level!="info"&&level!="warn"&&level!="error")throw std::runtime_error("Invalid log level");log(p.manifest.id,level,args.at("message").get<std::string>());return true;}
    if(op=="config.define") {
        auto key=text(args,"key");if(key.find('/')!=std::string::npos)throw std::runtime_error("Invalid setting key");
        auto schema=args.at("schema");valid_config(schema,schema.at("default"));p.config_schema[key]=schema;
        if(!p.config.contains(key))p.config[key]=schema.at("default");valid_config(schema,p.config[key]);return p.config[key];
    }
    if(op=="config.get") {auto key=text(args,"key");if(!p.config_schema.contains(key))throw std::runtime_error("Undefined setting: "+key);return p.config.at(key);}
    if(op=="config.set") {auto key=text(args,"key");valid_config(p.config_schema.at(key),args.at("value"));p.config[key]=args.at("value");if(!p.staging)atomic_json(confined(root_,"data/"+p.manifest.id+"/config.json"),p.config);return true;}
    if(op.starts_with("storage.")) {
        auto scope=args.value("scope","profile");if(scope!="profile"&&scope!="world")throw std::runtime_error("Storage scope must be profile or world");
        if(scope=="world"&&world_.empty())throw std::runtime_error("World storage requires a loaded world");
        auto& data=scope=="profile"?p.profile_data:p.world_data;auto key=text(args,"key");
        if(op=="storage.get")return data.value(key,args.value("default",Json()));
        if(op!="storage.set")throw std::runtime_error("Unknown storage operation");
        data[key]=args.at("value");if(data.dump().size()>1024*1024)throw std::runtime_error("Mod storage exceeds 1 MiB");
        if(scope=="profile"&&!p.staging)atomic_json(confined(root_,"data/"+p.manifest.id+"/profile.json"),data);return true;
    }
    if(op=="input.bind") {
        if(!p.manifest.capabilities.contains("input"))throw std::runtime_error("Missing capability: input");
        auto key=text(args,"key");int code=args.at("scancode").get<int>();if(code<=0||code>=512||code==67)throw std::runtime_error("Invalid or framework-reserved scancode");
        for(auto& other:plugins_)for(auto& [name,scan]:other->hotkeys)if(other->manifest.id!=p.manifest.id&&scan==code)throw std::runtime_error("Hotkey already owned by "+other->manifest.id);
        p.hotkeys[key]=code;return true;
    }
    std::string cap;
    if(op=="ui.world_marker"||op=="ui.world_project") {
        cap="ui";
        if(!p.active||!p.native_loaded||p.manifest.type!="native"||p.staging)throw std::runtime_error("World markers require an active native plugin callback");
        if(!p.manifest.capabilities.contains("world.read"))throw std::runtime_error("Missing capability: world.read");
    }
    else if(op.starts_with("ui.")) {cap="ui";if(!rendering)throw std::runtime_error("UI calls belong in ui.window/render callbacks");}
    else if(op.starts_with("world."))cap="world.read";
    else if(op.starts_with("objects."))cap="objects.read";
    else if(op.starts_with("camera."))cap="camera";
    else if(op=="time.get")cap="world.read";
    else if(op=="time.set")cap="time.write";
    else if(op.starts_with("actions."))cap="objects.write";
    else if(op=="content.register")cap="content";
    else throw std::runtime_error("Unknown API operation: "+op);
    if(!p.manifest.capabilities.contains(cap))throw std::runtime_error("Missing capability: "+cap);
    if(op=="content.register"&&!world_.empty())throw std::runtime_error("Content registration belongs to startup; restart required");
    if(op=="content.register"&&p.staging) {
        auto definitions=args.at("definitions");if(!definitions.is_array()||definitions.empty())throw std::runtime_error("Content definitions must be a nonempty array");
        std::set<std::string> ids;for(auto& row:p.content_definitions)ids.insert(row.at("id"));
        for(auto& row:definitions){auto id=row.at("id").get<std::string>();
            if(!id.starts_with(p.manifest.id+":")||!ids.insert(id).second)throw std::runtime_error("Content ID duplicated or outside mod namespace: "+id);
            for(const char* key:{"model","material"})if(!fs::is_regular_file(confined(p.directory,row.at(key).get<std::string>())))throw std::runtime_error("Content resource missing: "+std::string(key));
        }
        for(auto& row:definitions)p.content_definitions.push_back(row);return {{"staged",definitions.size()}};
    }
    if(p.staging)throw std::runtime_error("Game and UI operations belong in event callbacks");
    if(multiplayer&&p.manifest.multiplayer=="singleplayer")throw std::runtime_error("Singleplayer mod cannot run in multiplayer");
    if(multiplayer&&(cap=="objects.write"||cap=="time.write"||cap=="content"))throw std::runtime_error("Multiplayer bridge is not active; shared writes are unavailable");
    if(!adapter_)throw std::runtime_error("Game adapter unavailable: "+op);
    auto forwarded=args;forwarded["mod_id"]=p.manifest.id;
    if(op=="content.register")forwarded["directory"]=path_text(p.directory);
    if(op=="time.set"){
        std::string reason;if(!patch_authorized("api.time.set",reason))throw std::runtime_error(reason);
        if(!args.contains("paused")||!args.at("paused").is_boolean()||!args.contains("speed")||!args.at("speed").is_number()||!std::isfinite(args.at("speed").get<double>())||args.at("speed").get<double>()<0)
            throw std::runtime_error("PATCH_VALUES: time.set requires boolean paused and finite nonnegative speed");
        auto run=patches.begin("api.time.set",{{"paused",args.at("paused")},{"speed",args.at("speed")},{"epoch",std::to_string(epoch)}},nullptr,true);
        if(run.failed){patches.end(run,{{"original_returned",false},{"outcome","rejected"}});throw std::runtime_error("PATCH_VALUES: api.time.set writer failed; original request was not executed");}
        forwarded["paused"]=run.data.at("paused");forwarded["speed"]=run.data.at("speed");
        if(!patch_authorized("api.time.set",reason)){patches.end(run,{{"original_returned",false},{"outcome","rejected"}});throw std::runtime_error(reason);}
        try{auto result=adapter_(op,forwarded);patches.end(run,{{"original_returned",true},{"outcome","confirmed"},{"result",result}});return result;}
        catch(...){patches.end(run,{{"original_returned",false},{"outcome","unknown"}});throw;}
    }
    auto result=adapter_(op,forwarded);if(op=="content.register")p.content_registered=true;return result;
}
void Runtime::world_ready(const std::string& identity) {
    if(world_==identity)return;if(!world_.empty())world_leave();world_=identity;++epoch;patch_epoch_.store(epoch,std::memory_order_release);
    for(auto& p:plugins_)p->world_data=Json::object();
    object_bindings=Json::object();
    if(pending_world_metadata_.is_object()&&pending_world_metadata_.value("world",std::string())==identity) {
        if(!native_initial_metadata(pending_world_metadata_)) {
            for(auto& p:plugins_)if(pending_world_metadata_["mods"].contains(p->manifest.id))p->world_data=pending_world_metadata_["mods"][p->manifest.id];
            object_bindings=pending_world_metadata_.value("objects",Json::object());
            for(auto& row:object_bindings)row["epoch"]=0;
        }
    }
    pending_world_metadata_=Json();
    emit("world_ready",{{"epoch",epoch},{"name",identity}});
}
void Runtime::world_leave() {if(world_.empty())return;emit("world_leave",{{"epoch",epoch},{"name",world_}});last_world_metadata_=capture_save(world_);world_.clear();++epoch;patch_epoch_.store(epoch,std::memory_order_release);for(auto& p:plugins_)p->world_data=Json::object();}
Json Runtime::capture_save(const std::string& name) const {
    if(world_.empty()||name!=world_) {
        if(last_world_metadata_.is_object()&&last_world_metadata_.value("world",std::string())==name)return last_world_metadata_;
        throw std::runtime_error("Save metadata has no matching observed world: "+name);
    }
    Json data={{"world",world_},{"epoch",epoch},{"mods",Json::object()},{"required",Json::array()},{"objects",object_bindings}};
    for(auto& p:plugins_){if(p->active)data["mods"][p->manifest.id]=p->world_data;if(p->content_registered)data["required"].push_back(p->fingerprint);}
    return data;
}
Json Runtime::capture_initial_save(const std::string& name) const {
    if(name.empty() || !world_.empty())
        throw std::runtime_error("Initial save metadata requires no ready world");
    if(pending_world_metadata_.is_object())
        throw std::runtime_error("Initial save metadata cannot replace pending loaded mod data");
    for(const auto& plugin:plugins_)
        if(plugin->active && (!plugin->world_data.is_object() || !plugin->world_data.empty()))
            throw std::runtime_error("Initial save metadata has uninitialized mod data: " + plugin->manifest.id);
    for(const auto& row:object_bindings)
        if(!row.is_object() || row.value("epoch",epoch)>=epoch)
            throw std::runtime_error("Initial save metadata has current or unknown object bindings");
    Json data={{"world",name},{"epoch",nullptr},{"phase","native_initial"},
        {"mods",Json::object()},{"objects",Json::object()},{"required",Json::array()}};
    for(const auto& plugin:plugins_)
        if(plugin->content_registered)data["required"].push_back(plugin->fingerprint);
    return data;
}
void store_save_metadata(const fs::path& root,Json data,const std::string& identity) {
    native_initial_metadata(data);
    data["save"]=identity;auto world=data.at("world").get<std::string>();
    atomic_json(root/"saves"/(digest(identity)+".json"),data);atomic_json(root/"worlds"/(digest(world)+".json"),data);
}
void Runtime::save_acknowledged(const Json& data,const std::string& identity) {
    if(native_initial_metadata(data))throw std::runtime_error("Native initial metadata cannot acknowledge a ready-world save");
    emit("saved",{{"save",identity},{"epoch",data.at("epoch")},{"name",data.at("world")}});
}
void Runtime::saved(const std::string& identity) {
    if(world_.empty())throw std::runtime_error("Save acknowledgement without a world");
    auto data=capture_save(world_);store_save_metadata(root_,data,identity);save_acknowledged(data,identity);
}
void Runtime::check_save(const std::string& identity) {
    pending_world_metadata_=Json();
    auto path=root_/"saves"/(digest(identity)+".json");if(!fs::exists(path))return;
    auto metadata=read_json(path);
    native_initial_metadata(metadata);
    for(auto& needed:metadata.at("required")) {
        auto required_id=needed.at("id").get<std::string>();
        auto found=std::find_if(plugins_.begin(),plugins_.end(),[&](auto& p){return p->active&&p->manifest.id==required_id;});
        if(found==plugins_.end()||(*found)->fingerprint!=needed)throw std::runtime_error("Save requires matching content: "+needed.at("id").get<std::string>());
    }
    pending_world_metadata_=metadata;
}
Json Runtime::shared_manifest() const {Json result=Json::array();for(auto& p:plugins_)if(p->active&&p->manifest.multiplayer=="shared"){auto item=p->fingerprint;item["settings"]=p->config;result.push_back(item);}return result;}
std::vector<std::string> Runtime::multiplayer_conflicts(const Json& peer) const {
    std::vector<std::string> errors;auto local=shared_manifest();
    if(!peer.is_array())return {"Peer shared manifest is not an array"};
    for(auto& item:local)if(std::find(peer.begin(),peer.end(),item)==peer.end())errors.push_back("Shared mod differs: "+item.at("id").get<std::string>());
    for(auto& item:peer)if(std::find(local.begin(),local.end(),item)==local.end())errors.push_back("Peer requires different shared mod: "+item.value("id","unknown"));
    return errors;
}
Json Runtime::status() const {
    Json mods=Json::array();for(auto& p:plugins_)mods.push_back({{"id",p->manifest.id},{"name",p->manifest.name},{"version",p->manifest.version},{"type",p->manifest.type},{"active",p->active},{"error",p->error},{"calls",p->calls},{"last_ms",p->last_ms},{"total_ms",p->total_ms},{"settings",p->config},{"schema",p->config_schema}});
    return {{"version",MT2FW_VERSION},{"api_version","1.1.0"},{"epoch",epoch},{"world",world_},{"mods",mods},{"multiplayer_bridge",false}};
}
}
