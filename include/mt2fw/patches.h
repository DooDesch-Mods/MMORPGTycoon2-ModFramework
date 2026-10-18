#pragma once
#include <nlohmann/json.hpp>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace mt2fw {
struct PatchSpec {
    std::string id, target, mode = "observe";
    int priority = 0;
    std::vector<std::string> before, after;
    static PatchSpec parse(const nlohmann::json&);
};
struct PatchCall {
    nlohmann::json data;
    const void* native = nullptr;
};
struct PatchCallbacks {
    std::function<nlohmann::json(const PatchCall&)> before;
    std::function<void(const PatchCall&)> after;
};
struct PatchRegistration { PatchSpec spec; PatchCallbacks callbacks; };
class PatchRegistry {
    struct Entry;
public:
    using Json = nlohmann::json;
    using Error = std::function<void(const std::string&,const std::string&)>;
    class Run {
    public:
        Run() = default;
        Run(Run&&) noexcept;
        Run& operator=(Run&&) noexcept;
        Run(const Run&) = delete;
        Run& operator=(const Run&) = delete;
        ~Run();
        Json data = Json::object();
        bool failed = false;
        uint64_t invocation = 0, generation = 0;
    private:
        PatchRegistry* registry_ = nullptr;
        std::vector<std::shared_ptr<Entry>> selected_, entered_;
        std::string target_;
        friend class PatchRegistry;
    };
    explicit PatchRegistry(Error error = {});
    static void validate_values(const std::string& mode,const Json&);
    void validate(void* owner,const std::string& owner_id,const std::vector<PatchRegistration>&) const;
    void replace(void* owner,const std::string& owner_id,const std::vector<PatchRegistration>&);
    void add(void* owner,const std::string& owner_id,PatchSpec,PatchCallbacks);
    bool remove(void* owner,const std::string& id);
    bool retire(void* owner);
    bool busy(void* owner) const;
    bool has_subscribers(const std::string& target) const;
    Json targets() const;
    Json status() const;
    Run begin(const std::string& target,Json data,const void* native = nullptr,bool allow_modify = false);
    void end(Run&,Json after = Json::object(),const void* native = nullptr) noexcept;
    bool in_observer() const;
    bool in_callback() const;
private:
    struct Entry {
        void* owner = nullptr;
        std::string owner_id, key;
        PatchRegistration registration;
        std::string last_error;
        uint64_t suppressed_errors = 0;
    };
    mutable std::recursive_mutex gate_;
    std::vector<std::shared_ptr<Entry>> entries_;
    std::map<std::string,std::vector<std::shared_ptr<Entry>>> order_;
    std::map<void*,size_t> inflight_;
    Error error_;
    uint64_t generation_ = 0, next_invocation_ = 1;
    std::map<std::string,std::vector<std::shared_ptr<Entry>>> sorted(const std::vector<std::shared_ptr<Entry>>&) const;
    std::vector<std::shared_ptr<Entry>> candidate(void*,const std::string&,const std::vector<PatchRegistration>&) const;
    void publish(std::vector<std::shared_ptr<Entry>>);
    void report(Entry&,const char*,const std::string&) noexcept;
};
}
