#include "mt2fw/core.h"
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif
namespace mt2fw {
fs::path confined(const fs::path& root,const fs::path& relative) {
    if(relative.empty()||relative.is_absolute()||relative.has_root_name())throw std::runtime_error("Relative path required");
    auto base=fs::weakly_canonical(root),path=base;
    for(const auto& component:relative){auto s=path_text(component);
        if(s==".."||s.find(':')!=std::string::npos||s.find('\\')!=std::string::npos||s.ends_with('.')||s.ends_with(' '))throw std::runtime_error("Path component refused: "+s);
        path/=component;
        if(fs::is_symlink(fs::symlink_status(path)))throw std::runtime_error("Symbolic links are not allowed: "+path_text(path));
#ifdef _WIN32
        DWORD attributes=GetFileAttributesW(path.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT))throw std::runtime_error("Reparse point refused: "+path_text(path));
#endif
    }
    auto canonical=fs::weakly_canonical(path);auto mismatch=std::mismatch(base.begin(),base.end(),canonical.begin(),canonical.end());
    if(mismatch.first!=base.end())throw std::runtime_error("Path escapes mod root");return canonical;
}
Json read_json(const fs::path& path) {
#ifdef _WIN32
    HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open JSON Win32="+std::to_string(GetLastError())+": "+path_text(path));
    struct Close{HANDLE file;~Close(){if(!CloseHandle(file))OutputDebugStringA("error: JSON read handle close failed\n");}} close{file};
    LARGE_INTEGER length{};if(!GetFileSizeEx(file,&length)||length.QuadPart<0||length.QuadPart>4*1024*1024)throw std::runtime_error("JSON size unavailable or exceeds 4 MiB: "+path_text(path));
    std::string bytes(size_t(length.QuadPart),'\0');DWORD count=0;
    if(!ReadFile(file,bytes.data(),DWORD(bytes.size()),&count,nullptr)||count!=bytes.size())throw std::runtime_error("JSON read incomplete Win32="+std::to_string(GetLastError())+": "+path_text(path));
    return Json::parse(bytes);
#else
    if(fs::file_size(path)>4*1024*1024)throw std::runtime_error("JSON file exceeds 4 MiB: "+path_text(path));
    std::ifstream stream(path);if(!stream)throw std::runtime_error("Cannot open JSON: "+path_text(path));return Json::parse(stream);
#endif
}
void atomic_json(const fs::path& path,const Json& value) {
    auto bytes=value.dump(2);if(bytes.size()>4*1024*1024)throw std::runtime_error("JSON storage exceeds 4 MiB");
    fs::create_directories(path.parent_path());auto temporary=path;temporary+=".tmp";
    {std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);stream<<bytes<<'\n';stream.close();if(!stream)throw std::runtime_error("Cannot write JSON: "+path_text(temporary));}
#ifdef _WIN32
    BOOL replaced=fs::exists(path)?ReplaceFileW(path.c_str(),temporary.c_str(),nullptr,0,nullptr,nullptr):MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH);
    if(!replaced)throw std::runtime_error("Atomic JSON replacement failed Win32="+std::to_string(GetLastError()));
#else
    fs::rename(temporary,path);
#endif
}
}
