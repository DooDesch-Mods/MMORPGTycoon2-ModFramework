    Json events{{"ready", true},{"open",desired},{"visible",nullptr},{"visibility_known",false},{"suspended",window.suspended},{"clicked",Json::array()},{"values",Json::object()}};
    if (!window.panel) return events;
    if (!desired || panel_open) {
        if (window.requested || panel_open) check(show(window.panel, false), "Hide native app");
        window.requested = desired; window.suspended = desired && panel_open; events["open"] = desired;
        uint8_t actual=0;require(read(window.panel,0x45f,actual),"Native app hidden visibility unreadable");events["visible"]=actual!=0;events["visibility_known"]=true;events["suspended"]=window.suspended;return events;
    }
    if (!window.requested || window.suspended) check(show(window.panel, true), "Open native app");
    window.requested = true; window.suspended = false;
    uint8_t visible = 0; require(read(window.panel, 0x45f, visible), "Native app visibility unreadable"); events["open"] = visible != 0;
    events["visible"]=visible!=0;events["visibility_known"]=true;events["suspended"]=false;
    if (!visible) { window.clicked = Json::array(); return events; }
