// Copyright (c) 2026 DooDesch. Native event checks reuse the authored Multiplayer diagnostic contract.
struct AppIconState {
    std::array<float,2> position{}, entity{}, dimensions{};
    uint8_t dragging = 0, armed = 0;
    uint64_t click_ms = 0;
};
AppIconState app_icon_state(void* icon, void* record) {
    AppIconState state;
    require(read(record,0x3c,state.position) && read(icon,0x110,state.entity) && read(icon,0x138,state.dimensions) &&
        read(icon,0x358,state.dragging) && read(icon,0x359,state.armed) && read(icon,0x350,state.click_ms),
        "App icon event state is unreadable");
    for (const auto* pair : {&state.position,&state.entity,&state.dimensions})
        for (float value : *pair) require(std::isfinite(value), "App icon event geometry is non-finite");
    require(state.dragging <= 1 && state.armed <= 1, "App icon event flags differ");
    return state;
}
Json app_icon_state_json(const AppIconState& state) {
    return {{"record_position",state.position},{"entity_position",state.entity},{"dimensions",state.dimensions},
        {"dragging",state.dragging!=0},{"click_armed",state.armed!=0},{"last_click_ms",state.click_ms}};
}
DWORD app_icon_mouse_down(void* cursor, bool* down) {
    __try { *down = reinterpret_cast<bool(*)(const void*)>(image+0x5853e0)(cursor); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); } return 0;
}
DWORD app_icon_event(uintptr_t rva, void* icon, void* cursor, const std::array<float,2>& point) {
    __try { reinterpret_cast<void(*)(void*,void*,const void*)>(image+rva)(icon,cursor,point.data()); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); } return 0;
}
Json probe_app_icon(const std::string& owner, const Json& args) {
    const auto event = args.value("event",std::string("double"));
    require(event=="single" || event=="double" || event=="drag", "Unknown app icon diagnostic event");
    native::WorldStatus world; std::string world_error;
    require(native::read_world_status(world,world_error),world_error);
    require(world.active_mode_index==2 && world.requested_mode_index==2 && !world.title_transition_pending,
        "App icon diagnostic requires actual idle Title with no pending mode transition");
    auto found = apps.find(owner); require(found!=apps.end(), "App icon owner is not registered");
    auto& app = found->second; auto binding = icons.find(app.icon);
    require(app.desktop && app.icon_error.empty() && binding!=icons.end() && binding->second.plugin==app.owner &&
        binding->second.desktop==current_desktop && mode_index==2 && icon_identity(app.icon,binding->second) && entry_visible(app.icon,true),
        "App icon diagnostic requires its current visible Title Desktop entry");
    bool active = false; for (const auto& plugin : runtime->plugins()) if(plugin.get()==app.owner && plugin->active) active=true;
    require(active, "App icon diagnostic owner is inactive");
    void* const target_icon = app.icon;
    const Binding retained_binding = binding->second;
    int native_type = 0;
    require(read(retained_binding.record,0x38,native_type) && native_type==6, "App icon native record type differs");
    const auto still_owned = [&]() { auto current=icons.find(target_icon); return current!=icons.end() &&
        current->second.desktop==retained_binding.desktop && current->second.record==retained_binding.record &&
        current->second.plugin==app.owner && icon_identity(target_icon,retained_binding); };
    const auto before = app_icon_state(target_icon,retained_binding.record);
    require(!before.dragging, "App icon is already dragging; no replay");
    const auto launches = app.launches; const auto errors = command_errors;
    Json result{{"target","app-icon:"+owner},{"event",event},{"native_attempted",true},
        {"native_returned",false},{"confirmed",false},{"result","unknown"},{"before",app_icon_state_json(before)}};
    if(event=="double") {
        const auto code = activate_native(target_icon,true);
        const bool reached = app.launches==launches+1 && app.request.pending;
        result["exception_code"]=code; result["native_returned"]=code==0;
        result["handler_reached"]=reached; result["confirmed"]=!code && reached && errors==command_errors;
        result["native_event"]="mmoDesktopIcon::DoubleClick";
        result["result"]=result.at("confirmed").get<bool>()?"confirmed":"unknown";
        if(!result.at("confirmed").get<bool>()) { result["cause"]="App icon double-click did not confirm its owner-specific open request"; log_error(result.at("cause").get<std::string>()); }
        return result;
    }
    void* cursor=nullptr; uintptr_t cursor_class=0; void* trap=nullptr; void* clicked=nullptr;
    std::array<float,2> cursor_before{}, desktop_size{};
    require(read(reinterpret_cast<void*>(image),0x13df9d0,cursor) && read(cursor,0,cursor_class) && cursor_class==image+0x1786dd0 &&
        read(cursor,0x1d8,trap) && read(cursor,0x1c0,clicked) && read(cursor,0x1f0,cursor_before) && read(current_desktop,0x138,desktop_size),
        "App icon native cursor/geometry differs");
    bool down=false; const auto idle=app_icon_mouse_down(cursor,&down);
    require(!idle && !down && !trap && !clicked, "App icon diagnostic requires an idle uncaptured cursor; exception="+std::to_string(idle));
    void* slot=nullptr; void* clock=nullptr; uint64_t micros=0;
    require(read(reinterpret_cast<void*>(image),0x164b160,slot) && read(slot,0,clock) && read(clock,0x20,micros), "App icon native click clock is unreadable");
    const auto now=micros/1000;
    const auto elapsed=now>=before.click_ms?now-before.click_ms:0;
    require(!before.armed || (now>=before.click_ms && elapsed>499), "App icon first-click interval measured_ms="+std::to_string(elapsed)+" required_ms>499");
    const bool drag=event=="drag"; float delta=0;
    if(drag) delta=before.position[0]>=24.f?-20.f:20.f;
    for(float value:desktop_size) require(std::isfinite(value), "App icon Desktop dimensions are non-finite");
    require(before.dimensions[0]>0 && before.dimensions[1]>0 && before.position[0]+delta>=0 && before.position[1]>=0 &&
        before.position[0]+delta+before.dimensions[0]<=desktop_size[0] && before.position[1]+before.dimensions[1]<=desktop_size[1],
        "App icon bounded event falls outside its Desktop");
    const std::array<float,2> press{before.entity[0]+before.dimensions[0]*0.5f,before.entity[1]+24.f};
    const std::array<float,2> drop{press[0]+delta,press[1]};
    result["cursor_fields_written"]=false; result["cursor_screen_before"]=cursor_before;
    result["native_event"]=drag?"mmoDesktopIcon::Press/Drag/Release":"mmoDesktopIcon::Press/Release";
    const auto press_code=app_icon_event(0x6094b0,target_icon,cursor,press);
    DWORD drag_code=0,release_code=0; bool moved_started=!drag; std::string cause;
    std::optional<AppIconState> pressed;
    try { require(still_owned(), "App icon owner changed after Press"); pressed=app_icon_state(target_icon,retained_binding.record); result["after_press"]=app_icon_state_json(*pressed); }
    catch(const std::exception& error) { cause=error.what(); }
    const bool drag_attempted=drag && !press_code && pressed && still_owned() && app.launches==launches && errors==command_errors;
    if(drag_attempted) {
        drag_code=app_icon_event(0x608bd0,target_icon,cursor,drop);
        try { require(still_owned(), "App icon owner changed during Drag"); const auto during=app_icon_state(target_icon,retained_binding.record); moved_started=during.dragging; result["during_drag"]=app_icon_state_json(during); }
        catch(const std::exception& error) { cause+=std::string("; drag read: ")+error.what(); }
    }
    const bool release_owned=still_owned();
    if(release_owned) release_code=app_icon_event(0x607a60,target_icon,cursor,drag_attempted?drop:press);
    else cause+="; app icon owner changed before Release";
    std::optional<AppIconState> after;
    if(release_owned) try { require(still_owned(), "App icon owner changed after Release"); after=app_icon_state(target_icon,retained_binding.record); result["after"]=app_icon_state_json(*after); }
    catch(const std::exception& error) { cause+=std::string("; release read: ")+error.what(); }
    std::array<float,2> cursor_after{}; const bool cursor_read=read(cursor,0x1f0,cursor_after);
    const bool returned=!press_code && !drag_code && release_owned && !release_code;
    const bool moved=after && std::abs(after->position[0]-before.position[0]-delta)<0.5f && std::abs(after->position[1]-before.position[1])<0.5f;
    const bool confirmed=returned && pressed && pressed->armed && !pressed->dragging && after && !after->dragging && moved && moved_started &&
        cause.empty() && app.launches==launches && errors==command_errors && cursor_read && cursor_before==cursor_after;
    result.update({{"native_returned",returned},{"confirmed",confirmed},{"sequence_confirmed",confirmed},{"drag_attempted",drag_attempted},
        {"release_attempted",release_owned},{"press_exception",press_code},{"drag_exception",drag_code},{"release_exception",release_code},
        {"cursor_screen_after",cursor_after},{"launches_unchanged",app.launches==launches},{"result",confirmed?"confirmed":"unknown"}});
    if(!confirmed) { result["cause"]="App icon event unconfirmed: press="+std::to_string(press_code)+" drag="+std::to_string(drag_code)+" release="+std::to_string(release_code)+" "+cause; log_error(result.at("cause").get<std::string>()); }
    return result;
}
bool wizard_class(void* pane, uintptr_t address_point) {
    uintptr_t actual=0; return read(pane,0,actual) && actual==image+address_point;
}
void* wizard_child(void* parent, void* window, uintptr_t address_point) {
    void* found=nullptr;
    for(const auto offset : {size_t(0x2d0),size_t(0x300)}) {
        void* array=nullptr; int count=0;
        require(read(parent,offset,array) && read(parent,offset+8,count) && count>=0 && count<=128,
            "Wizard icon ancestor child array differs");
        require(!count || readable(array,size_t(count)*sizeof(void*)), "Wizard icon ancestor children are unreadable");
        for(int i=0;i<count;++i) {
            void* child=nullptr; require(read(array,size_t(i)*sizeof(void*),child),"Wizard icon child is unreadable");
            if(!wizard_class(child,address_point)) continue;
            void* actual_parent=nullptr; void* owner=nullptr;
            require(read(child,0x278,actual_parent) && actual_parent==parent && read(child,0x280,owner) && owner==window,
                "Wizard icon child ancestry differs");
            require(!found,"Wizard icon ancestor contains duplicate matching children"); found=child;
        }
    }
    require(found,"Wizard icon ancestor has no expected original child"); return found;
}
DWORD wizard_icon_name(void* manager, int index, void** output) {
    __try { *output=reinterpret_cast<void*(*)(const void*,int)>(image+0x8576a0)(static_cast<unsigned char*>(manager)+8,index); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); } return 0;
}
DWORD wizard_select_icon(void* list, void* selector) {
    __try { reinterpret_cast<void(*)(void*,void*)>(image+0x6c0f80)(list,selector); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); } return 0;
}
Json probe_wizard_icon(const std::string& target, void* window, const Json& args) {
    void* carousel=nullptr; void* current_page=nullptr;
    auto* name_page=native_pane(window,"desktop_newgame_name_displayname");
    require(read(window,0x548,carousel) && carousel==native_pane(window,"multi") &&
        read(carousel,0x428,current_page) && current_page==name_page,"Wizard icon selection requires the original Name page");
    require(wizard_class(name_page,0x1745a80),"Wizard Name page class differs");
    void* group=nullptr; check(string_call(0x816150,name_page,"Icon",&group),"Wizard Name page icon group lookup");
    void* owner=nullptr; uint8_t shown=0;
    require(wizard_class(group,0x1745a80) && read(group,0x280,owner) && owner==window && read(group,0x69,shown) && shown==1,
        "Wizard icon group class, owner or visibility differs");
    auto* container=wizard_child(group,window,0x171bf80);
    auto* scroll=wizard_child(container,window,0x16e6e10);
    auto* list=wizard_child(scroll,window,0x1718d60);
    void* items=nullptr; int count=0; int capacity=0;
    require(read(list,0x320,items) && read(list,0x328,count) && read(list,0x32c,capacity) && count>0 && count<=256 &&
        capacity>=count && capacity<=1024 && readable(items,size_t(count)*sizeof(void*)),"Wizard original icon list storage differs");
    const auto ordinal=args.value("icon_ordinal",0);
    require(ordinal>=0 && ordinal<count,"Wizard icon ordinal is outside the existing list");
    void* selector=nullptr;
    for(int i=0;i<count;++i) {
        void* item=nullptr; void* back=nullptr; void* parent=nullptr;
        require(read(items,size_t(i)*sizeof(void*),item) && wizard_class(item,0x17301a0) &&
            read(item,0x588,back) && back==list && read(item,0x278,parent) && parent==list && read(item,0x280,owner) && owner==window,
            "Wizard original icon member identity or owner differs");
        if(i==ordinal) selector=item;
    }
    uint8_t disabled=1; int index=-1; void* manager=nullptr; int icon_count=0;
    require(read(selector,0x46d,disabled) && !disabled && read(selector,0x69,shown) && shown==1 && read(selector,0x590,index) &&
        read(reinterpret_cast<void*>(image),0x13df7e0,manager) && read(manager,0x38,icon_count) &&
        icon_count>0 && icon_count<=4096 && index>=0 && index<icon_count,"Wizard selected icon enabled state or original index differs");
    void* native_name=nullptr; check(wizard_icon_name(manager,index,&native_name),"Wizard original icon name query");
    std::string expected,before,name;
    require(text(native_name,expected) && !expected.empty() && expected.size()<=128 &&
        text(static_cast<unsigned char*>(window)+0x570,name) && !name.empty() &&
        text(static_cast<unsigned char*>(window)+0x590,before),"Wizard original name or icon text is unreadable");
    Json result{{"target","wizard:"+target},{"native_event","mmoDesktopIconList::SetSelected"},
        {"native_attempted",false},{"native_returned",false},{"confirmed",false},{"world_creation_confirmed",false},
        {"icon_ordinal",ordinal},{"icon_index",index},{"list_count",count},{"icon_before",before},{"expected_icon",expected},
        {"guard_passed",true},{"result","observed"}};
    if(args.value("observe",false)) return result;
    if(!before.empty()) { result.update({{"result","preserved_existing_selection"},{"selection_preserved",true}}); return result; }
    const auto code=wizard_select_icon(list,selector);
    result.update({{"native_attempted",true},{"native_returned",code==0},{"exception_code",code},{"result","unknown"}});
    if(code) { result["cause"]="Wizard original icon selection exception="+std::to_string(code); log_error(result.at("cause").get<std::string>()); return result; }
    void* registered=nullptr; void* selected_icon=nullptr; uint8_t selected_flag=0; std::string after;
    const auto lookup=window_lookup(&registered,"NewGame");
    const bool confirmed=!lookup && registered==window && wizard_class(window,0x1707fb0) &&
        read(list,0x330,selected_icon) && selected_icon==selector && read(selector,0x585,selected_flag) && selected_flag==1 &&
        text(static_cast<unsigned char*>(window)+0x590,after) && after==expected;
    result.update({{"confirmed",confirmed},{"icon_after",after},{"result",confirmed?"confirmed":"unknown"}});
    if(!confirmed) { result["cause"]="Wizard original icon selection did not confirm selected member and exact icon name; registry_exception="+std::to_string(lookup); log_error(result.at("cause").get<std::string>()); }
    return result;
}
DWORD wizard_game_type(void* window, int ordinal, void** output) {
    __try {
        auto** item=reinterpret_cast<void**(*)(void*,int)>(image+0xb3c000)(static_cast<unsigned char*>(window)+0x4f8,ordinal);
        *output=item?*item:nullptr;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); } return 0;
}
Json probe_wizard_type(const std::string& target, void* window, const Json& args) {
    void* carousel=nullptr; void* current_page=nullptr;
    auto* type_page=native_pane(window,"desktop_newgame_type_displayname");
    require(read(window,0x548,carousel) && carousel==native_pane(window,"multi") &&
        read(carousel,0x428,current_page) && current_page==type_page,"Wizard type selection requires the original Type page");
    int count=0; const auto ordinal=args.value("type_ordinal",0);
    require(read(window,0x508,count) && count>0 && count<=256,"Wizard original game-type count is absent or out of bounds");
    require(ordinal>=0 && ordinal<count,"Wizard game-type ordinal is outside the existing list");
    void* type=nullptr; check(wizard_game_type(window,ordinal,&type),"Wizard original game-type query");
    std::string expected,before;
    require(type && text(static_cast<unsigned char*>(type)+0x18,expected) && !expected.empty() && expected.size()<=128 &&
        expected.find_first_of("\"\\\r\n\t")==std::string::npos && expected.find('\0')==std::string::npos,
        "Wizard existing game-type identifier is absent or invalid");
    require(text(static_cast<unsigned char*>(window)+0x5b0,before),"Wizard current game-type property is unreadable");
    Json result{{"target","wizard:"+target},{"native_event","mmoPane::Command SetCreateGameType"},
        {"native_attempted",false},{"native_returned",false},{"confirmed",false},{"world_creation_confirmed",false},
        {"type_ordinal",ordinal},{"type_count",count},{"type_before",before},{"expected_type",expected},
        {"guard_passed",true},{"result","observed"}};
    if(args.value("observe",false)) return result;
    if(!before.empty()) { result.update({{"result","preserved_existing_selection"},{"selection_preserved",true}}); return result; }
    const std::string command="MessageTo WindowName \"NewGame\" SetCreateGameType \""+expected+"\"";
    const auto code=string_call(0x770300,window,command.c_str());
    result.update({{"native_attempted",true},{"native_returned",code==0},{"exception_code",code},{"result","unknown"}});
    if(code) { result["cause"]="Wizard original game-type command exception="+std::to_string(code); log_error(result.at("cause").get<std::string>()); return result; }
    void* registered=nullptr; std::string after;
    const auto lookup=window_lookup(&registered,"NewGame");
    const bool confirmed=!lookup && registered==window && wizard_class(window,0x1707fb0) &&
        text(static_cast<unsigned char*>(window)+0x5b0,after) && after==expected;
    result.update({{"confirmed",confirmed},{"type_after",after},{"result",confirmed?"confirmed":"unknown"}});
    if(!confirmed) { result["cause"]="Wizard original game-type command did not confirm exact selected identifier; registry_exception="+std::to_string(lookup); log_error(result.at("cause").get<std::string>()); }
    return result;
}
Json probe_wizard_state(const std::string& target, void* window) {
    void* carousel=nullptr; void* registered_carousel=nullptr; void* current=nullptr; void* owner=nullptr;
    require(read(window,0x548,carousel) && carousel,"Wizard state Carousel link is unreadable or absent");
    check(string_call(0x81d860,window,"multi",&registered_carousel),"Wizard state original Carousel lookup");
    require(registered_carousel==carousel && read(carousel,0x280,owner) && owner==window,"Wizard state Carousel identity or owner differs");
    require(read(carousel,0x428,current) && current,"Wizard state current page is unreadable or absent");
    require(read(current,0x280,owner) && owner==window,"Wizard state current page owner differs");
    const std::array<const char*,6> pages{"desktop_newgame_name_displayname","desktop_newgame_type_displayname",
        "desktop_newgame_affinity_displayname","desktop_newgame_map_displayname","desktop_newgame_logo_displayname",
        "desktop_newgame_summary_displayname"};
    int matches=0,page=0;
    for(size_t index=0;index<pages.size();++index) {
        void* found=nullptr; check(string_call(0x81d860,window,pages[index],&found),std::string("Wizard state original page lookup ")+pages[index]);
        if(found==current) { ++matches; page=int(index)+1; }
    }
    require(matches==1,"Wizard state current page match is ambiguous or missing; matched="+std::to_string(matches)+" required=1");
    void* task=nullptr; void* layout=nullptr; uint8_t dirty=1; int32_t regions=0;
    require(read(window,0x688,task),"Wizard state map task is unreadable");
    require(read(window,0x690,dirty),"Wizard state map dirty flag is unreadable");
    require(dirty<=1,"Wizard state map dirty flag is not a canonical boolean");
    require(read(window,0x678,layout),"Wizard state map layout is unreadable");
    require(!layout || read(layout,0x80,regions),"Wizard state map region count is unreadable");
    const bool map_ready=layout && !task && !dirty && regions>0;
    return {{"target","wizard:"+target},{"status","observed"},{"result","observed"},{"confirmed",true},{"native_attempted",false},
        {"native_returned",false},{"world_creation_confirmed",false},{"page",page},{"page_count",pages.size()},
        {"page_id",pages[size_t(page-1)]},{"map_ready",map_ready},
        {"map",{{"task_pending",task!=nullptr},{"dirty",dirty!=0},{"layout_present",layout!=nullptr},
            {"regions",layout?Json(regions):Json(nullptr)}}}};
}
struct WizardAffinityCode {uintptr_t rva;size_t size;const char* sha;};
constexpr WizardAffinityCode wizard_affinity_codes[]{
    {0x674780,1008,"5505cdfa2a51f468b54c61cc5f79ab121499fb4c841f69a84d3a7ab4e6715f2c"},
    {0x7d3fc0,240,"cd5ee00e3fdf3625ac209600ae90cb9ac8b39d5f0a7a72e2a6e08ee5f50edfd0"},
    {0x7d31e0,3472,"dd06563be8826fce57ef99d9dc27ba22fd34f2e74824cc06d75061880253c6af"},
    {0x7d1720,1376,"aa55d20f93f082e6669525cd3c22e67b910d4a288a2bee1d8f9439c9e7b1925e"},
    {0x75b870,5888,"345c976eaecce2bf982550b0aab1b1994cc800273b2355eb4ecd9a198ad22983"},
    {0x75dc80,2544,"d3cfce799b94322cb54f1565282633078d8ef2fa2f0b5390ed939333db3d85af"},
};
struct WizardAffinityState {
    void* page=nullptr; void* view=nullptr;
    std::array<void*,4> items{}, buttons{};
    std::array<int32_t,10> raw_base{}, raw_allocation{};
    std::array<int32_t,4> base{}, allocation{}, displayed{}, indices{};
    int32_t first=-1,last=-1,budget=-1,remaining=-1;
    Json rows=Json::array();
};
std::map<std::string,Json> wizard_affinity_attempts;
std::map<std::string,std::string> wizard_affinity_faults;
void wizard_affinity_descendant(void* child,void* ancestor,void* window) {
    std::array<void*,16> seen{}; size_t count=0;
    for(auto* current=child;current!=ancestor;) {
        require(current && count<seen.size(),"Wizard affinity ancestry is absent or exceeds16 links");
        for(size_t i=0;i<count;++i)require(seen[i]!=current,"Wizard affinity ancestry is cyclic");
        seen[count++]=current;void* owner=nullptr;void* parent=nullptr;
        require(read(current,0x280,owner) && owner==window && read(current,0x278,parent),
            "Wizard affinity ancestor owner or parent differs");current=parent;
    }
}
void wizard_affinity_code_guards() {
    for(const auto& code:wizard_affinity_codes) {
        auto* pointer=reinterpret_cast<const char*>(image+code.rva);
        require(readable(pointer,code.size),"Wizard affinity native body is unreadable");
        require(digest(std::string(pointer,code.size))==code.sha,"Wizard affinity native body differs RVA="+std::to_string(code.rva));
    }
}
WizardAffinityState wizard_affinity_state(void* window) {
    require(wizard_class(window,0x1707fb0),"Wizard affinity NewGame class differs");
    WizardAffinityState state;void* carousel=nullptr;void* current=nullptr;void* owner=nullptr;uint8_t shown=0,editable=0;
    state.page=native_pane(window,"desktop_newgame_affinity_displayname");
    require(wizard_class(state.page,0x1745a80) && read(window,0x548,carousel) && carousel==native_pane(window,"multi") &&
        read(carousel,0x280,owner) && owner==window && read(carousel,0x428,current) && current==state.page &&
        read(state.page,0x280,owner) && owner==window && read(state.page,0x69,shown) && shown==1,
        "Wizard affinity requires its actual visible owned Player Appeal page");
    state.view=native_pane(window,"statistics_view");
    require(wizard_class(state.view,0x17157a0) && read(state.view,0x31c,editable) && editable==1,
        "Wizard affinity StatisticsView class/editable differs");
    wizard_affinity_descendant(state.view,state.page,window);
    require(read(state.view,0x314,state.first) && read(state.view,0x318,state.last) &&
        state.first>=0 && state.last<10 && state.last-state.first==3 &&
        read(state.view,0x320,state.raw_base) && read(state.view,0x34c,state.raw_allocation) &&
        read(state.view,0x348,state.budget) && read(state.view,0x398,state.remaining) &&
        state.budget>=0 && state.budget<=1000000 && state.remaining>=0 && state.remaining<=state.budget,
        "Wizard affinity actual statistic range/base/allocation/remaining differs");
    void* items=nullptr;int32_t count=0,capacity=0;int64_t assigned=0;
    require(read(state.view,0x380,items) && read(state.view,0x388,count) && count==4 &&
        read(state.view,0x38c,capacity) && capacity>=count && capacity<=128 && readable(items,size_t(count)*sizeof(void*)),
        "Wizard affinity actual item array differs");
    for(size_t ordinal=0;ordinal<state.items.size();++ordinal) {
        const auto index=state.first+int32_t(ordinal);state.indices[ordinal]=index;
        state.base[ordinal]=state.raw_base[size_t(index)];state.allocation[ordinal]=state.raw_allocation[size_t(index)];
        require(state.base[ordinal]>=0 && state.base[ordinal]<=1000000 && state.allocation[ordinal]>=0 && state.allocation[ordinal]<=1000000,
            "Wizard affinity statistic value is outside the diagnostic bound");
        state.displayed[ordinal]=state.base[ordinal]+state.allocation[ordinal];assigned+=state.allocation[ordinal];
        auto*& item=state.items[ordinal];auto*& button=state.buttons[ordinal];int32_t actual=-1;
        require(read(items,ordinal*sizeof(void*),item) && wizard_class(item,0x1715540) && read(item,0x340,actual) && actual==index &&
            read(item,0x330,button) && wizard_class(button,0x16e3ec0),"Wizard affinity item/index/up-button class differs");
        for(size_t previous=0;previous<ordinal;++previous)require(state.items[previous]!=item && state.buttons[previous]!=button,
            "Wizard affinity item or up-button is duplicated");
        wizard_affinity_descendant(item,state.view,window);
        void* child=nullptr;check(string_call(0x816150,item,"up",&child),"Wizard affinity original up-button lookup");
        void* parent=nullptr;uint8_t disabled=2;std::string command;
        require(child==button && read(button,0x278,parent) && parent==item && read(button,0x280,owner) && owner==window &&
            read(button,0x69,shown) && shown==1 && read(button,0x46d,disabled) && disabled<=1 &&
            text(static_cast<unsigned char*>(button)+0x388,command),"Wizard affinity original up-button owner/state/command differs");
        const auto expected="MessageTo Pane statistics_view Increment "+std::to_string(index);
        require(command==expected,"Wizard affinity up-button command does not match its actual statistic index");
        state.rows.push_back({{"ordinal",ordinal},{"statistic_index",index},{"command",command},{"disabled",disabled}});
    }
    require(int64_t(state.budget)-assigned==state.remaining,"Wizard affinity remaining differs from actual budget minus allocations");
    void* after_items=nullptr;int32_t after_count=0,after_capacity=0;
    require(read(state.view,0x380,after_items) && after_items==items && read(state.view,0x388,after_count) && after_count==count &&
        read(state.view,0x38c,after_capacity) && after_capacity==capacity,"Wizard affinity item header changed during read");
    for(size_t ordinal=0;ordinal<state.items.size();++ordinal) {
        void* item=nullptr;require(read(items,ordinal*sizeof(void*),item) && item==state.items[ordinal],
            "Wizard affinity current item membership changed during read");
    }
    return state;
}
Json wizard_affinity_json(const WizardAffinityState& state) {
    return {{"base",state.base},{"allocation",state.allocation},{"displayed",state.displayed},{"remaining",state.remaining},
        {"raw_base",state.raw_base},{"raw_allocation",state.raw_allocation},{"budget",state.budget}};
}
int wizard_affinity_next_disabled(void* window) {
    auto* next=native_pane(window,"Next");void* owner=nullptr;uint8_t disabled=2;
    require(wizard_class(next,0x16e3ec0) && read(next,0x280,owner) && owner==window &&
        read(next,0x46d,disabled) && disabled<=1,"Wizard affinity Next button class/owner/state differs");
    return disabled;
}
Json probe_wizard_affinity(const std::string& owner,const std::string& target,void* window,const Json& args) {
    require(!args.contains("text") && !args.contains("name") && !args.contains("seed"),"Wizard affinity does not accept text/name/seed edits");
    require(args.contains("affinity_ordinal") && args.at("affinity_ordinal").is_number_integer(),"Wizard affinity requires integer affinity_ordinal");
    const auto ordinal=args.at("affinity_ordinal").get<int64_t>();
    require(ordinal>=0 && ordinal<4,"Wizard affinity ordinal requires0..3");
    wizard_affinity_code_guards();
    const auto before=wizard_affinity_state(window);
    Json result{{"target","wizard:"+target},{"native_event","mmoButtonPane::Activate original affinity up button"},
        {"affinity_ordinal",ordinal},{"native_attempted",false},{"native_returned",false},{"confirmed",false},
        {"world_creation_confirmed",false},{"guard_passed",true},{"result","observed"},
        {"before",wizard_affinity_json(before)},{"rows",before.rows},{"next_disabled",wizard_affinity_next_disabled(window)}};
    if(args.value("observe",false)) {
        if(wizard_affinity_faults.contains(owner))result["allocation_fault"]=wizard_affinity_faults.at(owner);
        return result;
    }
    require(!wizard_affinity_faults.contains(owner),"Wizard affinity prior Unknown blocks further allocation; no replay/reset");
    require(args.contains("affinity_token") && args.at("affinity_token").is_string(),"Wizard affinity requires affinity_token");
    const auto token=args.at("affinity_token").get<std::string>();
    require(token.size()==36,"Wizard affinity token requires a canonical UUID");
    for(size_t i=0;i<token.size();++i)require((i==8||i==13||i==18||i==23)?token[i]=='-':
        ((token[i]>='0'&&token[i]<='9')||(token[i]>='a'&&token[i]<='f')),"Wizard affinity token requires a canonical lowercase UUID");
    require(!wizard_affinity_attempts.contains(token) && wizard_affinity_attempts.size()<64,"Wizard affinity token already attempted or budget reached; no replay");
    require(args.contains("expected_remaining") && args.at("expected_remaining").is_number_integer() &&
        args.at("expected_remaining").get<int64_t>()==before.remaining && before.remaining>0,
        "Wizard affinity actual remaining differs from positive expected_remaining");
    require(args.contains("expected_values") && args.at("expected_values").is_array() && args.at("expected_values").size()==4,
        "Wizard affinity requires four actual expected_values");
    for(size_t i=0;i<before.displayed.size();++i)require(args.at("expected_values")[i].is_number_integer() &&
        args.at("expected_values")[i].get<int64_t>()==before.displayed[i],"Wizard affinity actual displayed values differ from expected_values");
    require(before.rows.at(size_t(ordinal)).at("disabled")==0,"Wizard affinity actual selected up-button is disabled");
    result["affinity_token"]=token;result["native_attempted"]=true;result["result"]="unknown";
    wizard_affinity_attempts.emplace(token,result);
    const auto failed=[&](const std::string& cause) {
        result["confirmed"]=false;result["result"]="unknown";result["cause"]=cause;
        wizard_affinity_faults.emplace(owner,cause);wizard_affinity_attempts.at(token)=result;log_error(cause);return result;
    };
    const auto code=activate_native(before.buttons[size_t(ordinal)],false);
    result["native_returned"]=code==0;result["exception_code"]=code;wizard_affinity_attempts.at(token)=result;
    if(code)return failed("Wizard affinity original up-button exception="+std::to_string(code));
    try {
        void* registered=nullptr;check(window_lookup(&registered,"NewGame"),"Wizard affinity post-call registry");
        require(registered==window,"Wizard affinity original window changed after increment");
        const auto owned=new_world({{"mod_id",owner},{"observe",true}});
        require(owned.value("confirmed",false) && owned.value("visible",false),"Wizard affinity owner latch/visibility changed after increment");
        const auto after=wizard_affinity_state(window);result["after"]=wizard_affinity_json(after);
        result["next_disabled"]=wizard_affinity_next_disabled(window);
        require(after.page==before.page && after.view==before.view && after.items==before.items && after.buttons==before.buttons &&
            after.first==before.first && after.last==before.last && after.raw_base==before.raw_base && after.budget==before.budget &&
            after.remaining==before.remaining-1,"Wizard affinity original increment owner/base/budget/remaining transition differs");
        for(size_t index=0;index<after.raw_allocation.size();++index)require(after.raw_allocation[index]==
            before.raw_allocation[index]+(int32_t(index)==before.indices[size_t(ordinal)]?1:0),
            "Wizard affinity original increment changed the wrong allocation count");
        for(size_t index=0;index<after.displayed.size();++index)require(after.displayed[index]==before.displayed[index]+(index==size_t(ordinal)?1:0),
            "Wizard affinity original increment changed the wrong displayed affinity");
        result["confirmed"]=true;result["result"]="confirmed";wizard_affinity_attempts.at(token)=result;return result;
    }catch(const std::exception& error){return failed(std::string("Wizard affinity post-increment Unknown: ")+error.what());}
    catch(...){return failed("Wizard affinity post-increment non-standard Unknown");}
}

