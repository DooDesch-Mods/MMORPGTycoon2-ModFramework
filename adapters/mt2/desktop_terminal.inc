// Included in desktop's namespace. Only the retained Multiplayer app is served.
namespace {
#include "terminal_panel_pins.inc"
bool terminal_pins_checked=false;
void terminal_verify(){
    for(const auto& pin:terminal_pins){
        if(pin.rva==0x81ec20){
            // Window::Show has an acknowledged Multiplayer HUD hook. Recheck its
            // current patch and detour ownership even after static pins passed.
            std::string cause;
            require(bool(verify_terminal_body),"Terminal Window::Show ownership verifier is unavailable");
            const bool valid=verify_terminal_body(pin.rva,pin.size,pin.sha256,cause);
            require(valid,"Terminal native body differs rva="+std::to_string(pin.rva)+": "+cause);
            continue;
        }
        if(terminal_pins_checked)continue;
        const auto* body=reinterpret_cast<const char*>(image+pin.rva);
        require(readable(body,pin.size)&&digest(std::string(body,pin.size))==pin.sha256,
            "Terminal native body differs rva="+std::to_string(pin.rva));
    }
    terminal_pins_checked=true;
}
terminal_panel::Attempt terminal_draw_receipt;
terminal_panel::Click terminal_click;
uint64_t terminal_lease=0;
void* terminal_panel_owner=nullptr;
NativeWindow& terminal_window(){
    require(runtime&&thread&&thread==GetCurrentThreadId()&&(mode_index==2||mode_index==3),"Terminal native panel requires its bound Title or InGame thread");
    auto found=native_windows.find("doodesch.multiplayer");
    require(found!=native_windows.end(),"Terminal Multiplayer native window is not registered");
    auto& app=found->second;bool active=false;
    for(const auto& plugin:runtime->plugins())if(plugin.get()==app.owner&&plugin->active)active=true;
    require(active&&app.panel&&app.mode==mode&&!app.suspended,"Terminal native app owner is unavailable");
    void* registered=nullptr;check(window_lookup(&registered,app.name.c_str()),"Terminal panel registry lookup");
    require(registered==app.panel,"Terminal native window registry changed");return app;
}
bool terminal_button(const std::string& id){return id=="leave"||id=="chat_send"||id=="report"||id=="report_back"||id=="report_send"||id=="report_auto";}
bool terminal_button_available(NativeWindow& app,const std::string& id){
    auto* target=native_pane(app.panel,id);uintptr_t table=0;uint8_t disabled=1,shown=0,visible=0;
    require(read(target,0,table)&&table==image+0x16e3ec0&&read(target,0x46d,disabled)&&disabled<=1&&
        read(target,0x69,shown)&&shown<=1&&read(target,0x2b4,visible)&&visible<=1,"Terminal panel button owner or state is unreadable");
    return !disabled&&shown&&visible;
}
void terminal_queue(NativeWindow& app,const std::string& id){
    require(terminal_button(id),"Terminal panel rejected a shared-world or admission action");
    bool known=false;for(const auto& item:app.widgets)if(item.at("id")==id&&item.at("type")=="button")known=true;
    require(known,"Terminal panel button is not owned");
    require(terminal_button_available(app,id),"Terminal panel button is disabled or hidden");
    require(app.clicked.size()<16,"Terminal native app event queue is full");app.clicked.push_back(id);
}
DWORD terminal_reveal(void* target){
    __try{
        reinterpret_cast<void(*)(void*,bool,bool)>(image+0x81ec20)(target,true,true);
        reinterpret_cast<void(*)(void*)>(image+0x81e010)(target);
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
void terminal_field(NativeWindow& app,void* field){
    bool owned=false;for(const char* id:{"chat_input","report_comment"})if(native_pane(app.panel,id)==field)owned=true;
    uintptr_t table=0,binding=0;uint8_t visible=0;size_t command_size=0;
    require(owned&&read(field,0,table)&&table==image+0x1703290&&read(field,0x4a0,binding)&&!binding&&
        read(field,0x3e8,command_size)&&!command_size&&read(field,0x69,visible)&&visible==1&&
        read(field,0x2b4,visible)&&visible==1&&read(field,0x3d8,visible)&&visible==1,
        "Terminal input has an external binding, command, hidden or non-editable owner");
}
DWORD terminal_focus_call(void* window,void* field){
    __try{reinterpret_cast<void(*)(void*,void*)>(image+0x81ada0)(window,field);}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
DWORD terminal_key_call(void* input,const SDL_Event* event,void* field){
    __try{
        if(event)reinterpret_cast<void(*)(void*,const SDL_Event*)>(image+0x9e9d90)(input,event);
        reinterpret_cast<void(*)(void*,float)>(image+0x7f2580)(field,0.f);
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
void terminal_events(NativeWindow& app,const terminal_panel::Native& n,const terminal_panel::Owner& owner){
    struct Event {SDL_Event value{};std::string text;};
    struct Batch {uint32_t window;std::vector<Event> events;std::string error;} batch{SDL_GetWindowID(reinterpret_cast<SDL_Window*>(owner.window)),{}, {}};
    require(batch.window,"Terminal SDL window ID is unavailable");
    // Remove only this window's input. Unrelated SDL windows and lifecycle
    // events remain queued. Failed simulation input must not become hotkeys.
    SDL_FilterEvents([](void* context,SDL_Event* event)->bool{
        auto& b=*static_cast<Batch*>(context);uint32_t window=0;
        switch(event->type){
        case SDL_EVENT_KEY_DOWN:case SDL_EVENT_KEY_UP:window=event->key.windowID;break;
        case SDL_EVENT_TEXT_INPUT:window=event->text.windowID;break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:case SDL_EVENT_MOUSE_BUTTON_UP:window=event->button.windowID;break;
        case SDL_EVENT_MOUSE_MOTION:window=event->motion.windowID;break;
        case SDL_EVENT_MOUSE_WHEEL:window=event->wheel.windowID;break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:window=event->window.windowID;break;
        default:return true;
        }
        if(window!=b.window||!b.error.empty())return true;
        try{
            if(b.events.size()>=128){b.error="Terminal input batch exceeds 128 events; remaining input retained";return true;}
            Event copy;copy.value=*event;
            if(event->type==SDL_EVENT_TEXT_INPUT){require(event->text.text,"Terminal text event lacks content");copy.text=event->text.text;require(copy.text.size()<=1024,"Terminal text event exceeds its native input bound");}
            b.events.push_back(std::move(copy));return false;
        }catch(const std::exception& e){b.error=e.what();return true;}
        catch(...){b.error="Terminal input capture raised an unknown exception";return true;}
    },&batch);
    int width=0,height=0;require(SDL_GetWindowSize(reinterpret_cast<SDL_Window*>(owner.window),&width,&height),"Terminal logical window dimensions unavailable");
    auto calls=terminal_panel::input_calls(image);
    for(auto& captured:batch.events){const auto& event=captured.value;
        if(event.type==SDL_EVENT_WINDOW_FOCUS_LOST){terminal_click.cancel();submit_keys.reset();continue;}
        if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN||event.type==SDL_EVENT_MOUSE_BUTTON_UP){
            if(event.button.button!=SDL_BUTTON_LEFT)continue;
            if(event.button.x<0||event.button.y<0||event.button.x>=width||event.button.y>=height){terminal_click.cancel();continue;}
            const auto target=terminal_panel::hit(n,calls,owner,event.button.x,event.button.y,width,height);
            std::string id;for(const auto& item:app.widgets){
                const auto name=item.at("id").get<std::string>();
                if((terminal_button(name)||name=="chat_input"||name=="report_comment")&&reinterpret_cast<uintptr_t>(native_pane(app.panel,name))==target){id=name;break;}
            }
            if(terminal_button(id)&&!terminal_button_available(app,id))id.clear();
            if(event.type==SDL_EVENT_MOUSE_BUTTON_DOWN){
                if(id.empty()){terminal_click.cancel();continue;}
                if(id=="chat_input"||id=="report_comment"){
                    auto* field=reinterpret_cast<void*>(target);terminal_field(app,field);
                    void* input=nullptr;void* handler=nullptr;void* previous=nullptr;
                    require(read(reinterpret_cast<void*>(image),0x13df990,input)&&read(input,0xb0,handler)&&read(app.panel,0x3c0,previous),"Terminal focused input owner unreadable");
                    if(previous){std::set<uintptr_t> tree;terminal_panel::inspect_tree(n,owner,owner.panel,tree);require(tree.contains(reinterpret_cast<uintptr_t>(previous)),"Terminal previous focus is outside its owned pane tree");}
                    if(handler){require(previous&&handler==static_cast<unsigned char*>(previous)+0x2c8,"Terminal cannot replace a foreign native StringMode handler");terminal_field(app,previous);}
                    check(terminal_focus_call(app.panel,field),"Terminal native input focus");
                    require(read(input,0xb0,handler)&&handler==static_cast<unsigned char*>(field)+0x2c8,"Terminal native focus did not acknowledge its field");
                    terminal_click.cancel();
                }else terminal_click.down(terminal_lease,owner.panel,target);
            }else if(terminal_click.up(terminal_lease,owner.panel,target)&&!id.empty())terminal_queue(app,id);
            continue;
        }
        if(event.type!=SDL_EVENT_KEY_DOWN&&event.type!=SDL_EVENT_KEY_UP&&event.type!=SDL_EVENT_TEXT_INPUT)continue;
        if(event.type!=SDL_EVENT_TEXT_INPUT&&input(event,batch.window,false))continue;
        if(event.type==SDL_EVENT_KEY_UP)continue;
        void* field=nullptr;require(read(app.panel,0x3c0,field),"Terminal focused pane unavailable");if(!field)continue;
        bool editable=false;for(const char* id:{"chat_input","report_comment"})if(native_pane(app.panel,id)==field)editable=true;
        if(!editable)continue;
        terminal_field(app,field);void* native_input=nullptr;void* handler=nullptr;uintptr_t table=0;uint8_t active=0,paused=0;
        require(read(reinterpret_cast<void*>(image),0x13df990,native_input)&&read(native_input,0,table)&&table==image+0x1749430&&
            read(native_input,0xb0,handler)&&handler==static_cast<unsigned char*>(field)+0x2c8&&
            read(native_input,0xa6,active)&&active==1&&read(native_input,0xa9,paused)&&paused==0,
            "Terminal text delivery lacks its exact active native StringMode handler");
        if(event.type==SDL_EVENT_TEXT_INPUT){check(string_call(0x9e97d0,native_input,captured.text.c_str()),"Terminal native text event");check(terminal_key_call(native_input,nullptr,field),"Terminal native text refresh");}
        else check(terminal_key_call(native_input,&event,field),"Terminal native editing key");
    }
    require(batch.error.empty(),batch.error);
}
}
Json terminal_status(){
    auto& app=terminal_window();uint8_t shown=0;
    require(read(app.panel,0x45f,shown)&&shown<=1,"Terminal panel visibility unreadable");
    Json buttons=Json::object();
    for(const char* id:{"leave","report","report_back"})buttons[id]=terminal_button_available(app,id);
    return {{"terminal",true},{"lease",terminal_lease},{"mode",mode_index},{"owner","doodesch.multiplayer"},
        {"panel_visible",shown==1},{"buttons",buttons},{"draw_blocked",terminal_draw_receipt.blocked},{"draw_error",terminal_draw_receipt.error}};
}
Json terminal_activate(const Json& args){
    auto& app=terminal_window();const auto target=args.at("target").get<std::string>();
    constexpr std::string_view prefix="app:doodesch.multiplayer:";
    require(target.starts_with(prefix),"Terminal activation requires the retained Multiplayer panel");
    const auto id=target.substr(prefix.size());
    if(args.contains("text")){
        require(id=="chat_input"||id=="report_comment","Terminal text target is not a local field");
        size_t maximum=0;for(const auto& item:app.widgets)if(item.at("id")==id&&item.at("type")=="input")maximum=native_input::limit(item);
        require(maximum,"Terminal input is not registered");auto* field=native_pane(app.panel,id);
        uintptr_t table=0,binding=0;uint8_t visible=0;size_t command_size=0;
        require(read(field,0,table)&&table==image+0x1703290&&read(field,0x4a0,binding)&&!binding&&
            read(field,0x3e8,command_size)&&!command_size&&read(field,0x69,visible)&&visible==1&&
            read(field,0x2b4,visible)&&visible==1,"Terminal text field has an external binding, command or hidden owner");
        auto value=args.at("text").get<std::string>();native_input::validate(value,maximum);
        check(string_call(0x7f1630,field,value.c_str()),"Terminal local input text");
        std::string actual;require(text(static_cast<unsigned char*>(field)+0x2d0,actual)&&actual==value,"Terminal local input readback differs");
        return {{"accepted",true},{"readback_matches",true},{"bytes",actual.size()}};
    }
    terminal_queue(app,id);return {{"accepted",true},{"widget",id},{"queued",true},{"native_button_invoked",false}};
}
bool terminal_draw(uint64_t lease,uintptr_t native_window,int width,int height,const std::function<void()>& repaint,std::string& error){
    try{
        require(lease,"Terminal panel lacks retained Step lease");auto& app=terminal_window();
        if(terminal_lease!=lease){terminal_lease=lease;terminal_panel_owner=app.panel;terminal_draw_receipt={};terminal_click.cancel();}
        require(terminal_panel_owner==app.panel,"Terminal panel changed within its retained lease");
        if(terminal_draw_receipt.blocked){error=terminal_draw_receipt.error;return false;}
        uintptr_t resolved=0;uint32_t bound=0;std::string hash,cause;
        require(native::read_resolver_receipt(resolved,bound,hash,cause),cause);
        require(resolved==image&&bound==thread&&hash=="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6","Terminal executable/thread resolver differs");
        terminal_verify();
        auto n=terminal_panel::native_calls(image,[](uintptr_t at,void* out,size_t size){
            require(readable(reinterpret_cast<void*>(at),size),"Terminal native owner read is unavailable");std::memcpy(out,reinterpret_cast<void*>(at),size);
        });
        terminal_panel::Owner o;o.image=image;o.mode=reinterpret_cast<uintptr_t>(mode);o.panel=o.registered_panel=reinterpret_cast<uintptr_t>(app.panel);
        o.thread=thread;o.current_thread=GetCurrentThreadId();o.window=native_window;o.width=width;o.height=height;o.mode_index=mode_index;
        require(read(reinterpret_cast<void*>(image),0x13df8f0,o.game)&&read(reinterpret_cast<void*>(image),0x1c4e3a0,o.screen),"Terminal Game/Screen owner read failed");
        terminal_panel::inspect(n,o,false);
        uint8_t suppress=1;uintptr_t child=0;
        require(read(app.panel,0x378,suppress)&&!suppress&&read(app.panel,0x3e0,child)&&!child,"Terminal window has modal suppression or a rez child");
        // Complete the native panel's own reveal without Window::Update, which
        // can synthesize hotkeys, play transition sounds and update entities.
        uint8_t transitioning=0,visible=0;require(read(app.panel,0x45c,transitioning)&&read(app.panel,0x45f,visible),"Terminal reveal flags unavailable");
        if(transitioning||!visible){
            const auto code=terminal_reveal(app.panel);
            if(code){terminal_draw_receipt.blocked=true;terminal_draw_receipt.error="Terminal native panel reveal exception="+std::to_string(code)+"; no replay";throw std::runtime_error(terminal_draw_receipt.error);}
        }
        terminal_events(app,n,o);
        const bool ok=terminal_panel::draw(n,o,repaint,terminal_draw_receipt);error=terminal_draw_receipt.error;return ok;
    }catch(const std::exception& e){error=e.what();return false;}
}
