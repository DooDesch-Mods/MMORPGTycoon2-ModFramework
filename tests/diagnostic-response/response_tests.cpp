#include "mt2fw/core.h"
#include <windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

using Json=mt2fw::Json;
namespace fs=std::filesystem;
fs::path root;
std::optional<Json> pending_diagnostic_response;
std::string last_request,last_error,diagnostic_response_error,process_started="exact-start";
bool diagnostic_response_terminal=false;
uint64_t diagnostic_response_attempts=0;
auto previous_diagnostic=std::chrono::steady_clock::time_point{};
Json incoming,published;
int checks=0,mutations=0,publications=0,logs=0,publish_failures=0;
std::vector<std::string> levels;
std::string write_failure="Atomic JSON replacement failed Win32=32";
bool operation_failure=false,logger_failure=false,actual_files=false;

void check(bool ok,const char* cause){++checks;if(!ok)throw std::runtime_error(cause);}
struct Runtime {
    void log(const std::string&,const std::string& level,const std::string&){
        ++logs;levels.push_back(level);
        if(logger_failure)throw std::runtime_error("injected logger failure");
    }
} storage;
Runtime* runtime=&storage;
std::string path_text(const fs::path& path){return mt2fw::path_text(path);}
void report(const std::string& cause){if(last_error!=cause){last_error=cause;runtime->log("framework","error",cause);}}
Json read_json(const fs::path& path){return actual_files?mt2fw::read_json(path):incoming;}
void atomic_json(const fs::path& path,const Json& response){
    ++publications;
    if(publish_failures){--publish_failures;throw std::runtime_error(write_failure);}
    if(actual_files)mt2fw::atomic_json(path,response);
    published=response;
}
Json execute(const std::string&,const Json&){
    ++mutations;
    if(operation_failure)throw std::runtime_error("original operation failed after attempt");
    return {{"mutation",mutations}};
}

#include "diagnostic_response.inc"

void input(const std::string& id){
    incoming={{"id",id},{"pid",GetCurrentProcessId()},{"process_start_time",process_started},{"operation","mutate"},{"arguments",Json::object()}};
    if(actual_files)mt2fw::atomic_json(root/"diagnostics/request.json",incoming);
}
void reset(){
    pending_diagnostic_response.reset();last_request.clear();last_error.clear();diagnostic_response_error.clear();
    diagnostic_response_terminal=false;diagnostic_response_attempts=0;incoming=published=Json();
    mutations=publications=logs=publish_failures=0;levels.clear();operation_failure=logger_failure=actual_files=false;
    write_failure="Atomic JSON replacement failed Win32=32";input("first");
}
void pump(){previous_diagnostic=std::chrono::steady_clock::now()-std::chrono::milliseconds(101);diagnostics();}

struct Reader {
    HANDLE file;
    explicit Reader(const fs::path& path):file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)){
        if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("Test reader open failed Win32="+std::to_string(GetLastError()));
    }
    void close(){
        if(file==INVALID_HANDLE_VALUE)return;
        if(!CloseHandle(file))throw std::runtime_error("Test reader close failed Win32="+std::to_string(GetLastError()));
        file=INVALID_HANDLE_VALUE;
    }
    ~Reader(){if(file!=INVALID_HANDLE_VALUE&&!CloseHandle(file))std::cerr<<"error: test reader cleanup failed Win32="<<GetLastError()<<'\n';}
};

