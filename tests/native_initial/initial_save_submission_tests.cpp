#include <nlohmann/json.hpp>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include "initial_save_path.h"
using Json=nlohmann::json;
namespace fs=std::filesystem;
using DWORD=unsigned long;
struct Runtime {
    bool ambiguous=false;
    int initial_captures=0,ordinary_captures=0;
    Json capture_initial_save(const std::string& folder){++initial_captures;if(ambiguous)throw std::runtime_error("uninitialized mod data");return {{"phase","native_initial"},{"world",folder}};}
    Json capture_save(const std::string& folder){++ordinary_captures;return {{"world",folder}};}
} fixture_runtime;
Runtime* runtime=&fixture_runtime;
std::mutex gate;
bool running=true,prepared=true,native_valid=true;
DWORD engine_thread=7;
uintptr_t fixture_caller=0x104a2c3a;
int prepares=0,original_calls=0,checks=0;
bool actual_absolute=false,had_scope=false,write_failed=false,twice=false;
const void* actual_filename=nullptr;
std::string failure,write_name="save/Fresh/initial_state.vrz";
Json attached;
void expect(bool ok,const char* cause){++checks;if(!ok)throw std::runtime_error(cause);}
DWORD GetCurrentThreadId(){return 7;}
void* GetModuleHandleW(const wchar_t*){return reinterpret_cast<void*>(static_cast<uintptr_t>(0x10000000));}
void* fixture_return_address(){return reinterpret_cast<void*>(fixture_caller);}
std::string path_text(const fs::path& path){return path.string();}
void report(const std::string& cause){failure=cause;}
bool prepare_save(){++prepares;return prepared;}
DWORD command_view(const void* pointer,const char*& data,size_t& length){if(!pointer)return 87;const auto& value=*static_cast<const std::string*>(pointer);data=value.data();length=value.size();return 0;}
DWORD command_copy(const char* data,char* output,size_t size){std::memcpy(output,data,size);return 0;}
namespace mt2mp::game::native {
bool read_initial_save_world(void*,std::string& folder,std::string& error){if(!native_valid){error="wrong native state";return false;}folder="Fresh";return true;}
}
void original_impl(void*,const void*,bool);
using SaveCommand=void(*)(void*,const void*,bool);
SaveCommand original_save_command=original_impl,original_blocking_save=original_impl;
#define _ReturnAddress fixture_return_address
#include "save_submission.inc"
#undef _ReturnAddress
void original_impl(void*,const void* filename,bool absolute){
    ++original_calls;actual_filename=filename;actual_absolute=absolute;had_scope=initial_save_scope!=nullptr;
    if(had_scope) {
        try {
            attached=capture_save_metadata(write_name.c_str(),fs::path(write_name));
            if(twice)capture_save_metadata(write_name.c_str(),fs::path(write_name));
        }catch(const std::runtime_error& error){write_failed=true;failure=error.what();}
    }
}
void reset(){
    expect(initial_save_scope==nullptr,"scope restored after native return");fixture_runtime={};runtime=&fixture_runtime;
    running=prepared=native_valid=true;engine_thread=7;fixture_caller=0x104a2c3a;
    prepares=original_calls=0;actual_absolute=had_scope=write_failed=twice=false;actual_filename=nullptr;
    failure.clear();attached=Json();write_name="save/Fresh/initial_state.vrz";
}
int main(){
    try {
        std::string stem="initial_state";
        reset();save_blocking(nullptr,&stem,false);
        expect(original_calls==1 && prepares==0 && had_scope && !write_failed && attached.at("phase")=="native_initial","known initial native caller uses immutable pre-ready metadata once");
        expect(actual_filename==&stem && !actual_absolute && fixture_runtime.ordinary_captures==0,"native arguments and absolute semantics preserved");
        reset();write_name="tmp/save/Fresh/initial_state.vrz.17";save_blocking(nullptr,&stem,false);
        expect(!write_failed && had_scope,"native temporary initial path bound to same snapshot");
        for(const auto& invalid:{"save/Other/initial_state.vrz","save/Fresh/autosave.vrz","tmp/save/Fresh/initial_state.vrz.x","save/Fresh/../initial_state.vrz"}){
            reset();write_name=invalid;save_blocking(nullptr,&stem,false);
            expect(original_calls==1 && write_failed && attached.is_null() && fixture_runtime.ordinary_captures==0,"wrong initial path never receives metadata or ordinary fallback");
        }
        reset();twice=true;save_blocking(nullptr,&stem,false);
        expect(original_calls==1 && write_failed && fixture_runtime.initial_captures==1,"second write cannot reuse initial ownership");
        reset();fixture_runtime.ambiguous=true;save_blocking(nullptr,&stem,false);
        expect(original_calls==1 && write_failed && attached.is_null() && failure.find("uninitialized mod data")!=std::string::npos,"ambiguous mod data remains an explicit metadata failure while original save runs once");
        reset();native_valid=false;save_blocking(nullptr,&stem,false);
        expect(original_calls==1 && write_failed && fixture_runtime.initial_captures==0,"native state guard rejects metadata before core capture");
        reset();save_blocking(nullptr,&stem,true);
        expect(original_calls==1 && actual_absolute && write_failed && fixture_runtime.initial_captures==0,"absolute initial path rejected without changing native bool");
        reset();engine_thread=8;save_blocking(nullptr,&stem,false);
        expect(original_calls==1 && write_failed && fixture_runtime.initial_captures==0,"wrong thread cannot capture initial metadata");
        reset();fixture_caller=0x104a3b80;save_blocking(nullptr,&stem,true);
        expect(original_calls==1 && prepares==1 && !had_scope && actual_absolute,"ordinary blocking path preserves normal preparation and arguments");
        reset();prepared=false;fixture_caller=0x104a3b80;save_blocking(nullptr,&stem,false);
        expect(original_calls==0 && prepares==1,"ordinary preparation failure is still blocking");
        reset();running=false;save_blocking(nullptr,&stem,true);
        expect(original_calls==1 && prepares==0 && !had_scope && actual_absolute,"stopped runtime forwards original without metadata");
        reset();save_command(nullptr,&stem,true);
        expect(original_calls==1 && prepares==1 && !had_scope && actual_absolute,"ordinary async behavior unchanged");
        reset();auto regular=capture_save_metadata("save/Fresh/autosave.vrz",fs::path("save/Fresh/autosave.vrz"));
        expect(fixture_runtime.ordinary_captures==1 && regular.at("world")=="Fresh","out-of-scope writes use strict ordinary capture");
        std::cout<<checks<<" initial submission conditions passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
