#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include "determinism_presentation_rng.h"
namespace p=mt2fw::mt2::determinism::presentation;
thread_local p::BuildingWidgetQuery building_widget_scope;
bool admitted=true,throw_native=false,nesting=false;unsigned calls=0;bool seen_outer=false,seen_inner=false;
bool admit_building_widget(const void*,uintptr_t){return admitted;}
void* original(void*,const void*,int,int);
using Fn=void*(*)(void*,const void*,int,int);Fn original_get_pad_entrance=original;
#include "widget_wrapper_actual.inc"
void* original(void* out,const void* building,int pad,int entrance){
    ++calls;if(!nesting){seen_outer=building_widget_scope.active;
        if(pad==1){nesting=true;admitted=false;auto nested=get_pad_entrance(out,building,2,entrance);nesting=false;admitted=true;
            if(nested!=out||!building_widget_scope.active)throw std::runtime_error("nested scope not restored");}}
    else seen_inner=building_widget_scope.active;
    if(throw_native)RaiseException(0xe0011234u,0,0,nullptr);
    return out;
}
uint32_t catch_native(void* out){__try{get_pad_entrance(out,(void*)2,0,0);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main()try{int output=0;building_widget_scope.active=false;admitted=true;calls=0;
    need(get_pad_entrance(&output,(void*)2,1,3)==&output&&calls==2&&seen_outer&&!seen_inner&&!building_widget_scope.active,"nested original output or scope differs");
    calls=0;admitted=false;seen_outer=true;need(get_pad_entrance(&output,(void*)2,0,0)==&output&&calls==1&&!seen_outer,"nonmatching original call changed");
    admitted=true;throw_native=true;building_widget_scope.active=false;need(catch_native(&output)==0xe0011234u&&!building_widget_scope.active,"native exception or TLS unwind changed");
    std::cout<<"actual widget native wrapper once-only, nested and SEH unwind cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
