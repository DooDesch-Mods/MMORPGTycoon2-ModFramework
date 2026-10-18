#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <intrin.h>
#include "sdk/native_ownership.h"
#include "sdk/native_release.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks=0,original_calls=0,desktop_calls=0,after_hook=0,passive_calls=0,release_calls=0;
bool desktop_consumed=false;
std::vector<std::string> order,errors;
void* original_receiver=nullptr;
const void* original_command=nullptr;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
void report(const std::string& cause){errors.push_back(cause);}
void original(void* receiver,const void* command){++original_calls;order.push_back("original");original_receiver=receiver;original_command=command;}
auto original_ingame_command=&original;
std::recursive_mutex gate;
int runtime_value=1;
auto runtime=&runtime_value;
bool running=true;
struct Probe {
    uint32_t disposition=MT2FW_MT2_RELEASE_FORWARD;
    bool observer_throws=false,release_throws=false;
    MT2FW_MT2_CommandCall observation{};
    MT2FW_MT2_ReleaseCall call{};
    std::string observed_text,release_text;
    int return_frame=-1;
    void observe_command(const MT2FW_MT2_CommandCall& input){
        ++passive_calls;order.push_back("passive");observation=input;
        observed_text=input.readable?std::string(input.text,input.length):"";
        if(observer_throws)throw std::runtime_error("passive original cause");
    }
    __declspec(noinline) uint32_t release_command(const MT2FW_MT2_ReleaseCall& input){
        ++release_calls;order.push_back("release");call=input;
        release_text=input.readable?std::string(input.text,input.length):"";
        void* frames[8]{};auto count=CaptureStackBackTrace(0,8,frames,nullptr);return_frame=-1;
        for(USHORT i=0;i<count;++i)if(reinterpret_cast<uintptr_t>(frames[i])==input.native_return)return_frame=i;
        if(release_throws)throw std::runtime_error("private original cause");
        return disposition;
    }
} probe;
auto native_ownership=&probe;
namespace mt2::desktop {
bool command(void*,const void*){++desktop_calls;order.push_back("desktop");return desktop_consumed;}
}
#include "release_mode_entry.inc"
__declspec(noinline) void invoke(void* receiver,const void* command){ingame_command(receiver,command);++after_hook;}
void reset(){original_calls=desktop_calls=after_hook=passive_calls=release_calls=0;order.clear();errors.clear();probe={};desktop_consumed=false;running=true;runtime=&runtime_value;native_ownership=&probe;}
}
int main(){try{
    int receiver=1;struct NativeString {const char* data;size_t size;} command{"LevelUp",7};
    reset();invoke(&receiver,&command);
    check(original_calls==1&&after_hook==1&&desktop_calls==1&&passive_calls==1&&release_calls==1,"Forward keeps one original call and both pre-effect services");
    check(order==std::vector<std::string>{"passive","release","desktop","original"},"passive observation remains before authority desktop and original");
    check(original_receiver==&receiver&&original_command==&command&&probe.call.receiver==&receiver&&probe.call.native_command==&command,"physical original arguments are never replaced");
    check(probe.observed_text=="LevelUp"&&probe.release_text=="LevelUp"&&probe.call.thread==GetCurrentThreadId(),"borrowed copied text and actual boundary thread preserved");
    if(probe.return_frame!=2)throw std::runtime_error("Immediate native return is not caller above physical entry; unwind frame="+std::to_string(probe.return_frame));
    ++checks;
    reset();probe.disposition=MT2FW_MT2_RELEASE_SUPPRESS;invoke(&receiver,&command);
    check(!original_calls&&!desktop_calls&&after_hook==1&&passive_calls==1&&release_calls==1,"Suppress skips original but retains passive observation");
    reset();probe.disposition=MT2FW_MT2_RELEASE_FAILED;invoke(&receiver,&command);
    check(!original_calls&&!desktop_calls&&passive_calls==1,"Failed private decision never falls through");
    reset();probe.disposition=99;invoke(&receiver,&command);
    check(!original_calls&&!desktop_calls,"unexpected private decision cannot forward");
    reset();probe.release_throws=true;invoke(&receiver,&command);
    check(!original_calls&&!desktop_calls&&errors.size()==1&&errors[0].find("private original cause")!=std::string::npos,"private dispatch exception preserves cause and cannot replay original");
    reset();probe.observer_throws=true;invoke(&receiver,&command);
    check(original_calls==1&&release_calls==1&&errors.size()==1&&errors[0].find("passive original cause")!=std::string::npos,"passive observer error retains its original forwarding behavior");
    reset();desktop_consumed=true;invoke(&receiver,&command);
    check(!original_calls&&desktop_calls==1&&release_calls==1,"existing desktop handler still consumes its command");
    reset();command={"Quit",4};invoke(&receiver,&command);
    check(original_calls==1&&probe.observed_text=="Quit"&&probe.release_text=="Quit","unrelated original command is preserved for narrow service filtering");
    reset();invoke(&receiver,nullptr);
    check(original_calls==1&&!probe.observation.readable&&probe.observation.read_error==ERROR_INVALID_PARAMETER&&!probe.call.readable&&probe.call.native_command==nullptr,"unreadable native object remains distinguishable");
    reset();command={reinterpret_cast<const char*>(1),7};invoke(&receiver,&command);
    check(!probe.call.readable&&probe.call.read_error&&!probe.observation.readable,"original memory read failure reaches both services");
    reset();running=false;invoke(&receiver,&command);
    check(original_calls==1&&!passive_calls&&!release_calls&&!desktop_calls,"stopped Framework forwards unchanged");
    reset();runtime=nullptr;invoke(&receiver,&command);
    check(original_calls==1&&!release_calls&&!desktop_calls,"absent runtime forwards unchanged");
    reset();native_ownership=nullptr;command={"Quit",4};invoke(&receiver,&command);
    check(original_calls==1&&!release_calls&&desktop_calls==1,"absent ownership service preserves desktop and original");
    std::cout<<checks<<" actual Mode entry checks passed\n";return 0;
}catch(const std::exception& problem){std::cerr<<"Actual Mode entry failed after "<<checks<<": "<<problem.what()<<"\n";return 1;}}
