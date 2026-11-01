#include <intrin.h>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include "determinism_presentation_rng.h"
namespace p=mt2fw::mt2::determinism::presentation;
using Selection=p::Selection;int global_source=1,local_source=2;p::Counters counts;
bool route=true,fail_original=false;unsigned original_calls=0,finished=0;
Selection choose(uintptr_t helper,uintptr_t,void* source){if(route&&helper==0x9fcf20){++counts.attempted;return {&local_source,&counts};}return {source};}
Selection(*select_source)(uintptr_t,uintptr_t,void*)=choose;
uintptr_t begin(uintptr_t,uintptr_t,void*){return 1;}
void finish_buffer(uintptr_t,void*,unsigned){++finished;}
#include "widget_vector_actual.inc"
void* original(void* output,void* source,const void* box){++original_calls;if(fail_original)throw std::runtime_error("native original failure");
    if(box!=(void*)3)throw std::runtime_error("box changed");*static_cast<void**>(output)=source;return output;}
void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main()try{original_vector3_box=original;void* output=nullptr;
    need(get_vector3_box(&output,&global_source,(void*)3)==&output&&output==&local_source&&original_calls==1&&finished==1&&counts.completed==1,"actual vector routing or output differs");
    route=false;need(get_vector3_box(&output,&global_source,(void*)3)==&output&&output==&global_source&&original_calls==2,"nonmatching vector source changed");
    route=true;fail_original=true;bool caught=false;try{get_vector3_box(&output,&global_source,(void*)3);}catch(const std::runtime_error& e){caught=std::string(e.what())=="native original failure";}
    need(caught&&original_calls==3&&finished==2&&counts.attempted==2&&counts.completed==1,"original exception or completion counter changed");
    std::cout<<"actual vector source routing, original output and exception cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
