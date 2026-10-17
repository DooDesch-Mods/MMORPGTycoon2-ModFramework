#include <nlohmann/json.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

using Json = nlohmann::json;
struct NativeWindow {
    void* panel = nullptr;
    bool requested = true, suspended = false;
    Json clicked = Json::array();
} window;
uint8_t fixture_visible = 1;
bool panel_open = false, fixture_readable = true, fixture_show_effect = true;
int fixture_open_calls = 0, fixture_hide_calls = 0, fixture_reads = 0, fixture_show_error = 0;
void require(bool accepted, const std::string& cause) {
    if (!accepted) throw std::runtime_error(cause);
}
void check(int error, const std::string& cause) { require(error == 0, cause); }
int show(void* receiver, bool desired) {
    require(receiver == &fixture_visible, "original Show receiver differs");
    if (desired) ++fixture_open_calls; else ++fixture_hide_calls;
    if (fixture_show_error) return fixture_show_error;
    if (fixture_show_effect) fixture_visible = desired ? uint8_t{1} : uint8_t{0};
    return 0;
}
bool read(const void* receiver, size_t offset, uint8_t& value) {
    require(receiver == &fixture_visible && offset == 0x45f, "native visibility receiver/offset differs");
    ++fixture_reads;
    if (!fixture_readable) return false;
    value = fixture_visible;
    return true;
}
Json observe(bool desired) {
#include "actual_native_window_block.inc"
    return events;
}
Json previous_observe(bool desired) {
#include "previous_native_window_block.inc"
    return events;
}
void reset() {
    window.panel = &fixture_visible; window.requested = true; window.suspended = false;
    window.clicked = Json::array({"pending-click"}); fixture_visible = 1;
    panel_open = false; fixture_readable = fixture_show_effect = true;
    fixture_open_calls = fixture_hide_calls = fixture_reads = fixture_show_error = 0;
}
int checks = 0;
void expect(bool accepted, const char* cause) { ++checks; require(accepted, cause); }
template<class Invoke> void fails(Invoke invoke, const char* expected) {
    bool rejected = false;
    try { invoke(); }
    catch (const std::runtime_error& error) { rejected = std::string(error.what()) == expected; }
    expect(rejected, "original failure cause must remain visible");
}
int main() {
    try {
        reset(); fixture_visible = 0;
        auto event = previous_observe(true);
        bool model_open = event.at("open").get<bool>();
        expect(!model_open && window.requested && window.clicked.empty(), "old branch reports physical close but retains stale requested cache");
        expect(fixture_open_calls == 0 && fixture_hide_calls == 0, "old close observation performs no Show");
        // The app adopts closed and makes no SDK request until the explicit reopen.
        model_open = true;
        event = previous_observe(model_open);
        expect(!event.at("open").get<bool>() && !fixture_visible && fixture_open_calls == 0,
            "old stale cache suppresses explicit reopen without an intervening closed SDK call");

        reset(); fixture_visible = 0;
        event = observe(true);
        model_open = event.at("open").get<bool>();
        expect(!model_open && !window.requested && window.clicked.empty(), "physical close clears desired-state cache and stale clicks");
        expect(event.at("visibility_known") && event.at("visible") == false && !event.at("suspended").get<bool>(), "close uses actual native visibility readback");
        expect(!fixture_visible && fixture_open_calls == 0 && fixture_hide_calls == 0, "observing close never forces immediate reopen");
        model_open = true;
        event = observe(model_open);
        expect(fixture_open_calls == 1 && fixture_hide_calls == 0 && fixture_visible == 1, "explicit reopen calls original Show exactly once");
        expect(event.at("open") && event.at("visibility_known") && event.at("visible") && window.requested, "reopen is confirmed by actual visibility readback");
        event = observe(true);
        expect(fixture_open_calls == 1 && event.at("visible"), "ordinary open polling does not repeat Show");

        reset(); panel_open = true;
        event = observe(true);
        expect(event.at("open") && event.at("suspended") && event.at("visibility_known") && event.at("visible") == false,
            "Mods panel suspension retains desired open while reporting hidden native window");
        expect(window.requested && window.suspended && fixture_hide_calls == 1 && fixture_open_calls == 0,
            "suspension does not consume desired state as physical close");
        model_open = true;
        if (!event.at("suspended").get<bool>()) model_open = event.at("open").get<bool>();
        panel_open = false;
        event = observe(model_open);
        expect(fixture_open_calls == 1 && fixture_hide_calls == 1 && event.at("visible") && !window.suspended,
            "ending suspension resumes original Show once");
        event = observe(model_open);
        expect(fixture_open_calls == 1 && event.at("open"), "resumed polling does not repeat Show");

        reset(); event = observe(false);
        expect(fixture_hide_calls == 1 && !window.requested && !window.suspended && !fixture_visible,
            "ordinary explicit close hides once and clears desired state");
        expect(event.at("visibility_known") && event.at("visible") == false && event.at("open") == false,
            "explicit close reports actual visibility");
        event = observe(false);
        expect(fixture_hide_calls == 1, "already closed polling does not repeat Hide");

        reset(); window.panel = nullptr;
        event = observe(false);
        expect(event.at("ready") && event.at("open") == false && !event.at("visibility_known").get<bool>() && event.at("visible").is_null(),
            "absent window has no invented visibility");
        expect(fixture_reads == 0 && fixture_open_calls == 0 && fixture_hide_calls == 0, "absent window invokes no native leaf");

        reset(); fixture_readable = false;
        fails([] { observe(true); }, "Native app visibility unreadable");
        expect(fixture_open_calls == 0 && fixture_hide_calls == 0, "failed open observation does not retry Show");
        reset(); fixture_readable = false;
        fails([] { observe(false); }, "Native app hidden visibility unreadable");
        expect(fixture_hide_calls == 1, "failed close readback does not replay Hide");
        reset(); panel_open = true; fixture_readable = false;
        fails([] { observe(true); }, "Native app hidden visibility unreadable");
        expect(window.requested && window.suspended && fixture_hide_calls == 1, "failed suspended readback preserves suspension state");

        reset(); window.requested = false; fixture_visible = 0; fixture_show_effect = false;
        event = observe(true);
        expect(fixture_open_calls == 1 && event.at("open") == false && event.at("visible") == false && !window.requested,
            "Show return alone cannot confirm reopen without native visible state");
        reset(); window.requested = false; fixture_visible = 0; fixture_show_error = 5;
        fails([] { observe(true); }, "Open native app");
        expect(fixture_open_calls == 1 && !window.requested, "original Show failure does not become acknowledged open or replay");
        reset(); fixture_show_error = 5;
        fails([] { observe(false); }, "Hide native app");
        expect(fixture_hide_calls == 1 && window.requested, "original Hide failure remains unacknowledged");
        std::cout << checks << " native window reopen conditions passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