int main(){try{
    root=fs::current_path()/("diagnostic-response-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    fs::create_directories(root/"diagnostics");std::ofstream(root/"diagnostics/request.json")<<"{}";
    reset();publish_failures=2;pump();
    check(mutations==1&&publications==1&&pending_diagnostic_response&&published.is_null(),"first sharing failure lost completed operation");
    check(logs==1&&!diagnostic_response_terminal&&levels==std::vector<std::string>{"warn"}&&last_error.empty(),"first sharing failure not tolerated/visible/retryable");
    pump();check(mutations==1&&publications==2&&logs==1,"same failure reexecuted or spammed log");
    input("second");pump();
    check(mutations==1&&publications==3&&!pending_diagnostic_response&&published.at("id")=="first","new ID overtook pending response");
    check(logs==2&&last_error.empty()&&levels==std::vector<std::string>({"warn","info"}),"publication recovery not logged/cleared");
    input("first");pump();check(mutations==1&&publications==3,"duplicate input repeated operation");
    input("second");pump();check(mutations==2&&publications==4&&published.at("id")=="second","next ID not executed after pending response publication");

    reset();operation_failure=true;publish_failures=1;pump();
    check(mutations==1&&pending_diagnostic_response&&!pending_diagnostic_response->at("ok").get<bool>(),"completed failure response lost");
    pump();check(mutations==1&&!published.at("ok").get<bool>()&&published.at("error")=="original operation failed after attempt","failed operation replayed or reported success");

    reset();operation_failure=true;logger_failure=true;bool raised=false;
    try{pump();}catch(const std::exception&){raised=true;}
    check(raised&&mutations==1&&pending_diagnostic_response&&publications==0,"failure logging dropped completed response");
    logger_failure=false;pump();check(mutations==1&&!published.at("ok").get<bool>(),"logging failure caused operation replay");

    reset();publish_failures=1;logger_failure=true;raised=false;
    try{pump();}catch(const std::exception&){raised=true;}
    check(raised&&mutations==1&&pending_diagnostic_response,"publication logging dropped response");
    logger_failure=false;pump();check(mutations==1&&published.at("ok")==true,"publication logging failure replayed operation");

    reset();publish_failures=1;write_failure="Atomic JSON replacement failed Win32=5";pump();
    check(diagnostic_response_terminal&&pending_diagnostic_response&&mutations==1&&logs==1&&levels==std::vector<std::string>{"error"}&&!last_error.empty(),"terminal write failure not retained/visible");
    input("second");pump();pump();check(publications==1&&mutations==1&&logs==1&&pending_diagnostic_response,"terminal error retried or admitted newer mutation");
    reset();publish_failures=2;pump();write_failure="Atomic JSON replacement failed Win32=5";pump();
    check(diagnostic_response_terminal&&logs==2&&mutations==1,"publication reason change hidden");
    reset();incoming["pid"]=GetCurrentProcessId()+1;pump();check(mutations==0&&publications==0&&!pending_diagnostic_response,"wrong process request executed");

    reset();actual_files=true;input("live-first");
    const auto response_path=root/"diagnostics/response.json";
    mt2fw::atomic_json(response_path,{{"id","old"}});
    Reader reader(response_path);
    pump();
    check(mutations==1&&publications==1&&pending_diagnostic_response&&!diagnostic_response_terminal,"real sharing violation lost completed response");
    check(diagnostic_response_error.find("Win32=32")!=std::string::npos,"real ReplaceFileW did not report the expected sharing violation");
    check(mt2fw::read_json(response_path).at("id")=="old","failed publication changed the acknowledged response");
    check(mt2fw::read_json(root/"diagnostics/response.json.tmp").at("id")=="live-first","completed response was not retained in the temporary file");
    input("live-next");pump();check(mutations==1&&publications==2&&logs==1,"live reader caused replay, newer request admission or repeated logs");
    reader.close();pump();
    check(mutations==1&&publications==3&&!pending_diagnostic_response&&mt2fw::read_json(response_path).at("id")=="live-first","publication did not recover the original completed result");
    check(logs==2&&diagnostic_response_error.empty()&&diagnostic_response_attempts==3,"live publication recovery diagnostics differ");
    pump();check(mutations==2&&mt2fw::read_json(response_path).at("id")=="live-next","new request did not wait for prior response publication");
    std::cout<<checks<<" diagnostic response checks passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"Diagnostic response check failed after "<<checks<<": "<<error.what()<<'\n';return 1;}}
