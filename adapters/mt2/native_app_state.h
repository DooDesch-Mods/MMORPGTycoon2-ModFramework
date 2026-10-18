#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace mt2fw::native_app {
enum class IconKind { fallback, cache, asset };
inline IconKind icon_kind(const std::string& value) {
    if (value.empty()) return IconKind::fallback;
    if (value.starts_with("cache/") && value.size() == 74 && value.ends_with(".png") &&
        value.substr(6,64).find_first_not_of("0123456789abcdef") == std::string::npos) return IconKind::cache;
    if (!value.starts_with("assets/") || !value.ends_with(".png") || value.size() > 256 ||
        value.find_first_of("\\:<>\"|?*") != std::string::npos)
        throw std::runtime_error("Desktop icon requires an owner assets/*.png or cache/<sha256>.png file");
    for (unsigned char ch : value) if (ch < 32) throw std::runtime_error("Desktop icon path contains a control character");
    for (size_t begin = 0; begin < value.size();) {
        const auto end = value.find('/', begin);
        const auto part = value.substr(begin, end == std::string::npos ? end : end - begin);
        if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ')
            throw std::runtime_error("Desktop icon path contains an unsafe component");
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return IconKind::asset;
}
inline std::string icon_template(std::string bytes, const std::string& key) {
    if (key.size() != 64 || key.find_first_not_of("0123456789abcdef") != std::string::npos)
        throw std::runtime_error("Desktop app template key is not a SHA256 identity");
    const std::string anchor = "mt2fw_mods_entry";
    const auto offset = bytes.find(anchor);
    if (offset == std::string::npos || bytes.find(anchor, offset + anchor.size()) != std::string::npos)
        throw std::runtime_error("Desktop app entry template identity differs");
    bytes.replace(offset, anchor.size(), "mt2fw_app_" + key);
    return bytes;
}
struct WizardSnapshot {
    uintptr_t title = 0, window = 0;
    bool visible = false;
};
struct WizardFailure : std::runtime_error {
    bool unknown, attempted;
    WizardFailure(const std::string& cause, bool uncertain, bool called)
        : std::runtime_error(cause), unknown(uncertain), attempted(called) {}
};
enum class WizardResult { already_open, opened };

class WizardLatch {
    std::optional<WizardSnapshot> pending_;
public:
    void reset_epoch() { pending_.reset(); }
    template<class Observe> WizardSnapshot inspect(Observe observe) const { return observe(); }
    template<class Observe, class Launch> WizardResult open(Observe observe, Launch launch) {
        const auto before = observe();
        if (before.visible) { pending_.reset(); return WizardResult::already_open; }
        if (pending_ && pending_->title == before.title && pending_->window == before.window)
            throw WizardFailure("Previous New Game launch is unconfirmed; inspect the original wizard before retrying", true, false);
        bool attempted = false;
        pending_ = before;
        try {
            launch(attempted);
            if (!attempted) throw std::runtime_error("New Game launcher returned without an execution acknowledgement");
            const auto after = observe();
            if (after.title != before.title || after.window != before.window)
                throw std::runtime_error("New Game owner changed after launch");
            if (!after.visible) throw std::runtime_error("New Game launch did not confirm native visibility");
            pending_.reset();
            return WizardResult::opened;
        } catch (const std::exception& error) {
            if (!attempted) pending_.reset();
            throw WizardFailure(error.what(), attempted, attempted);
        }
    }
};

struct OpenRequest {
    bool pending = false, active = false;
    void retire() { active = false; pending = false; }
    void launch() {
        if (!active) throw std::runtime_error("Desktop app owner is inactive");
        pending = true;
    }
    bool consume() { const bool value = active && pending; pending = false; return value; }
};
}
