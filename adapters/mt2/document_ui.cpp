#include "document_ui.h"
#include <imgui.h>
#include <windows.h>
#include <gl/GL.h>
#include <fstream>
#include <array>
#include <algorithm>
#include <set>
#include <cstring>

namespace mt2fw::document_ui {
namespace {
ImFont* body=nullptr;
ImFont* heading=nullptr;
struct Texture{GLuint handle=0;uint32_t width=0,height=0;uint64_t used=0;std::string owner;};
std::map<std::string,Texture> textures;
uint64_t frame=0;
constexpr ImVec4 ink{0.949f,0.969f,0.984f,1},muted{0.694f,0.761f,0.808f,1};
constexpr ImVec4 cyan{0.392f,0.894f,0.835f,1},surface{0.047f,0.094f,0.137f,1};
void require(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
void validate(const Json& node,size_t depth,size_t& count){
    require(node.is_object()&&depth<=8&&++count<=200,"UI document exceeds its depth or widget budget");
    auto kind=node.value("type",std::string());
    static const std::set<std::string> types{"text","button","input","selectable","image","separator","space","panel","columns"};
    require(types.contains(kind),"Unknown UI document widget: "+kind);
    for(auto key:{"id","text","title","subtitle","value","hint","tone","image"})if(node.contains(key))require(node[key].is_string()&&node[key].get_ref<const std::string&>().size()<=2048,"Invalid UI widget string");
    for(auto key:{"width","height"})if(node.contains(key))require(node[key].is_number()&&node[key].get<double>()>=0&&node[key].get<double>()<=4096,"Invalid UI widget dimension");
    for(auto key:{"disabled","selected","primary"})if(node.contains(key))require(node[key].is_boolean(),"Invalid UI widget state");
    if(kind=="panel"||kind=="columns"){
        require(node.contains("children")&&node["children"].is_array(),"UI container needs children");
        if(kind=="columns")require(!node["children"].empty()&&node["children"].size()<=4,"UI columns require between one and four children");
        for(auto& child:node["children"])validate(child,depth+1,count);
    }
}
struct End{enum Kind{Window,Child,Table,Disabled,Font,Group}kind;~End(){switch(kind){case Window:ImGui::End();break;case Child:ImGui::EndChild();break;case Table:ImGui::EndTable();break;case Disabled:ImGui::EndDisabled();break;case Font:ImGui::PopFont();break;case Group:ImGui::EndGroup();break;}}};
void image(const std::string& owner,const std::string& id,float width,float height){
    auto found=textures.find(owner+":"+id);if(found==textures.end()){ImGui::TextColored(muted,"Preview loading...");return;}
    auto& texture=found->second;texture.used=frame;
    float available=ImGui::GetContentRegionAvail().x;if(width<=0||width>available)width=available;
    float scale=(std::min)(width/texture.width,height>0?height/texture.height:width/texture.width);
    ImVec2 size{texture.width*scale,texture.height*scale};
    if(size.x<available)ImGui::SetCursorPosX(ImGui::GetCursorPosX()+(available-size.x)*0.5f);
    ImGui::Image(static_cast<ImTextureID>(texture.handle),size);
}
void node(const Json& value,const std::string& owner,Json& events,size_t index){
    auto type=value.at("type").get<std::string>();auto id=value.value("id",std::to_string(index));ImGui::PushID(id.c_str());
    struct PopId{~PopId(){ImGui::PopID();}} pop;
    bool disabled=value.value("disabled",false);ImGui::BeginDisabled(disabled);End disable{End::Disabled};
    if(type=="text"){
        auto text=value.value("text",std::string()),tone=value.value("tone",std::string());
        auto font=tone=="heading"&&heading?heading:body;
        if(font)ImGui::PushFont(font);else ImGui::PushFont(ImGui::GetFont());End end{End::Font};
        ImGui::PushStyleColor(ImGuiCol_Text,tone=="muted"?muted:tone=="success"?cyan:tone=="error"?ImVec4{1,0.66f,0.56f,1}:ink);
        ImGui::PushTextWrapPos(0);ImGui::TextUnformatted(text.c_str());ImGui::PopTextWrapPos();ImGui::PopStyleColor();
    }else if(type=="button"){
        bool primary=value.value("primary",false);if(primary){ImGui::PushStyleColor(ImGuiCol_Button,cyan);ImGui::PushStyleColor(ImGuiCol_ButtonHovered,{0.49f,0.95f,0.89f,1});ImGui::PushStyleColor(ImGuiCol_ButtonActive,{0.29f,0.77f,0.72f,1});ImGui::PushStyleColor(ImGuiCol_Text,surface);}
        bool selected=value.value("selected",false)&&!primary;if(selected){ImGui::PushStyleColor(ImGuiCol_Button,{0.06f,0.27f,0.29f,1});ImGui::PushStyleColor(ImGuiCol_Text,cyan);}
        auto label=value.value("text",std::string());if(ImGui::Button(label.c_str(),{value.value("width",0.0f)==0?ImGui::GetContentRegionAvail().x:value.value("width",0.0f),38}))events["clicked"].push_back(id);
        if(primary)ImGui::PopStyleColor(4);
        if(selected)ImGui::PopStyleColor(2);
    }else if(type=="input"){
        std::array<char,257> buffer{};auto current=value.value("value",std::string());std::memcpy(buffer.data(),current.data(),(std::min)(current.size(),buffer.size()-1));
        ImGui::SetNextItemWidth(-1);if(ImGui::InputTextWithHint("##value",value.value("hint",std::string()).c_str(),buffer.data(),buffer.size()))events["values"][id]=std::string(buffer.data());
    }else if(type=="selectable"){
        auto position=ImGui::GetCursorScreenPos();auto width=ImGui::GetContentRegionAvail().x;
        if(ImGui::Selectable("##select",value.value("selected",false),0,{width,68}))events["clicked"].push_back(id);
        auto draw=ImGui::GetWindowDrawList();auto title=value.value("title",std::string());auto subtitle=value.value("subtitle",std::string());
        draw->PushClipRect(position,{position.x+width,position.y+68},true);
        draw->AddText({position.x+12,position.y+10},ImGui::GetColorU32(ink),title.c_str());
        draw->AddText({position.x+12,position.y+37},ImGui::GetColorU32(muted),subtitle.c_str());draw->PopClipRect();
    }else if(type=="image")image(owner,value.value("image",std::string()),value.value("width",0.0f),value.value("height",0.0f));
    else if(type=="separator")ImGui::Separator();
    else if(type=="space")ImGui::Dummy({0,value.value("height",12.0f)});
    else if(type=="panel"){
        ImGui::BeginChild("##panel",{0,value.value("height",0.0f)},ImGuiChildFlags_AlwaysUseWindowPadding);End child{End::Child};
        size_t i=0;for(auto& item:value.at("children"))node(item,owner,events,i++);
    }else if(type=="columns"){
        auto& children=value.at("children");if(ImGui::BeginTable("##columns",int(children.size()),ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_NoSavedSettings)){
            End table{End::Table};for(auto& child:children){float width=child.value("width",0.0f);ImGui::TableSetupColumn("",width>0?ImGuiTableColumnFlags_WidthFixed:ImGuiTableColumnFlags_WidthStretch,width>0?width:1.0f);}
            ImGui::TableNextRow();for(size_t i=0;i<children.size();++i){ImGui::TableSetColumnIndex(int(i));node(children[i],owner,events,i);}
        }
    }
}
}
void initialize(){
    if(ImGui::GetIO().Fonts->Fonts.empty())ImGui::GetIO().Fonts->AddFontDefault();
    wchar_t windows[MAX_PATH]{};if(!GetWindowsDirectoryW(windows,MAX_PATH))throw std::runtime_error("Cannot locate Windows UI fonts");
    auto path=fs::path(windows)/"Fonts/segoeui.ttf",bold=fs::path(windows)/"Fonts/seguisb.ttf";
    if(fs::is_regular_file(path))body=ImGui::GetIO().Fonts->AddFontFromFileTTF(path.string().c_str(),18);
    if(fs::is_regular_file(bold))heading=ImGui::GetIO().Fonts->AddFontFromFileTTF(bold.string().c_str(),25);
    if(!body||!heading)OutputDebugStringA("warn: Framework document UI uses the default font because Windows Segoe UI is unavailable");
    ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
}
Json request(const std::string& operation,const Json& args,const fs::path& profile){
    auto owner=args.at("mod_id").get<std::string>();
    if(operation=="ui.image.load"){
        auto id=args.at("id").get<std::string>();require(id.size()<=128&&!id.empty(),"Invalid image identity");auto key=owner+":"+id;
        if(auto found=textures.find(key);found!=textures.end()){found->second.used=frame;return {{"id",id},{"ready",true}};}
        size_t owned=0;auto oldest=textures.end();for(auto item=textures.begin();item!=textures.end();++item)if(item->second.owner==owner){++owned;if(oldest==textures.end()||item->second.used<oldest->second.used)oldest=item;}
        if(owned>=16&&oldest!=textures.end()){glDeleteTextures(1,&oldest->second.handle);textures.erase(oldest);}
        auto storage=confined(profile,fs::path("data")/owner);
        auto path=confined(storage,fs::u8path(args.at("path").get<std::string>()));
        std::ifstream stream(path,std::ios::binary|std::ios::ate);require(bool(stream),"UI preview cache is unavailable");auto size=stream.tellg();require(size>=16&&size<=16+2048*2048*4,"UI preview cache exceeds its image budget");stream.seekg(0);
        std::array<char,8> marker{};uint32_t width=0,height=0;stream.read(marker.data(),8);stream.read(reinterpret_cast<char*>(&width),4);stream.read(reinterpret_cast<char*>(&height),4);
        require(marker==std::array<char,8>{'M','2','R','G','B','A','1',0}&&width&&height&&width<=2048&&height<=2048&&size==std::streamoff(16+size_t(width)*height*4),"Invalid UI preview cache");
        std::vector<unsigned char> pixels(size_t(width)*height*4);stream.read(reinterpret_cast<char*>(pixels.data()),std::streamsize(pixels.size()));require(bool(stream),"UI preview cache read failed");
        using BindBuffer=void(APIENTRY*)(GLenum,GLuint);
        auto bind_buffer=reinterpret_cast<BindBuffer>(wglGetProcAddress("glBindBuffer"));require(bind_buffer!=nullptr,"UI image upload buffer entry point unavailable");
        GLint previous=0,alignment=0,row_length=0,skip_rows=0,skip_pixels=0,unpack_buffer=0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&previous);glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row_length);glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skip_rows);glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skip_pixels);glGetIntegerv(0x88EF,&unpack_buffer);
        Texture texture;texture.width=width;texture.height=height;texture.owner=owner;texture.used=frame;
        glGenTextures(1,&texture.handle);require(texture.handle!=0,"UI image texture allocation failed");
        glBindTexture(GL_TEXTURE_2D,texture.handle);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);bind_buffer(0x88EC,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,GLsizei(width),GLsizei(height),0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());GLint uploaded=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&uploaded);
        glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);glPixelStorei(GL_UNPACK_ROW_LENGTH,row_length);glPixelStorei(GL_UNPACK_SKIP_ROWS,skip_rows);glPixelStorei(GL_UNPACK_SKIP_PIXELS,skip_pixels);bind_buffer(0x88EC,GLuint(unpack_buffer));glBindTexture(GL_TEXTURE_2D,GLuint(previous));
        if(uploaded!=GLint(width)){glDeleteTextures(1,&texture.handle);throw std::runtime_error("UI image upload was not confirmed");}
        textures.emplace(key,texture);return {{"id",id},{"ready",true}};
    }
    require(operation=="ui.document","Unknown document UI operation");auto definition=args.at("document");size_t count=0;validate(definition.at("content"),0,count);
    auto id=definition.value("id",std::string("main")),title=definition.at("title").get<std::string>();require(title.size()<=128&&id.size()<=128,"UI document title or identity is too long");
    auto display=ImGui::GetIO().DisplaySize;float width=(std::min)(definition.value("width",1120.0f),(std::max)(360.0f,display.x-48));float height=(std::min)(definition.value("height",660.0f),(std::max)(240.0f,display.y-72));
    ImGui::SetNextWindowPos({display.x*0.5f,display.y*0.5f},ImGuiCond_FirstUseEver,{0.5f,0.5f});ImGui::SetNextWindowSize({width,height},ImGuiCond_FirstUseEver);ImGui::SetNextWindowSizeConstraints({640,360},{4096,4096});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{16,16});ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{12,9});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{10,10});ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,6);ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,4);ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,4);
    std::array<std::pair<ImGuiCol,ImVec4>,13> colors{{{ImGuiCol_WindowBg,surface},{ImGuiCol_ChildBg,{0.062f,0.121f,0.168f,1}},{ImGuiCol_Text,ink},{ImGuiCol_TextDisabled,muted},{ImGuiCol_Border,{0.17f,0.25f,0.3f,1}},{ImGuiCol_TitleBgActive,{0.07f,0.15f,0.20f,1}},{ImGuiCol_Button,{0.1f,0.23f,0.29f,1}},{ImGuiCol_ButtonHovered,{0.14f,0.32f,0.39f,1}},{ImGuiCol_ButtonActive,{0.16f,0.39f,0.43f,1}},{ImGuiCol_FrameBg,{0.04f,0.09f,0.13f,1}},{ImGuiCol_Header,{0.07f,0.30f,0.32f,1}},{ImGuiCol_HeaderHovered,{0.1f,0.25f,0.29f,1}},{ImGuiCol_HeaderActive,{0.08f,0.35f,0.36f,1}}}};
    for(auto& [key,color]:colors)ImGui::PushStyleColor(key,color);
    if(body)ImGui::PushFont(body);else ImGui::PushFont(ImGui::GetFont());
    struct Styles{~Styles(){ImGui::PopFont();ImGui::PopStyleColor(13);ImGui::PopStyleVar(6);}} styles;
    bool open=true;Json events={{"clicked",Json::array()},{"values",Json::object()},{"open",true}};
    bool visible=ImGui::Begin((title+"###"+owner+":"+id).c_str(),&open,ImGuiWindowFlags_NoCollapse);End end{End::Window};
    if(visible){node(definition.at("content"),owner,events,0);if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::IsKeyPressed(ImGuiKey_Escape))open=false;}events["open"]=open;return events;
}
void collect(const Runtime& runtime){
    ++frame;std::set<std::string> active;for(auto& mod:runtime.plugins())if(mod->active)active.insert(mod->manifest.id);
    for(auto item=textures.begin();item!=textures.end();){if(!active.contains(item->second.owner)||frame-item->second.used>1800){glDeleteTextures(1,&item->second.handle);item=textures.erase(item);}else ++item;}
}
void shutdown(){for(auto& [id,texture]:textures)glDeleteTextures(1,&texture.handle);textures.clear();body=nullptr;heading=nullptr;}
}
