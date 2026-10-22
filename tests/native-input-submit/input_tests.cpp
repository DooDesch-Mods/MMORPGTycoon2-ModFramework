#include "native_input.h"
#include <SDL3/SDL_events.h>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>

using Json=mt2fw::Json;
namespace native_input=mt2fw::native_input;
struct Plugin { bool active=true; };
struct Runtime {
    std::vector<std::unique_ptr<Plugin>> entries;
    auto& plugins(){return entries;}
} fixture_runtime;
Runtime* runtime=&fixture_runtime;
uint32_t thread=7;
uint32_t GetCurrentThreadId(){return 7;}
void* mode=&fixture_runtime;
bool panel_open=false;
struct NativeWindow {
    const Plugin* owner=nullptr;
    void* mode=nullptr;
    void* panel=nullptr;
    bool requested=true,suspended=false;
    Json widgets,clicked=Json::array();
};
std::map<std::string,NativeWindow> native_windows;
native_input::SubmitKeys submit_keys;
std::array<uint8_t,0x600> panel{},field{},button{};
std::vector<std::string> errors;
void* unreadable=nullptr;
void require(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
void log_error(const std::string& cause){errors.push_back(cause);}
bool read(void* object,size_t offset,uint8_t& value){
    if(object==unreadable)return false;
    require(object==panel.data()||object==field.data()||object==button.data(),"unexpected pane");
    require(offset<field.size(),"invalid native offset");value=static_cast<uint8_t*>(object)[offset];return true;
}
void* native_pane(void* receiver,const std::string& id){
    require(receiver==panel.data(),"wrong window");
    if(id=="message"||id=="second")return field.data();
    require(id=="send","wrong target button");return button.data();
}
#include "native_input_dispatch.inc"

int checks=0;
void expect(bool condition,const char* cause){++checks;require(condition,cause);}
NativeWindow& app(){return native_windows.at("test.mod");}
void reset(){
    fixture_runtime.entries.clear();fixture_runtime.entries.push_back(std::make_unique<Plugin>());
    runtime=&fixture_runtime;thread=7;mode=&fixture_runtime;panel_open=false;unreadable=nullptr;
    submit_keys.reset();errors.clear();panel.fill(0);field.fill(0);button.fill(0);
    panel[0x45f]=1;field[0x69]=field[0x2b4]=field[0x3d8]=field[0x584]=1;
    button[0x69]=button[0x2b4]=1;
    std::memcpy(field.data()+0x2d0,"message text",13);
    native_windows.clear();auto& w=native_windows["test.mod"];
    w.owner=fixture_runtime.entries.front().get();w.mode=mode;w.panel=panel.data();
    w.widgets=Json::array({{{"id","message"},{"type","input"},{"submit","send"}},{{"id","send"},{"type","button"},{"submit",""}}});
}
bool key(bool down=true,bool repeat=false,SDL_Scancode code=SDL_SCANCODE_RETURN,uint32_t window=42,bool captured=false){
    SDL_Event event{};event.type=down?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;
    event.key.windowID=window;event.key.scancode=code;event.key.repeat=repeat;
    return input(event,42,captured);
}
template<class Action> void rejects(Action action,const char* cause){
    bool failed=false;try{action();}catch(const std::exception& e){failed=std::string(e.what()).find(cause)!=std::string::npos;}
    expect(failed,"validation must retain the concrete cause");
}
int main(){try{
    reset();const auto original_field=field;const auto original_button=button;
    expect(key(),"focused Enter must be consumed");
    expect(app().clicked==Json::array({"send"}),"Enter must emit the existing button ID");
    expect(key()&&key(true,true)&&app().clicked.size()==1,"held or repeated Enter must send only once");
    expect(key(false)&&!key(false),"only captured key release must be consumed");
    expect(key()&&app().clicked.size()==2,"release allows another press");
    expect(field==original_field&&button==original_button,"dispatch must preserve text, focus and native pane state");
    expect(errors.empty(),"ordinary submit must not log an error");
    reset();expect(key(true,false,SDL_SCANCODE_KP_ENTER),"keypad Enter must submit");
    expect(key()&&app().clicked.size()==2,"independent Enter keys must retain separate latches");
    expect(key(false,false,SDL_SCANCODE_KP_ENTER)&&key(false),"each captured key must release");
    reset();expect(!key(true,true)&&app().clicked.empty(),"an unowned repeat must pass through");
    expect(!key(true,false,SDL_SCANCODE_A),"other keys must pass through");
    expect(!key(true,false,SDL_SCANCODE_RETURN,43),"other game windows must pass through");
    expect(!key(true,false,SDL_SCANCODE_RETURN,42,true),"overlay keyboard capture has priority");
    for(auto offset:{0x584,0x69,0x2b4,0x3d8}){reset();field[size_t(offset)]=0;expect(!key()&&app().clicked.empty(),"unfocused, hidden or noneditable input must pass through");}
    for(auto offset:{0x69,0x2b4}){reset();button[size_t(offset)]=0;expect(key()&&app().clicked.empty(),"hidden submit target must not activate or drop focus");}
    reset();button[0x46d]=1;expect(key()&&app().clicked.empty(),"disabled submit target must not activate");
    button[0x46d]=0;expect(key()&&app().clicked.empty(),"enabling a button while Enter is held must not activate");
    expect(key(false)&&key()&&app().clicked.size()==1,"next physical press can activate the enabled button");
    reset();panel_open=true;expect(!key(),"Mods manager must retain keyboard ownership");
    reset();app().suspended=true;expect(!key(),"suspended apps must not receive submit");
    reset();app().requested=false;expect(!key(),"closed apps must not receive submit");
    reset();panel[0x45f]=0;expect(!key(),"physically closed windows must not receive submit");
    reset();fixture_runtime.entries.front()->active=false;expect(!key(),"inactive mod instances must not receive submit");
    reset();app().mode=nullptr;expect(!key(),"old modes must not receive submit");
    reset();thread=8;expect(!key(),"another thread must not dispatch native input");
    reset();app().widgets[0]["submit"]="";expect(!key(),"inputs without submit must retain native handling");
    reset();expect(key(),"capture before focus loss");SDL_Event lost{};lost.type=SDL_EVENT_WINDOW_FOCUS_LOST;lost.window.windowID=42;
    expect(!input(lost,42,false)&&!key(false),"focus loss must reset held keys without swallowing unrelated releases");
    expect(key()&&app().clicked.size()==2,"focus recovery must allow the next physical press");
    reset();expect(key(),"capture before overlay opens");expect(key(false,false,SDL_SCANCODE_RETURN,42,true),"captured release must be drained even after overlay opens");
    reset();app().widgets.push_back({{"id","second"},{"type","input"},{"submit","send"}});
    expect(key()&&app().clicked.empty()&&errors.size()==1&&errors[0].find("more than one")!=std::string::npos,"ambiguous focus must fail visibly without submitting");
    reset();unreadable=button.data();expect(key()&&app().clicked.empty()&&errors.size()==1,"unreadable submit button must retain the native failure");
    reset();app().clicked=Json::array();for(int i=0;i<16;++i)app().clicked.push_back("send");
    expect(key()&&app().clicked.size()==16&&errors.size()==1&&errors[0]=="Native app event queue is full","queue budget must reject without growing");
    reset();auto definition=Json{{"id","message"},{"type","input"},{"submit","send"}};
    expect(native_input::submit(definition,app().widgets)=="send","submit must resolve a same-window button");
    for(auto value:Json::array({true,7,"","missing","message","send_label","send!"})){definition["submit"]=value;rejects([&]{native_input::submit(definition,app().widgets);},"submit");}
    definition["submit"]="send";definition["type"]="button";rejects([&]{native_input::submit(definition,app().widgets);},"submit");
    definition.erase("submit");expect(native_input::submit(definition,app().widgets).empty(),"existing widgets remain compatible");
    std::cout<<checks<<" native input submit checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
