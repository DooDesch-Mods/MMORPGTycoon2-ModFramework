#include "document_ui.h"
#include "native_input.h"
#include <imgui.h>
#include <windows.h>
#include <fstream>
#include <iostream>

using namespace mt2fw;
namespace {
void check(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
template<class F> void rejected(F action,const std::string& reason){
    bool matched=false;
    try{action();}catch(const std::exception& error){matched=std::string(error.what()).find(reason)!=std::string::npos;}
    check(matched,("Expected rejection: "+reason).c_str());
}
Json definition(Json content){return {{"mod_id","test.cards"},{"document",{{"id","test"},{"title","Card browser"},{"content",std::move(content)}}}};}
}
int main(){
    try{
        check(native_input::limit(Json::object())==128,"Native input default limit changed");
        check(native_input::limit({{"max_bytes",240}})==240,"Chat byte limit unavailable");
        for(auto value:Json::array({0,1025,-1,128.5,"240",true}))rejected([&]{native_input::limit({{"max_bytes",value}});},"max_bytes");
        native_input::validate(std::string(240,'a'),240);
        rejected([&]{native_input::validate(std::string(241,'a'),240);},"max_bytes");
        check(native_input::prefix(std::string(237,'a')+"\xe2\x82\xac",240)==std::string(237,'a')+"\xe2\x82\xac","Complete UTF-8 boundary was lost");
        check(native_input::prefix(std::string(239,'a')+"\xe2\x82\xac",240)==std::string(239,'a'),"UTF-8 was split at byte limit");
        check(native_input::prefix(std::string(237,'a')+"\xf0\x9f\x98\x80",240)==std::string(237,'a'),"Four-byte UTF-8 was split");
        rejected([&]{native_input::prefix("\xc0\xaf",240);},"UTF-8");
        rejected([&]{native_input::prefix("\xed\xa0\x80",240);},"UTF-8");
        rejected([&]{native_input::prefix("\xf4\x90\x80\x80",240);},"UTF-8");
        rejected([&]{native_input::prefix(std::string("a\0b",3),240);},"null byte");
        ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={1280,768};io.DeltaTime=1.f/60;io.IniFilename=nullptr;
        io.Fonts->AddFontDefault();io.Fonts->Build();ImGui::NewFrame();
        auto root=fs::current_path()/"document-ui-test-data";
        auto valid=definition({{"type","panel"},{"id","library"},{"children",Json::array({{{"type","text"},{"text","Cards"}},{{"type","input"},{"id","search"},{"value",""}}})}});
        auto invoke=[&](const Json& args){return document_ui::request("ui.document",args,root);};
        auto malformed=valid;malformed["document"]["content"]["children"][0]["type"]="unknown";
        rejected([&]{invoke(malformed);},"Unknown UI document widget");
        malformed=valid;malformed["document"]["content"]["children"][0]["disabled"]="false";
        rejected([&]{invoke(malformed);},"Invalid UI widget state");
        malformed=valid;malformed["document"]["content"]["children"]=Json::array();
        for(int i=0;i<201;++i)malformed["document"]["content"]["children"].push_back({{"type","text"},{"text","Card"}});
        rejected([&]{invoke(malformed);},"widget budget");
        malformed=definition({{"type","text"},{"text","Leaf"}});
        for(int i=0;i<10;++i)malformed["document"]["content"]={{"type","panel"},{"children",Json::array({malformed["document"]["content"]})}};
        rejected([&]{invoke(malformed);},"widget budget");
        auto result=invoke(valid);
        check(result.at("clicked").empty()&&result.at("values").empty()&&result.at("open")==true,"Valid document failed after rejected documents");
        auto other=definition({{"type","text"},{"text","Independent second mod"}});other["mod_id"]="test.other";invoke(other);
        ImGui::Render();
        Json image={{"mod_id","test.cards"},{"id","preview"},{"path","../other-mod/cache.rgba"}};
        rejected([&]{document_ui::request("ui.image.load",image,root);},"Path component refused");
        image["path"]=path_text(fs::absolute(root/"outside.rgba"));
        rejected([&]{document_ui::request("ui.image.load",image,root);},"Relative path required");
        image["path"]="missing.rgba";
        rejected([&]{document_ui::request("ui.image.load",image,root);},"unavailable");
        document_ui::shutdown();ImGui::DestroyContext();
        std::cout<<"Document validation, recovery, independent documents and image confinement passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"error: document UI tests: "<<error.what()<<'\n';return 1;}
}
