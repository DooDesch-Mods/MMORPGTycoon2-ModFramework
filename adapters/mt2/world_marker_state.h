#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <exception>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace mt2fw::mt2::markers {
using Json = nlohmann::json;
using Key = std::pair<std::string, std::string>;
inline void require(bool value, const char* cause) { if (!value) throw std::runtime_error(cause); }
inline uint64_t epoch_value(const Json& value) {
    if (value.is_number_unsigned()) return value.get<uint64_t>();
    if (value.is_number_integer()) {
        auto number = value.get<int64_t>(); require(number >= 0, "World epoch must be unsigned"); return uint64_t(number);
    }
    require(value.is_string(), "World epoch must be uint64 or a decimal string");
    auto text = value.get<std::string>();
    require(!text.empty() && (text.size() == 1 || text.front() != '0'), "World epoch string is not canonical");
    uint64_t result = 0; auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    require(parsed.ec == std::errc() && parsed.ptr == text.data() + text.size(), "World epoch is outside uint64");
    return result;
}
template<size_t N> std::array<float, N> vector(const Json& value, bool unit = false) {
    require(value.is_array() && value.size() == N, "Marker vector has an invalid dimension");
    std::array<float, N> result{};
    for (size_t i = 0; i < N; ++i) {
        require(value[i].is_number(), "Marker vector must be numeric");
        auto number = value[i].get<double>(); result[i] = float(number);
        require(std::isfinite(number) && std::isfinite(result[i]), "Marker vector must fit finite Float32");
        require(!unit || (number >= 0 && number <= 1), "Marker color must be in [0,1]");
    }
    return result;
}
inline std::string identifier(const Json& args) {
    auto id = args.at("id").get<std::string>();
    require(!id.empty() && id.size() <= 64, "Marker ID must contain 1 to 64 ASCII bytes");
    for (auto c : id) require((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                             (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.', "Marker ID contains an invalid character");
    return id;
}
inline std::string peer_name(const Json& args) {
    auto name = args.at("name").get<std::string>();
    require(!name.empty() && name.size() <= 64, "Marker name must contain 1 to 64 UTF-8 bytes");
    for (unsigned char c : name) require(c >= 32 && c != 127, "Marker name contains a control character");
    // The JSON parser validates UTF-8. Dump also validates direct in-process requests.
    (void)Json(name).dump();
    return name;
}
struct Entry {
    std::array<float, 3> world{};
    std::array<float, 4> color{};
    std::string name;
    uint64_t epoch = 0, revision = 0, drawn_revision = 0, draws = 0;
    bool enabled = true, visible = false;
};
class Store {
public:
    std::map<Key, Entry> entries;
    uint64_t epoch = 0;
    bool ready = false;
    void context(uint64_t next_epoch, bool next_ready) {
        if (epoch != next_epoch || !next_ready) entries.clear();
        epoch = next_epoch; ready = next_ready;
    }
    void validate_epoch(const Json& args) const {
        require(ready && epoch != 0, "World markers require an acknowledged active world");
        require(epoch_value(args.at("epoch")) == epoch, "World marker belongs to another Framework world epoch");
    }
    void retire(const std::string& owner) {
        for (auto i = entries.begin(); i != entries.end();) {
            if (i->first.first == owner) i = entries.erase(i); else ++i;
        }
    }
    Json request(const std::string& owner, const Json& args) {
        const auto action = args.at("action").get<std::string>();
        if (action == "status") {
            Json rows = Json::array();
            for (const auto& [key, e] : entries) if (key.first == owner)
                rows.push_back({{"id", key.second}, {"enabled", e.enabled}, {"visible", e.visible},
                                {"revision", e.revision}, {"drawn_revision", e.drawn_revision}, {"draws", e.draws}});
            return {{"epoch", std::to_string(epoch)}, {"ready", ready}, {"markers", rows}};
        }
        if (action == "clear") { retire(owner); return {{"status", "retired"}}; }
        const Key key{owner, identifier(args)};
        if (action == "retire") { entries.erase(key); return {{"id", key.second}, {"status", "retired"}}; }
        if (action == "hide") {
            auto i = entries.find(key); if (i != entries.end()) { i->second.enabled = false; i->second.visible = false; }
            return {{"id", key.second}, {"status", "hidden"}};
        }
        require(action == "set", "Unknown world marker action"); validate_epoch(args);
        Entry next; next.epoch = epoch; next.world = vector<3>(args.at("world"));
        next.color = vector<4>(args.at("color"), true); next.name = peer_name(args);
        auto old = entries.find(key);
        if (old == entries.end()) {
            size_t count = 0; for (const auto& [owned, _] : entries) if (owned.first == owner) ++count;
            require(count < 4, "Each plugin can own at most four world markers");
        } else { next.draws = old->second.draws; next.drawn_revision = old->second.drawn_revision; }
        next.revision = ++revision_;
        entries.insert_or_assign(key, next);
        return {{"id", key.second}, {"status", "accepted"}, {"revision", next.revision}, {"epoch", std::to_string(epoch)}};
    }
    void presented(const Key& key, bool visible) {
        auto i = entries.find(key); if (i == entries.end()) return;
        i->second.visible = visible;
        if (visible) { i->second.drawn_revision = i->second.revision; ++i->second.draws; }
    }
private:
    uint64_t revision_ = 0;
};
// The original draw must run once even if preparation fails. Cleanup cannot mask
// the original exception. Callers report preparation and cleanup failures.
template<class Prepare, class Original, class Cleanup>
void transient_draw(Prepare prepare, Original original, Cleanup cleanup) {
    static_assert(std::is_nothrow_invocable_v<Cleanup>, "Native draw cleanup must report failures without throwing");
    std::exception_ptr preparation;
    try { prepare(); } catch (...) { preparation = std::current_exception(); }
    try { original(); } catch (...) { cleanup(); throw; }
    cleanup();
    if (preparation) std::rethrow_exception(preparation);
}
}
