// SPDX-License-Identifier: MIT
#include "mt2fw/core.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
using namespace mt2fw;
fs::path checked(const fs::path& root, const std::string& name) {
    if (name.empty() || name.front()=='/' || name.find_first_of("\\:\0",0,3)!=std::string::npos)
        throw std::runtime_error("Invalid relative path: "+name);
    static const std::regex reserved(R"((con|prn|aux|nul|com[1-9]|lpt[1-9])(\..*)?)",std::regex::icase);
    size_t start=0;
    do {
        auto end=name.find('/',start);auto part=name.substr(start,end==std::string::npos?end:end-start);
        if(part.empty() || part=="." || part==".." || part.ends_with('.') || part.ends_with(' ') ||
           part.find_first_of("<>\"|?*")!=std::string::npos || std::regex_match(part,reserved))
            throw std::runtime_error("Invalid Windows path: "+name);
        if(end==std::string::npos)break;
        start=end+1;
    }while(true);
    return confined(root,name);
}
void unlinked(const fs::path& path) {
    if(fs::is_symlink(fs::symlink_status(path)))throw std::runtime_error("Linked package path refused: "+path_text(path));
#ifdef _WIN32
    auto attributes=GetFileAttributesW(path.c_str());
    if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("Linked package path refused: "+path_text(path));
#endif
}
std::string folded(std::string text) {
#ifdef _WIN32
    auto input=fs::u8path(text).wstring();
    auto count=LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_UPPERCASE,input.data(),int(input.size()),nullptr,0,nullptr,nullptr,0);
    if(!count)throw std::runtime_error("Cannot compare package paths Win32="+std::to_string(GetLastError()));
    std::wstring output(size_t(count),L'\0');
    if(!LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_UPPERCASE,input.data(),int(input.size()),output.data(),count,nullptr,nullptr,0))
        throw std::runtime_error("Cannot compare package paths Win32="+std::to_string(GetLastError()));
    return path_text(fs::path(output));
#else
    std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return char(std::tolower(c));});return text;
#endif
}
Json check(const fs::path& input) {
    auto root=fs::absolute(input);unlinked(root);
    auto source=read_json(checked(root,"mod.json"));
    auto manifest=Manifest::parse(source);
    auto entry=checked(root,manifest.entry);
    if(!fs::is_regular_file(entry))throw std::runtime_error("Entry file missing: "+manifest.entry);
    if(manifest.type=="lua" && entry.extension()!=".lua")throw std::runtime_error("Lua entry must end in .lua");
    if(manifest.type=="native" && folded(path_text(entry.extension()))!=folded(".dll"))throw std::runtime_error("Native entry must end in .dll");
    std::set<std::string> names;uint64_t bytes=0;
    for(auto it=fs::recursive_directory_iterator(root);it!=fs::recursive_directory_iterator();++it) {
        auto relative=it->path().lexically_relative(root);bool ignored=false;
        for(const auto& part:relative)if(part==".git" || part=="__pycache__" || part=="build")ignored=true;
        if(ignored){if(it->is_directory())it.disable_recursion_pending();continue;}
        unlinked(it->path());auto name=path_text(relative);checked(root,name);
        if(!it->is_regular_file())continue;
        if(!names.insert(folded(name)).second)throw std::runtime_error("Case-insensitive path conflict: "+name);
        bytes+=it->file_size();
        if(names.size()>4096 || bytes>512ull*1024*1024)throw std::runtime_error("Package exceeds 4096 files or 512 MiB");
    }
    if(manifest.type=="content") {
        auto definitions=read_json(entry);std::set<std::string> ids;
        if(!definitions.is_array() || definitions.empty())throw std::runtime_error("Content entry must contain a nonempty list");
        for(const auto& definition:definitions) {
            auto id=definition.at("id").get<std::string>();
            if(!id.starts_with(manifest.id+":") || !ids.insert(id).second)throw std::runtime_error("Content ID must be unique and namespaced: "+id);
            for(const auto* key:{"model","material"}) {
                auto name=definition.at(key).get<std::string>();
                if(!fs::is_regular_file(checked(root,name)))throw std::runtime_error(std::string("Content ")+key+" missing: "+name);
            }
        }
    }
    return {{"manifest",source},{"files",names.size()},{"bytes",bytes}};
}
int run(const std::vector<fs::path>& args) {
    if(args.size()!=3 || args[1]!="check") {
        std::cerr<<"Usage: mt2fw check <mod-directory>\nThe runtime includes validation only. Authoring tools are in the GitHub SDK download.\n";return 2;
    }
    try {std::cout<<check(args[2]).dump(2)<<'\n';return 0;}
    catch(const std::exception& error){std::cerr<<"error: "<<error.what()<<'\n';return 1;}
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){std::vector<mt2fw::fs::path> args;for(int i=0;i<argc;++i)args.emplace_back(argv[i]);return run(args);}
#else
int main(int argc,char** argv){std::vector<mt2fw::fs::path> args;for(int i=0;i<argc;++i)args.emplace_back(argv[i]);return run(args);}
#endif