Json probe_wizard(const std::string& target, const Json& args) {
    const auto split=target.rfind(':'); require(split!=std::string::npos, "Wizard diagnostic requires owner:close, owner:create, owner:next, owner:select-icon, owner:select-type, owner:allocate-affinity or owner:state");
    const auto owner=target.substr(0,split), action=target.substr(split+1);
    require(action=="close" || action=="create" || action=="next" || action=="select-icon" || action=="select-type" || action=="allocate-affinity" || action=="state", "Wizard diagnostic action is not admitted");
    const auto observed=new_world({{"mod_id",owner},{"observe",true}});
    require(observed.value("confirmed",false) && observed.value("visible",false), "Wizard diagnostic requires its visible owned NewGame window");
    void* window=nullptr; check(window_lookup(&window,"NewGame"),"Wizard diagnostic registry"); require(window,"Wizard diagnostic window is unavailable");
    if(action=="select-icon") return probe_wizard_icon(target,window,args);
    if(action=="select-type") return probe_wizard_type(target,window,args);
    if(action=="allocate-affinity") return probe_wizard_affinity(owner,target,window,args);
    if(action=="state") return probe_wizard_state(target,window);
    void* button=nullptr; DWORD lookup_code=0;
    const std::string pane=action=="close"?"close":action=="next"?"Next":"Create";
    if(action=="close") {
        void* titlebar=nullptr; void* window_owner=nullptr;
        require(read(window,0x3b8,titlebar) && titlebar && read(titlebar,0x280,window_owner) && window_owner==window, "Wizard titlebar owner differs");
        lookup_code=string_call(0x816150,titlebar,pane.c_str(),&button);
    } else lookup_code=string_call(0x81d860,window,pane.c_str(),&button);
    void* button_owner=nullptr; uint8_t disabled=1,shown=0; std::string command;
    const bool owner_read=button && read(button,0x280,button_owner);
    const bool disabled_read=button && read(button,0x46d,disabled);
    const bool shown_read=button && read(button,0x69,shown);
    const bool command_read=button && text(static_cast<unsigned char*>(button)+0x388,command);
    const std::string expected=action=="close"?"MessageTo Window Close":action=="next"?"MessageTo Window NextPage":"MessageTo Window CreateGame";
    const bool owner_matches=owner_read && button_owner==window;
    const bool enabled=disabled_read && !disabled;
    const bool visible=shown_read && shown==1;
    const bool command_matches=command_read && command==expected;
    const bool passed=!lookup_code && button && owner_matches && enabled && visible && command_matches;
    Json guards{{"pane",pane},{"lookup_exception",lookup_code},{"button_found",button!=nullptr},
        {"owner_read",owner_read},{"owner_matches_window",owner_matches},
        {"disabled_read",disabled_read},{"disabled",disabled_read?Json(disabled):Json(nullptr)},{"enabled",enabled},
        {"shown_read",shown_read},{"shown",shown_read?Json(shown):Json(nullptr)},{"visible",visible},
        {"command_read",command_read},{"command",command_read?Json(command.substr(0,512)):Json(nullptr)},
        {"command_bytes",command_read?Json(command.size()):Json(nullptr)},{"command_truncated",command_read && command.size()>512},
        {"expected_command",expected},{"command_matches",command_matches}};
    Json result{{"target","wizard:"+target},{"native_event","mmoButtonPane::Activate"},{"native_attempted",false},
        {"native_returned",false},{"guard_passed",passed},{"guards",guards},{"world_creation_confirmed",false},{"result","rejected"}};
    if(!passed) {
        std::string cause="Wizard original button guard rejected:";
        if(lookup_code) cause+=" lookup_exception="+std::to_string(lookup_code);
        if(!button) cause+=" button_missing";
        if(!owner_read) cause+=" owner_unreadable"; else if(!owner_matches) cause+=" owner_mismatch";
        if(!disabled_read) cause+=" disabled_unreadable"; else if(!enabled) cause+=" disabled="+std::to_string(disabled);
        if(!shown_read) cause+=" shown_unreadable"; else if(!visible) cause+=" shown="+std::to_string(shown);
        if(!command_read) cause+=" command_unreadable"; else if(!command_matches) cause+=" command_mismatch";
        result["cause"]=cause; log_error(cause); return result;
    }
    if(args.value("observe",false)) { result["result"]="observed"; return result; }
    const auto code=activate_native(button,false);
    result.update({{"native_attempted",true},{"native_returned",code==0},{"exception_code",code},{"result","unknown"}});
    if(code) { result["cause"]="Wizard native button exception="+std::to_string(code); log_error(result.at("cause").get<std::string>()); return result; }
    if(action=="close") {
        const auto after=new_world({{"mod_id",owner},{"observe",true}});
        const bool closed=after.value("confirmed",false) && !after.value("visible",true);
        result["close_confirmed"]=closed; result["observation"]=after; result["result"]=closed?"confirmed":"unknown";
        if(!closed) { result["cause"]="Original wizard close did not confirm a hidden NewGame window"; log_error(result.at("cause").get<std::string>()); }
    } else result["result"]="accepted";
    return result;
}
