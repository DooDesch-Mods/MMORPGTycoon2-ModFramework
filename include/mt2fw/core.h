#pragma once
// Internal runtime declarations. The supported native DLL boundary is plugin.h.
#include "plugin.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <deque>
#include <mutex>

namespace mt2fw {
using Json = nlohmann::json;
namespace fs = std::filesystem;
struct Manifest {
    std::string id, name, version, type, entry, multiplayer;
    std::set<std::string> capabilities;
    std::map<std::string,std::string> dependencies;
    Json source;
    static Manifest parse(const Json& value);
};
bool version_matches(const std::string& version, const std::string& required);
fs::path confined(const fs::path& root, const fs::path& relative);
inline fs::path confined(const fs::path& root,const std::string& relative){return confined(root,fs::u8path(relative));}
inline fs::path confined(const fs::path& root,const char* relative){return confined(root,fs::u8path(relative));}
inline std::string path_text(const fs::path& path){auto value=path.generic_u8string();return {reinterpret_cast<const char*>(value.data()),value.size()};}
Json read_json(const fs::path& path);
void atomic_json(const fs::path& path, const Json& value);
void store_save_metadata(const fs::path& root, Json metadata, const std::string& identity);
struct LogEntry { std::string mod, level, message; };
struct Subscription { uint64_t token; std::string event; std::function<void(const Json&)> callback; };
class LuaMod;
class Runtime;
struct Plugin {
    Runtime* runtime = nullptr;
    Manifest manifest;
    fs::path directory;
    std::unique_ptr<LuaMod> lua;
    MT2FW_Host host{};
    void* native_module = nullptr;
    MT2FW_ShutdownFn native_shutdown = nullptr;
    bool native_loaded = false;
    uint32_t native_pins = 0;
    bool native_shutdown_pending = false, native_retirement_failed = false;
    bool native_retained = false;
    std::vector<Subscription> subscriptions;
    Json config = Json::object(), config_schema = Json::object();
    Json profile_data = Json::object(), world_data = Json::object();
    Json fingerprint;
    Json content_definitions = Json::array();
    bool content_registered = false;
    std::map<std::string,int> hotkeys;
    std::string error;
    bool active = false, staging = true;
    bool provisional = false;
    uint64_t calls = 0;
    double last_ms = 0, total_ms = 0;
    explicit Plugin(Runtime* owner);
    ~Plugin();
};
class Runtime {
public:
    using Adapter = std::function<Json(const std::string&,const Json&)>;
    explicit Runtime(fs::path root, Adapter adapter = {});
    ~Runtime();
    void load();
    bool reload(const std::string& id);
    void request_reload(const std::string& id);
    void end_frame();
    void emit(const std::string& event, const Json& args = Json::object());
    Json request(Plugin& plugin, const std::string& operation, const Json& args);
    Json status() const;
    void log(const std::string& mod, const std::string& level, const std::string& message);
    uint64_t subscribe(Plugin& plugin, const std::string& event, std::function<void(const Json&)> callback);
    void unsubscribe(Plugin& plugin, uint64_t token);
    void disable(Plugin& plugin, const std::string& cause);
    bool set_enabled(const std::string& id, bool enabled);
    bool enabled(const std::string& id) const;
    void world_ready(const std::string& identity);
    void world_leave();
    void saved(const std::string& save_identity);
    Json capture_save(const std::string& world) const;
    Json capture_initial_save(const std::string& world) const;
    void save_acknowledged(const Json& metadata,const std::string& identity);
    void check_save(const std::string& save_identity);
    std::vector<std::string> multiplayer_conflicts(const Json& peer) const;
    Json shared_manifest() const;
    Plugin* native_plugin(const MT2FW_Host* host) const;
    void release_native(Plugin& plugin);
    void stop_plugins();
    std::function<bool(Plugin&)> native_retire;
    const fs::path& root() const { return root_; }
    const std::vector<std::unique_ptr<Plugin>>& plugins() const { return plugins_; }
    const std::deque<LogEntry>& logs() const { return logs_; }
    bool multiplayer = false;
    bool rendering = false;
    uint64_t epoch = 0;
    Json object_bindings = Json::object();
private:
    fs::path root_;
    Adapter adapter_;
    std::vector<std::unique_ptr<Plugin>> plugins_;
    std::vector<void*> pinned_modules_;
    std::deque<LogEntry> logs_;
    std::vector<std::string> pending_reload_;
    std::string world_;
    Json settings_ = Json::object();
    Json pending_world_metadata_, last_world_metadata_;
    uint64_t next_token_ = 1;
    std::unique_ptr<Plugin> prepare(const Manifest& manifest, const fs::path& directory);
    void activate(Plugin& plugin);
    void deactivate(Plugin& plugin);
    void dispatch(Plugin& plugin, const std::string& event, const Json& args);
};
}
