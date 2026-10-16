#include "bootstrap.h"
#include "adapter.h"
#include "crypto.h"
#include <MinHook.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <gl/GL.h>
#include <chrono>
#include <fstream>
#include <mutex>
#include <deque>
#include <thread>
#include <condition_variable>
#include <iomanip>
#include <sstream>

namespace {
using namespace mt2fw;
std::unique_ptr<Runtime> runtime;
std::recursive_mutex gate;
fs::path root;
using Update=void(*)(void*,float);
using Swap=bool(*)(SDL_Window*);
using Poll=bool(*)(SDL_Event*);
using Hide=bool(*)();
using Relative=bool(*)(SDL_Window*,bool);
Update original_update=nullptr;Swap original_swap=nullptr;Poll original_poll=nullptr;
Hide original_hide=nullptr;Relative original_relative=nullptr;
bool show_menu=false,ui_ready=false,running=false,developer=false,german=false,restore_relative=false,menu_input_active=false;
SDL_Window* window=nullptr;
using BindFramebuffer=void(APIENTRY*)(GLenum,GLuint);
BindFramebuffer bind_framebuffer=nullptr;
// MT2 can leave an offscreen target bound when it presents the window.
struct WindowFramebuffer {
    GLint draw=0,read=0;
    WindowFramebuffer() {
        if(!bind_framebuffer)throw std::runtime_error("Framework framebuffer entrypoint unavailable");
        glGetIntegerv(0x8CA6,&draw);glGetIntegerv(0x8CAA,&read);
        bind_framebuffer(0x8CA9,0);bind_framebuffer(0x8CA8,0);
    }
    ~WindowFramebuffer(){bind_framebuffer(0x8CA9,GLuint(draw));bind_framebuffer(0x8CA8,GLuint(read));}
};
DWORD engine_thread=0;
std::string process_started;
uint64_t frames=0;
std::string pending_capture,last_request,last_error,log_filter;
char filter[128]{};
char console_input[2048]="{\"mod\":\"example.hello\",\"operation\":\"world.status\",\"arguments\":{}}";
std::deque<Json> console_requests;
std::deque<double> timings;
std::deque<double> original_timings,total_timings;
std::string benchmark_mode="reference";
thread_local double auxiliary_ms=0;
struct HookCost{std::chrono::steady_clock::time_point begin=std::chrono::steady_clock::now();~HookCost(){auxiliary_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();}};
using OpenFile=void*(*)(const char*);
using CloseFile=int(*)(void*);
using MoveFile=BOOL(WINAPI*)(LPCWSTR,LPCWSTR,DWORD);
MoveFile original_move=nullptr;
using TitleCommand=void(*)(void*,const void*);
TitleCommand original_title_command=nullptr;
using SaveCommand=void(*)(void*,const void*,bool);
SaveCommand original_save_command=nullptr;
OpenFile original_open_read=nullptr,original_open_write=nullptr;
CloseFile original_close=nullptr;
struct SaveJob {fs::path path;Json metadata;};
struct SaveCompletion {Json metadata;std::string hash,error;};
std::mutex save_gate;
std::condition_variable save_wake;
std::map<void*,SaveJob> save_files;
std::map<fs::path,SaveJob> closed_temporary_saves;
std::deque<SaveJob> save_jobs;
std::deque<SaveCompletion> saved_files;
std::jthread save_worker;
uint64_t confirmed_saves=0;
std::string last_save;
auto previous_status=std::chrono::steady_clock::time_point{};
auto previous_diagnostic=std::chrono::steady_clock::time_point{};
const char* tr(const char* de,const char* en){return german?de:en;}
void menu_state(bool visible);
void report(const std::string& error) {
    if(last_error!=error){last_error=error;if(runtime)runtime->log("framework","error",error);else OutputDebugStringA(error.c_str());}
}
bool save_name(const char* name) {
    if(!name)return false;std::string path(name);
    return path.ends_with(".vrz")&&!path.ends_with(".meta.vrz")&&path.find("save/")!=std::string::npos;
}
bool temporary_save_name(const char* name) {
    if(!name)return false;std::string value(name);auto marker=value.rfind(".vrz.");
    return marker!=std::string::npos&&save_name(value.substr(0,marker+4).c_str())&&value.find("tmp/")!=std::string::npos;
}
fs::path physical_save(const char* name,bool writing) {
    auto module=GetModuleHandleW(L"libphysfs.dll");
    auto directory=reinterpret_cast<const char*(*)()>(GetProcAddress(module,"PHYSFS_getWriteDir"));
    auto real=reinterpret_cast<const char*(*)(const char*)>(GetProcAddress(module,"PHYSFS_getRealDir"));
    auto point=reinterpret_cast<const char*(*)(const char*)>(GetProcAddress(module,"PHYSFS_getMountPoint"));
    const char* base=writing?(directory?directory():nullptr):(real?real(name):nullptr);
    if(!base)return {};
    std::string relative(name);while(relative.starts_with('/'))relative.erase(0,1);
    if(!writing&&point){auto mount=point(base);if(mount&&relative.starts_with(mount))relative.erase(0,std::strlen(mount));}
    return confined(fs::u8path(base),fs::u8path(relative));
}
void* open_read(const char* name) {
    return original_open_read(name);
}
void title_command(void* title,const void* command) {
    try {
        struct NativeString{const char* data;size_t size;};auto& text=*static_cast<const NativeString*>(command);
        if(text.data&&text.size<=4096){std::string value(text.data,text.size);
            if(value.starts_with("Load ")){
                std::istringstream input(value);std::string verb,stem,folder;input>>verb>>std::quoted(stem)>>std::quoted(folder);
                if(!input||stem.find_first_of("/\\:")!=std::string::npos||folder.find_first_of("/\\:")!=std::string::npos)throw std::runtime_error("Invalid native save selection");
                auto name="user/save/"+folder+"/"+stem+".vrz";auto path=physical_save(name.c_str(),false);
                if(path.empty()||!fs::is_regular_file(path))throw std::runtime_error("Selected save cannot be resolved in the isolated profile: "+name);
                auto hash=file_digest(path);std::lock_guard lock(gate);runtime->check_save(hash);
            }
        }
    }catch(const std::exception& error){std::lock_guard lock(gate);report(std::string("Save load blocked: ")+error.what());menu_state(true);return;}
    original_title_command(title,command);
}
void save_command(void* mode,const void* filename,bool asynchronous) {
    try{std::lock_guard lock(gate);mt2::refresh_saved_bindings();}
    catch(const std::exception& error){std::lock_guard lock(gate);report(std::string("Save blocked: persistent object mapping failed: ")+error.what());menu_state(true);return;}
    original_save_command(mode,filename,asynchronous);
}
void* open_write(const char* name) {
    auto file=original_open_write(name);if(!file||(!save_name(name)&&!temporary_save_name(name)))return file;
    try {
        auto path=physical_save(name,true);if(path.empty())throw std::runtime_error("PHYSFS write directory unavailable");
        Json metadata;{std::lock_guard lock(gate);if(!runtime)throw std::runtime_error("Framework save metadata runtime unavailable");metadata=runtime->capture_save(path_text(path.parent_path().filename()));}
        std::lock_guard lock(save_gate);save_files[file]={path,metadata};
    }catch(const std::exception& error){std::lock_guard lock(gate);report(std::string("Save metadata capture failed: ")+error.what());}
    return file;
}
int close_file(void* file) {
    int result=original_close(file);std::lock_guard lock(save_gate);auto found=save_files.find(file);
    if(found!=save_files.end()) {
        if(result){
            if(temporary_save_name(path_text(found->second.path).c_str()))closed_temporary_saves[found->second.path.lexically_normal()]=std::move(found->second);
            else {save_jobs.push_back(std::move(found->second));save_wake.notify_one();}
        }
        else saved_files.push_back({{}, {}, "Original PHYSFS_close failed; save is not confirmed"});
        save_files.erase(found);
    }
    return result;
}
BOOL WINAPI move_file(LPCWSTR source,LPCWSTR destination,DWORD flags) {
    BOOL result=original_move(source,destination,flags);DWORD last=GetLastError();
    try {
        if(result&&source&&destination){std::lock_guard lock(save_gate);auto found=closed_temporary_saves.find(fs::path(source).lexically_normal());
            if(found!=closed_temporary_saves.end()){
                auto target=fs::path(destination).lexically_normal();auto relative=target.lexically_relative(root/"game-data");
                if(relative.empty()||relative.is_absolute()||*relative.begin()==".."||!save_name(path_text(target).c_str()))throw std::runtime_error("Confirmed save rename destination is outside private save storage");
                auto job=std::move(found->second);closed_temporary_saves.erase(found);job.path=target;save_jobs.push_back(std::move(job));save_wake.notify_one();
            }
        }
    }catch(const std::exception& error){OutputDebugStringA((std::string("error: save rename confirmation failed: ")+error.what()).c_str());}
    SetLastError(last);return result;
}
void start_save_worker() {
    save_worker=std::jthread([](std::stop_token stop){
        for(;;) {
            SaveJob job;
            {std::unique_lock lock(save_gate);save_wake.wait(lock,[&]{return stop.stop_requested()||!save_jobs.empty();});if(save_jobs.empty()&&stop.stop_requested())break;job=std::move(save_jobs.front());save_jobs.pop_front();}
            SaveCompletion result{job.metadata,{},{}};
            try {
                auto before=fs::last_write_time(job.path);auto size=fs::file_size(job.path);result.hash=file_digest(job.path);
                if(fs::last_write_time(job.path)!=before||fs::file_size(job.path)!=size)throw std::runtime_error("Save changed during confirmation hash; no metadata committed");
                store_save_metadata(root,job.metadata,result.hash);
            }catch(const std::exception& error){result.error=error.what();OutputDebugStringA((std::string("error: save confirmation failed: ")+result.error).c_str());}
            std::lock_guard lock(save_gate);saved_files.push_back(std::move(result));
        }
    });
}
void drain_saves() {
    std::deque<SaveCompletion> completed;{std::lock_guard lock(save_gate);completed.swap(saved_files);}
    for(auto& result:completed) {
        if(!result.error.empty()){report("Save confirmation failed: "+result.error);continue;}
        runtime->save_acknowledged(result.metadata,result.hash);++confirmed_saves;last_save=result.hash;
    }
}
Json status() {
    auto value=runtime->status();value["pid"]=GetCurrentProcessId();value["frames"]=frames;value["running"]=running;value["ui_ready"]=ui_ready;value["menu_open"]=show_menu;
    value["error"]=last_error;value["process_start_time"]=process_started;value["confirmed_saves"]=confirmed_saves;value["last_save"]=last_save;
    value["menu_input_active"]=menu_input_active;
    if(window){value["relative_mouse"]=SDL_GetWindowRelativeMouseMode(window);value["text_input_active"]=SDL_TextInputActive(window);value["cursor_visible"]=SDL_CursorVisible();}
    if(!timings.empty()){std::vector<double> sorted(timings.begin(),timings.end());std::sort(sorted.begin(),sorted.end());value["core_ms_p95"]=sorted[size_t((sorted.size()-1)*0.95)];value["core_ms_samples"]=sorted.size();}
    value["measurement_mode"]=benchmark_mode;
    for(auto pair:{std::pair{"original_update_ms_p95",&original_timings},std::pair{"total_update_ms_p95",&total_timings}})if(!pair.second->empty()){std::vector<double> sorted(pair.second->begin(),pair.second->end());std::sort(sorted.begin(),sorted.end());value[pair.first]=sorted[size_t((sorted.size()-1)*0.95)];}
    Json logs=Json::array();for(auto& item:runtime->logs())logs.push_back({{"mod",item.mod},{"level",item.level},{"message",item.message}});value["logs"]=logs;return value;
}
void menu_input(bool visible) {
    if(!window||menu_input_active==visible)return;
    menu_input_active=visible;
    if(visible){restore_relative=SDL_GetWindowRelativeMouseMode(window);if(!original_relative(window,false))report(std::string("Disable relative mouse failed: ")+SDL_GetError());if(!SDL_ShowCursor())report(std::string("Show cursor failed: ")+SDL_GetError());if(!SDL_StartTextInput(window))report(std::string("Start text input failed: ")+SDL_GetError());}
    else{if(!SDL_StopTextInput(window))report(std::string("Stop text input failed: ")+SDL_GetError());if(restore_relative&&!original_relative(window,true))report(std::string("Restore relative mouse failed: ")+SDL_GetError());}
}
void menu_state(bool visible) {
    show_menu=visible;menu_input(visible);
}
Json ui_request(const std::string& op,const Json& args) {
    if(!ui_ready||!ImGui::GetCurrentContext())throw std::runtime_error("UI context unavailable");
    auto id=args.at("mod_id").get<std::string>();
    if(op=="ui.begin") {
        auto title=args.at("title").get<std::string>();if(title.size()>256)throw std::runtime_error("Window title exceeds 256 bytes");
        size_t index=0;for(auto& plugin:runtime->plugins()){if(plugin->manifest.id==id)break;if(plugin->manifest.capabilities.contains("ui"))++index;}
        auto display=ImGui::GetIO().DisplaySize;int columns=(std::max)(1,int((display.x-40)/400));
        ImGui::SetNextWindowPos(ImVec2(24+float(index%columns)*400,64+float(index/columns)*210),ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(376,180),ImGuiCond_FirstUseEver);
        return ImGui::Begin((title+"###"+id+":"+title).c_str());
    }
    if(op=="ui.end"){ImGui::End();return true;}
    ImGui::PushID(id.c_str());Json result;
    if(op=="ui.text"){auto value=args.at("text").get<std::string>();if(value.size()>8192)throw std::runtime_error("UI text exceeds 8192 bytes per widget");ImGui::TextUnformatted(value.c_str());result=true;}
    else if(op=="ui.button")result=ImGui::Button(args.at("label").get<std::string>().c_str());
    else if(op=="ui.checkbox"){bool value=args.at("value");ImGui::Checkbox(args.at("label").get<std::string>().c_str(),&value);result=value;}
    else if(op=="ui.number"){float value=args.at("value");ImGui::SliderFloat(args.at("label").get<std::string>().c_str(),&value,args.at("min"),args.at("max"));result=value;}
    else {ImGui::PopID();throw std::runtime_error("Unknown UI operation: "+op);}
    ImGui::PopID();return result;
}
void render_menu() {
    if(!show_menu)return;
    ImGui::SetNextWindowSize(ImVec2(850,590),ImGuiCond_FirstUseEver);
    bool opened=show_menu;
    if(ImGui::Begin("MT2 Mod Framework - Mods",&opened)) {
        ImGui::TextUnformatted(tr("Mods und Einstellungen","Mods and settings"));ImGui::SameLine();
        if(ImGui::Button(german?"English":"Deutsch"))german=!german;
        ImGui::SameLine();ImGui::Checkbox(tr("Entwicklerfenster","Developer tools"),&developer);
        ImGui::Separator();
        if(!last_error.empty()){ImGui::TextWrapped("%s",last_error.c_str());if(ImGui::SmallButton(tr("Meldung schließen","Dismiss message")))last_error.clear();ImGui::Separator();}
        for(auto& p:runtime->plugins()) {
            ImGui::PushID(p->manifest.id.c_str());
            bool active=runtime->enabled(p->manifest.id);
            if(ImGui::Checkbox(p->manifest.name.c_str(),&active))runtime->set_enabled(p->manifest.id,active);
            ImGui::SameLine();ImGui::TextDisabled("%s | %s",p->manifest.version.c_str(),p->manifest.type.c_str());
            if(p->manifest.type=="lua"&&!p->manifest.capabilities.contains("content")){ImGui::SameLine();if(ImGui::SmallButton(tr("Neu laden","Reload")))runtime->request_reload(p->manifest.id);}
            else {ImGui::SameLine();ImGui::TextDisabled("%s",tr("Neustart erforderlich","Restart required"));}
            if(!p->error.empty())ImGui::TextWrapped("%s",p->error.c_str());
            if(!p->manifest.dependencies.empty()&&ImGui::TreeNode(tr("Abhängigkeiten","Dependencies"))){for(auto& [name,range]:p->manifest.dependencies)ImGui::Text("%s %s",name.c_str(),range.c_str());ImGui::TreePop();}
            for(auto it=p->config_schema.begin();it!=p->config_schema.end();++it) {
                const auto key=it.key();auto schema=it.value();auto label=schema.value("label",key);bool changed=false;Json value=p->config.at(key);
                if(schema["type"]=="boolean"){bool v=value;changed=ImGui::Checkbox(label.c_str(),&v);value=v;}
                else if(schema["type"]=="integer"){int v=value;changed=ImGui::SliderInt(label.c_str(),&v,schema.value("min",0),schema.value("max",100));value=v;}
                else if(schema["type"]=="number"){float v=value;changed=ImGui::SliderFloat(label.c_str(),&v,schema.value("min",0.f),schema.value("max",100.f));value=v;}
                else if(schema["type"]=="string"){std::array<char,1024> buffer{};auto s=value.get<std::string>();std::memcpy(buffer.data(),s.c_str(),std::min(s.size(),buffer.size()-1));changed=ImGui::InputText(label.c_str(),buffer.data(),buffer.size());value=buffer.data();}
                if(changed)runtime->request(*p,"config.set",{{"key",key},{"value",value}});
            }
            if(developer)ImGui::Text("%s: %.3f ms | %llu %s",tr("Letzter Aufruf","Last call"),p->last_ms,p->calls,tr("Aufrufe","calls"));
            ImGui::Separator();ImGui::PopID();
        }
        if(runtime->plugins().empty())ImGui::TextWrapped("%s",tr("Noch keine Mods. Erstelle eine Mod im MT2FW-Helfer oder installiere ein Mod-ZIP.","No mods installed. Create a mod in the MT2FW helper or install a mod ZIP."));
        if(developer) {
            if(ImGui::CollapsingHeader(tr("API-Konsole","API console"))) {
                ImGui::TextWrapped("%s",tr("JSON-Anfrage an eine aktive Mod. Fähigkeiten und Spielthread-Prüfungen gelten auch hier.","JSON request for an active mod. Capability and game-thread checks also apply here."));
                ImGui::InputTextMultiline("##console",console_input,sizeof(console_input),ImVec2(-1,70));
                if(ImGui::Button(tr("Ausführen","Execute"))){try{if(console_requests.size()>=8)throw std::runtime_error("Console queue is full");console_requests.push_back(Json::parse(console_input));}catch(const std::exception& error){runtime->log("console","error",error.what());}}
            }
            if(ImGui::CollapsingHeader(tr("Objektinspektor","Object inspector"))) {
                static Json inspected=Json::array();
                if(mt2::in_world()) {
                    try {if(ImGui::Button(tr("Objekte lesen","Read objects")))inspected=mt2::request("objects.list",{{"limit",32}});for(auto& row:inspected)ImGui::TextUnformatted(row.dump().c_str());}
                    catch(const std::exception& error){ImGui::TextWrapped("%s",error.what());}
                }else ImGui::TextUnformatted(tr("Keine Welt geladen","No world loaded"));
            }
            ImGui::InputText(tr("Log-Filter","Log filter"),filter,sizeof(filter));
            ImGui::BeginChild("logs",ImVec2(0,160),true);
            for(auto& line:runtime->logs())if(!filter[0]||line.mod.find(filter)!=std::string::npos||line.message.find(filter)!=std::string::npos)ImGui::TextWrapped("[%s] %s: %s",line.mod.c_str(),line.level.c_str(),line.message.c_str());
            ImGui::EndChild();
        }
    }
    ImGui::End();if(!opened)menu_state(false);
}
void capture() {
    if(pending_capture.empty())return;
    int width=0,height=0;SDL_GetWindowSizeInPixels(window,&width,&height);
    if(width<=0||height<=0||uint64_t(width)*height>16000000)throw std::runtime_error("Screenshot dimensions invalid");
    std::vector<unsigned char> pixels(size_t(width)*height*4);GLint pack=0;glGetIntegerv(GL_PACK_ALIGNMENT,&pack);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());glPixelStorei(GL_PACK_ALIGNMENT,pack);
    if(auto error=glGetError();error!=GL_NO_ERROR)throw std::runtime_error("Screenshot glReadPixels failed: "+std::to_string(error));
    for(size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);
    BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+DWORD(pixels.size());
    BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=width;info.biHeight=height;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
    auto path=confined(root/"diagnostics",pending_capture+".bmp");std::ofstream stream(path,std::ios::binary);stream.write(reinterpret_cast<char*>(&file),sizeof(file));stream.write(reinterpret_cast<char*>(&info),sizeof(info));stream.write(reinterpret_cast<char*>(pixels.data()),pixels.size());stream.close();if(!stream)throw std::runtime_error("Screenshot file write failed");
    atomic_json(root/"diagnostics"/"capture.json",{{"id",pending_capture},{"path",path_text(path)},{"width",width},{"height",height},{"sha256",file_digest(path)}});pending_capture.clear();
}
void diagnostics() {
    auto now=std::chrono::steady_clock::now();if(now-previous_diagnostic<std::chrono::milliseconds(100))return;previous_diagnostic=now;
    auto path=root/"diagnostics"/"request.json";if(!fs::exists(path))return;
    auto request=read_json(path);auto id=request.at("id").get<std::string>();if(id==last_request)return;
    last_request=id;
    if(request.value("pid",uint32_t(0))!=GetCurrentProcessId()||request.value("process_start_time",std::string())!=process_started) {
        runtime->log("diagnostics","info","Skipped retained request from another process: "+id);return;
    }
    Json response={{"id",id},{"pid",GetCurrentProcessId()},{"process_start_time",process_started}};
    try {
        if(id.empty()||id.size()>64||id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")!=std::string::npos)throw std::runtime_error("Invalid diagnostic request ID");
        auto op=request.at("operation").get<std::string>();auto args=request.value("arguments",Json::object());Json result;
        if(op=="status")result=status();
        else if(op=="benchmark_reset"){timings.clear();original_timings.clear();total_timings.clear();result={{"reset",true},{"mode",benchmark_mode}};}
        else if(op=="reload"){runtime->request_reload(args.at("id"));result={{"accepted",true}};}
        else if(op=="menu"){menu_state(args.value("visible",true));developer=args.value("developer",true);result={{"visible",show_menu}};}
        else if(op=="capture"){if(!pending_capture.empty())throw std::runtime_error("Screenshot already pending");pending_capture=id;result={{"accepted",true},{"id",id}};}
        else if(op=="key") {
            auto code=args.at("scancode").get<int>();if(code<=0||code>=SDL_SCANCODE_COUNT)throw std::runtime_error("Invalid diagnostic scancode");
            SDL_Event down{};down.type=SDL_EVENT_KEY_DOWN;down.key.scancode=SDL_Scancode(code);down.key.key=SDL_GetKeyFromScancode(down.key.scancode,SDL_KMOD_NONE,false);down.key.down=true;
            SDL_Event up=down;up.type=SDL_EVENT_KEY_UP;up.key.down=false;
            if(!SDL_PushEvent(&down)||!SDL_PushEvent(&up))throw std::runtime_error(std::string("SDL key event enqueue failed: ")+SDL_GetError());result={{"accepted",true}};
        }
        else if(op=="click") {
            float x=args.at("x"),y=args.at("y");int width=0,height=0;if(!window||!SDL_GetWindowSize(window,&width,&height)||x<0||y<0||x>=width||y>=height)throw std::runtime_error("Diagnostic click is outside the owned game client");
            SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.x=x;motion.motion.y=y;
            SDL_Event down{};down.type=SDL_EVENT_MOUSE_BUTTON_DOWN;down.button.button=SDL_BUTTON_LEFT;down.button.down=true;down.button.x=x;down.button.y=y;
            SDL_Event up=down;up.type=SDL_EVENT_MOUSE_BUTTON_UP;up.button.down=false;
            if(!SDL_PushEvent(&motion)||!SDL_PushEvent(&down)||!SDL_PushEvent(&up))throw std::runtime_error(std::string("SDL click enqueue failed: ")+SDL_GetError());result={{"accepted",true}};
        }
        else if(op=="plugin_request") {
            auto plugin_id=args.at("id").get<std::string>();
            auto found=std::find_if(runtime->plugins().begin(),runtime->plugins().end(),[&](auto& p){return p->manifest.id==plugin_id&&p->active;});
            if(found==runtime->plugins().end())throw std::runtime_error("Active diagnostic plugin not found");
            result=runtime->request(**found,args.at("operation"),args.value("arguments",Json::object()));
        }else result=mt2::diagnostic(op,args);
        response["ok"]=true;response["result"]=result;
    }catch(const std::exception& error){response["ok"]=false;response["error"]=error.what();runtime->log("diagnostics","error",error.what());}
    atomic_json(root/"diagnostics"/"response.json",response);
}
void update(void* game,float dt) {
    auto native_begin=std::chrono::steady_clock::now();double auxiliary=auxiliary_ms;auxiliary_ms=0;
    original_update(game,dt);double original_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-native_begin).count()-auxiliary_ms;std::lock_guard lock(gate);if(!running)return;
    auto begin=std::chrono::steady_clock::now();
    try {
        if(!engine_thread)engine_thread=GetCurrentThreadId();
        if(engine_thread!=GetCurrentThreadId())throw std::runtime_error("Game update changed thread identity");
        if(benchmark_mode=="baseline")mt2::measurement_frame();else {mt2::frame(*runtime,dt);drain_saves();runtime->end_frame();}++frames;
        auto commands=std::move(console_requests);console_requests.clear();
        for(auto& command:commands){try{
            auto id=command.at("mod").get<std::string>();auto found=std::find_if(runtime->plugins().begin(),runtime->plugins().end(),[&](auto& p){return p->active&&p->manifest.id==id;});
            if(found==runtime->plugins().end())throw std::runtime_error("Console mod is not active: "+id);
            auto result=runtime->request(**found,command.at("operation"),command.value("arguments",Json::object()));runtime->log(id,"info","Console result: "+result.dump());
        }catch(const std::exception& error){runtime->log("console","error",error.what());}}
        diagnostics();
        auto finish=std::chrono::steady_clock::now();if(finish-previous_status>std::chrono::seconds(1)){previous_status=finish;atomic_json(root/"status.json",status());}
        double core_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()+auxiliary+auxiliary_ms;auxiliary_ms=0;
        timings.push_back(core_ms);original_timings.push_back(original_ms);total_timings.push_back(original_ms+core_ms);
        if(timings.size()>2048){timings.pop_front();original_timings.pop_front();total_timings.pop_front();}
    }catch(const std::exception& error){report(std::string("Game update failed: ")+error.what());}
}
bool swap(SDL_Window* target) {
    auto begin=std::chrono::steady_clock::now();
    std::lock_guard lock(gate);
    if(runtime&&running&&!show_menu&&runtime->plugins().empty()&&pending_capture.empty())return original_swap(target);
    try {
        if(runtime&&running) {
            if(!ui_ready) {
                window=target;ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui::StyleColorsDark();
                if(!ImGui_ImplSDL3_InitForOpenGL(window,SDL_GL_GetCurrentContext())||!ImGui_ImplOpenGL3_Init("#version 150"))throw std::runtime_error("Framework UI backend initialization failed");
                bind_framebuffer=reinterpret_cast<BindFramebuffer>(SDL_GL_GetProcAddress("glBindFramebuffer"));
                if(!bind_framebuffer)throw std::runtime_error("Framework framebuffer entrypoint unavailable");
                ui_ready=true;
                menu_input(show_menu);
            }
            WindowFramebuffer framebuffer;
            ImGui_ImplOpenGL3_NewFrame();ImGui_ImplSDL3_NewFrame();ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(16,16),ImGuiCond_Once);ImGui::SetNextWindowBgAlpha(0.88f);
            ImGui::Begin("##mt2fw-entry",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings);
            if(ImGui::Button("Mods (F10)"))menu_state(!show_menu);ImGui::End();
            render_menu();runtime->rendering=true;runtime->emit("render");runtime->rendering=false;ImGui::Render();ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());capture();
        }
    }catch(const std::exception& error){if(runtime)runtime->rendering=false;report(std::string("Framework render failed: ")+error.what());}
    auxiliary_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();return original_swap(target);
}
bool poll(SDL_Event* event) {
    while(original_poll(event)) {
        HookCost cost;
        if(!event)return true;
        std::lock_guard lock(gate);
        if(!runtime||!running)return true;
        if(event->type==SDL_EVENT_KEY_DOWN&&event->key.scancode==SDL_SCANCODE_F10&&!event->key.repeat){menu_state(!show_menu);continue;}
        if(event->type==SDL_EVENT_KEY_UP&&event->key.scancode==SDL_SCANCODE_F10)continue;
        bool overlay_visible=show_menu||!runtime->plugins().empty()||!pending_capture.empty();
        if(ui_ready&&overlay_visible)ImGui_ImplSDL3_ProcessEvent(event);
        if(event->type==SDL_EVENT_KEY_DOWN&&!event->key.repeat&&!show_menu) {
            std::vector<Json> owned;
            for(auto& p:runtime->plugins())if(p->active)for(auto& [name,scancode]:p->hotkeys)if(scancode==event->key.scancode) {
                owned.push_back({{"name",name},{"owner",p->manifest.id},{"scancode",scancode}});
            }
            // A failing callback may clear its owner's bindings, so dispatch after taking the snapshot.
            for(auto& value:owned)runtime->emit("input",value);
        }
        if(ui_ready&&overlay_visible) {
            const auto& io=ImGui::GetIO();bool mouse=event->type>=SDL_EVENT_MOUSE_MOTION&&event->type<=SDL_EVENT_MOUSE_WHEEL;
            bool keyboard=event->type==SDL_EVENT_KEY_DOWN||event->type==SDL_EVENT_KEY_UP||event->type==SDL_EVENT_TEXT_INPUT||event->type==SDL_EVENT_TEXT_EDITING;
            if((mouse&&io.WantCaptureMouse)||(keyboard&&show_menu&&io.WantCaptureKeyboard))continue;
        }
        return true;
    }
    return false;
}
bool hide_cursor(){if(show_menu)return SDL_ShowCursor();return original_hide();}
bool relative_mouse(SDL_Window* target,bool active){if(show_menu){restore_relative=active;return original_relative(target,false);}return original_relative(target,active);}
void hook(void* target,void* replacement,void** original) {
    if(!target)throw std::runtime_error("Native hook target absent");auto result=MH_CreateHook(target,replacement,original);if(result!=MH_OK)throw std::runtime_error(std::string("Create hook failed: ")+MH_StatusToString(result));
}
}
extern "C" __declspec(dllexport) bool MT2FW_InitInstalled(const MT2FW_Bootstrap* receipt) {
    try {
        if(runtime||!receipt||receipt->size!=sizeof(*receipt)||receipt->version!=1||receipt->isolation_flags!=3||receipt->process_id!=GetCurrentProcessId()||!receipt->profile||!receipt->game)throw std::runtime_error("Framework bootstrap receipt invalid");
        root=receipt->profile;fs::create_directories(root/"diagnostics");mt2::initialize(receipt->game,root);
        FILETIME created{},exited{},kernel{},user{};if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))throw std::runtime_error("Process creation time unavailable");
        process_started=std::to_string((uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime);
        runtime=std::make_unique<Runtime>(root,[](const std::string& op,const Json& args){if(op.starts_with("ui."))return ui_request(op,args);return mt2::request(op,args);});
        if(fs::exists(root/"benchmark.json"))benchmark_mode=read_json(root/"benchmark.json").at("mode");
        if(benchmark_mode!="reference"&&benchmark_mode!="empty"&&benchmark_mode!="baseline")throw std::runtime_error("Unknown measurement mode");
        if(benchmark_mode=="reference")runtime->load();auto result=MH_Initialize();if(result!=MH_OK)throw std::runtime_error(std::string("MinHook initialization failed: ")+MH_StatusToString(result));
        auto sdl=GetModuleHandleW(L"SDL3.dll");
        hook(mt2::game_update(),reinterpret_cast<void*>(update),reinterpret_cast<void**>(&original_update));
        if(benchmark_mode=="baseline"){result=MH_EnableHook(MH_ALL_HOOKS);if(result!=MH_OK)throw std::runtime_error(MH_StatusToString(result));running=true;return true;}
        for(auto& item:mt2::observation_hooks())hook(item.target,item.replacement,item.original);
        auto command=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr))+0x4b9be0;
        constexpr unsigned char command_guard[]={0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xd8,0x03,0,0};
        if(std::memcmp(command,command_guard,sizeof(command_guard)))throw std::runtime_error("Native title command prolog differs");
        hook(command,reinterpret_cast<void*>(title_command),reinterpret_cast<void**>(&original_title_command));
        auto save=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr))+0x4a0490;
        constexpr unsigned char save_guard[]={0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x28,0x48,0x89,0xce,0x48,0x89,0xd7};
        if(std::memcmp(save,save_guard,sizeof(save_guard)))throw std::runtime_error("Native save command prolog differs");
        hook(save,reinterpret_cast<void*>(save_command),reinterpret_cast<void**>(&original_save_command));
        hook(reinterpret_cast<void*>(GetProcAddress(sdl,"SDL_GL_SwapWindow")),reinterpret_cast<void*>(swap),reinterpret_cast<void**>(&original_swap));
        hook(reinterpret_cast<void*>(GetProcAddress(sdl,"SDL_PollEvent")),reinterpret_cast<void*>(poll),reinterpret_cast<void**>(&original_poll));
        hook(reinterpret_cast<void*>(GetProcAddress(sdl,"SDL_HideCursor")),reinterpret_cast<void*>(hide_cursor),reinterpret_cast<void**>(&original_hide));
        hook(reinterpret_cast<void*>(GetProcAddress(sdl,"SDL_SetWindowRelativeMouseMode")),reinterpret_cast<void*>(relative_mouse),reinterpret_cast<void**>(&original_relative));
        auto physfs=GetModuleHandleW(L"libphysfs.dll");
        hook(reinterpret_cast<void*>(GetProcAddress(physfs,"PHYSFS_openRead")),reinterpret_cast<void*>(open_read),reinterpret_cast<void**>(&original_open_read));
        hook(reinterpret_cast<void*>(GetProcAddress(physfs,"PHYSFS_openWrite")),reinterpret_cast<void*>(open_write),reinterpret_cast<void**>(&original_open_write));
        hook(reinterpret_cast<void*>(GetProcAddress(physfs,"PHYSFS_close")),reinterpret_cast<void*>(close_file),reinterpret_cast<void**>(&original_close));
        hook(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"MoveFileExW")),reinterpret_cast<void*>(move_file),reinterpret_cast<void**>(&original_move));
        start_save_worker();
        result=MH_EnableHook(MH_ALL_HOOKS);if(result!=MH_OK)throw std::runtime_error(std::string("Enable hooks failed: ")+MH_StatusToString(result));running=true;return true;
    }catch(const std::exception& error){report(error.what());auto disabled=MH_DisableHook(MH_ALL_HOOKS);if(disabled!=MH_OK&&disabled!=MH_ERROR_NOT_INITIALIZED)report(std::string("Initialization rollback failed: ")+MH_StatusToString(disabled));return false;}
}
extern "C" __declspec(dllexport) void MT2FW_Shutdown() {
    std::lock_guard lock(gate);running=false;
    if(save_worker.joinable()){save_worker.request_stop();save_wake.notify_all();save_worker.join();}
    try {if(runtime){runtime->world_leave();runtime.reset();}}
    catch(const std::exception& error){OutputDebugStringA(error.what());}
    auto result=MH_DisableHook(MH_ALL_HOOKS);if(result!=MH_OK&&result!=MH_ERROR_NOT_INITIALIZED)OutputDebugStringA(MH_StatusToString(result));
}
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH&&!DisableThreadLibraryCalls(module))OutputDebugStringA("warn: framework thread notifications remain enabled\n");return TRUE;}
