Json close_native_app(const std::string& owner, const Json& args) {
    require(fault.empty(), fault);
    require(thread == GetCurrentThreadId() && mode && panel && runtime,
        "Native app close requires the current game thread and mode");
    require(!args.contains("text"), "Native app close does not accept text");
    auto registration = apps.find(owner);
    auto found = native_windows.find(owner);
    require(registration != apps.end() && found != native_windows.end(), "Native app is not registered");
    auto& window = found->second;
    require(window.owner && window.owner == registration->second.owner,
        "Native app close registration owner differs");
    bool active = false;
    for (const auto& plugin : runtime->plugins())
        if (plugin.get() == window.owner && plugin->active) active = true;
    require(active, "Native app mod is no longer active");
    require(window.panel && window.mode == mode, "Native app belongs to another mode");
    require(!panel_open && window.requested && !window.suspended,
        "Native app close requires an open unsuspended app");
    void* registered = nullptr;
    check(window_lookup(&registered, window.name.c_str()), "Native app close registry lookup");
    require(registered == window.panel, "Native app close registry owner differs");
    uint8_t visible = 0;
    require(read(window.panel, 0x45f, visible) && visible == 1,
        "Native app close requires a visible window");
    // Window::FindPane excludes its separately owned titlebar.
    void* titlebar = nullptr;
    void* titlebar_window = nullptr;
    require(read(window.panel, 0x3b8, titlebar) && titlebar,
        "Native app titlebar is unavailable");
    require(read(titlebar, 0x280, titlebar_window) && titlebar_window == window.panel,
        "Native app titlebar belongs to another window");
    void* target = nullptr;
    check(string_call(0x816150, titlebar, "close", &target), "Native app titlebar close lookup");
    require(target, "Native app titlebar close button is missing");
    void* target_window = nullptr;
    uint8_t disabled = 0, shown = 0, entity_shown = 0;
    std::string native_command;
    require(read(target, 0x280, target_window) && target_window == window.panel,
        "Native app close button belongs to another window");
    const bool disabled_read = read(target, 0x46d, disabled);
    const bool shown_read = read(target, 0x2b4, shown);
    const bool entity_shown_read = read(target, 0x69, entity_shown);
    // Window OnShow does not visit the separately owned titlebar. Its native
    // drawing and click selection use entity visibility instead.
    require(disabled_read && disabled == 0 && shown_read && shown <= 1 &&
        entity_shown_read && entity_shown == 1,
        "Native app close button is disabled or hidden: disabled=" +
        (disabled_read ? std::to_string(disabled) : "unreadable") + " shown=" +
        (shown_read ? std::to_string(shown) : "unreadable") + " entity_shown=" +
        (entity_shown_read ? std::to_string(entity_shown) : "unreadable"));
    require(text(static_cast<unsigned char*>(target) + 0x388, native_command) &&
        native_command == "MessageTo Window Close", "Native app close button command differs");

    // The titlebar owns this button. Its event does not enter the mod widget queue.
    auto* native_window = window.panel;
    auto* native_mode = mode;
    const auto error = activate_native(target, false);
    Json result{{"native_event", "mmoButtonPane::Activate"}, {"target", "app-close:" + owner},
        {"native_attempted", true}, {"native_returned", error == 0}, {"exception_code", error},
        {"close_confirmed", false}, {"visibility_known", false}, {"visible", nullptr},
        {"result", "unknown"}};
    if (error) { log_error("Native app close activation exception=" + std::to_string(error)); return result; }
    registered = nullptr;
    const auto lookup_error = window_lookup(&registered, window.name.c_str());
    if (lookup_error || mode != native_mode || window.panel != native_window || registered != native_window) {
        result["cause"] = "Native app close registry or mode changed after activation";
        result["lookup_exception_code"] = lookup_error;
        log_error(result.at("cause").get<std::string>()); return result;
    }
    if (!read(native_window, 0x45f, visible) || visible > 1) {
        result["cause"] = "Native app close visibility is unreadable after activation";
        log_error(result.at("cause").get<std::string>()); return result;
    }
    result["visibility_known"] = true; result["visible"] = visible != 0;
    result["close_confirmed"] = visible == 0;
    result["result"] = visible == 0 ? "confirmed" : "rejected";
    if (visible) {
        result["cause"] = "Native app close returned but the window remains visible";
        log_error(result.at("cause").get<std::string>());
    }
    return result;
}
