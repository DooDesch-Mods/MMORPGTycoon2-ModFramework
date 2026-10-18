#include "native_app_state.h"
#include <iostream>
#include <functional>
#include <fstream>
#include <iterator>

using namespace mt2fw::native_app;
void require(bool value, const char* cause) { if (!value) throw std::runtime_error(cause); }
void failure(const std::function<void()>& action, const std::string& cause, bool unknown, bool attempted) {
    try { action(); }
    catch (const WizardFailure& error) {
        require(std::string(error.what()).find(cause) != std::string::npos, "Original wizard failure cause differs");
        require(error.unknown == unknown && error.attempted == attempted, "Wizard outcome classification differs");
        return;
    }
    throw std::runtime_error("Expected wizard failure was not returned");
}
int main() {
    try {
        int checks = 0;
        {
            WizardLatch latch; int calls = 0; bool visible = false;
            auto observe = [&] { return WizardSnapshot{1, 2, visible}; };
            auto launch = [&](bool& attempted) { attempted = true; ++calls; };
            require(!latch.inspect(observe).visible && calls == 0, "Read-only observation opened a cancelled wizard");
            failure([&] { latch.open(observe, launch); }, "visibility", true, true);
            require(!latch.inspect(observe).visible && calls == 1, "Read mode replayed an unconfirmed launch");
            failure([&] { latch.open(observe, launch); }, "Previous", true, false);
            visible = true;
            require(latch.inspect(observe).visible && calls == 1, "Read mode changed a visible wizard");
            ++checks;
        }
        {
            require(icon_kind("assets/multiplayer.png") == IconKind::asset &&
                icon_kind("cache/" + std::string(64, 'a') + ".png") == IconKind::cache && icon_kind("") == IconKind::fallback,
                "Owned icon references differ");
            for (const auto& path : {"C:/private.png", "https://remote/icon.png", "assets/../private.png", "assets//icon.png", "assets/link:stream.png", "assets/name /icon.png"}) {
                bool rejected = false;
                try { icon_kind(path); } catch (const std::runtime_error& error) { require(*error.what(), "Icon path rejection lost its cause"); rejected = true; }
                require(rejected, "Unsafe icon path accepted");
            }
            ++checks;
        }
        {
            std::ifstream file(MT2FW_TEST_ENTRY, std::ios::binary);
            require(bool(file), "Native Desktop entry fixture is missing");
            std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            const auto first = icon_template(bytes, std::string(64, 'a'));
            const auto second = icon_template(bytes, std::string(64, 'b'));
            require(first != second && first.find("mmoDesktopIcon {") != std::string::npos && first.find("mt2fw_mods_entry") == std::string::npos,
                "Native app template did not preserve the class with a separate root identity");
            for (const auto& invalid : {std::string("PNG bytes"), bytes + bytes}) {
                bool rejected = false;
                try { icon_template(invalid, std::string(64, 'a')); }
                catch (const std::runtime_error& error) {
                    require(std::string(error.what()).find("template identity") != std::string::npos, "Invalid template cause differs");
                    rejected = true;
                }
                require(rejected, "Invalid native template accepted");
            }
            ++checks;
        }
        {
            WizardLatch latch; int calls = 0;
            std::string name = "Edited world"; unsigned seed = 817;
            auto observe = [] { return WizardSnapshot{1, 2, true}; };
            auto launch = [&](bool& attempted) { attempted = true; ++calls; name = "Reset"; seed = 0; };
            require(latch.open(observe, launch) == WizardResult::already_open, "Visible wizard was not acknowledged");
            require(calls == 0 && name == "Edited world" && seed == 817, "Visible wizard edits were reset"); ++checks;
        }
        {
            WizardLatch latch; bool visible = false; int calls = 0;
            auto observe = [&] { return WizardSnapshot{1, 2, visible}; };
            auto launch = [&](bool& attempted) { attempted = true; ++calls; visible = true; };
            require(latch.open(observe, launch) == WizardResult::opened, "Hidden wizard was not opened");
            require(latch.open(observe, launch) == WizardResult::already_open && calls == 1, "Repeated opening reset the wizard"); ++checks;
        }
        {
            WizardLatch latch; bool visible = false; int calls = 0;
            auto observe = [&] { return WizardSnapshot{1, 2, visible}; };
            auto launch = [&](bool& attempted) { attempted = true; ++calls; };
            failure([&] { latch.open(observe, launch); }, "visibility", true, true);
            failure([&] { latch.open(observe, launch); }, "Previous", true, false);
            require(calls == 1, "Unconfirmed launch was replayed");
            visible = true;
            require(latch.open(observe, launch) == WizardResult::already_open && calls == 1, "Passive recovery relaunched the wizard"); ++checks;
        }
        {
            WizardLatch latch; int calls = 0;
            auto observe = [] { return WizardSnapshot{1, 2, false}; };
            auto launch = [&](bool& attempted) { attempted = true; ++calls; throw std::runtime_error("original native exception 37"); };
            failure([&] { latch.open(observe, launch); }, "original native exception 37", true, true);
            failure([&] { latch.open(observe, launch); }, "Previous", true, false);
            require(calls == 1, "Native exception caused a second execution"); ++checks;
        }
        {
            WizardLatch latch; uintptr_t owner = 1;
            auto observe = [&] { return WizardSnapshot{owner, 2, owner != 1}; };
            auto launch = [&](bool& attempted) { attempted = true; owner = 3; };
            failure([&] { latch.open(observe, launch); }, "owner changed", true, true); ++checks;
        }
        {
            WizardLatch latch; bool reject = true, visible = false;
            auto observe = [&] { return WizardSnapshot{1, 2, visible}; };
            auto launch = [&](bool& attempted) { if (reject) throw std::runtime_error("transition rejected before execution"); attempted = true; visible = true; };
            failure([&] { latch.open(observe, launch); }, "transition rejected", false, false);
            reject = false;
            require(latch.open(observe, launch) == WizardResult::opened, "Rejected request incorrectly quarantined an unexecuted action"); ++checks;
        }
        {
            WizardLatch latch; int calls = 0; bool confirm = false, visible = false;
            auto observe = [&] { return WizardSnapshot{1, 2, visible}; };
            auto launch = [&](bool& attempted) { attempted = true; ++calls; visible = confirm; };
            failure([&] { latch.open(observe, launch); }, "visibility", true, true);
            latch.reset_epoch(); confirm = true;
            require(latch.open(observe, launch) == WizardResult::opened && calls == 2, "A new native mode epoch retained an old launch quarantine"); ++checks;
        }
        {
            WizardLatch latch; int calls = 0;
            auto observe = []() -> WizardSnapshot { throw std::runtime_error("stale Title owner"); };
            try { latch.open(observe, [&](bool&) { ++calls; }); throw std::logic_error("Guard did not reject"); }
            catch (const std::runtime_error& error) { require(std::string(error.what()) == "stale Title owner", "Pre-launch guard cause was lost"); }
            require(calls == 0, "Invalid owner executed the native launcher"); ++checks;
        }
        {
            OpenRequest first, second; first.active = second.active = true;
            first.launch(); first.launch();
            require(!second.consume(), "A different mod consumed another app's launch");
            require(first.consume() && !first.consume(), "Desktop launch was not consumed once"); ++checks;
        }
        {
            OpenRequest app; app.active = true; app.launch(); app.retire(); app.active = true;
            require(!app.consume(), "Retired owner's pending launch reached its replacement");
            app.retire();
            try { app.launch(); throw std::logic_error("Retired app launch accepted"); }
            catch (const std::runtime_error& error) { require(std::string(error.what()) == "Desktop app owner is inactive", "Retired app rejection differs"); }
            require(!app.consume(), "Inactive app delivered an open request"); ++checks;
        }
        std::cout << "native app state checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) { std::cerr << "error: " << error.what() << '\n'; return 1; }
}
