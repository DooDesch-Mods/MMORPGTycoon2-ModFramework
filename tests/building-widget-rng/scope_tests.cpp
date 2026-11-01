#include "determinism_presentation_rng.h"
#include <iostream>
#include <stdexcept>
namespace p=mt2fw::mt2::determinism::presentation;
void need(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main()try{
    p::Local local;p::Counters count;p::BuildingWidgetQuery scope;int original=1,other=2;unsigned initialized=0;
    auto seed=[&](void* value,uint64_t){++initialized;static_cast<p::RandomState*>(value)->s[0]=42;};
    auto select=[&](bool armed,uint64_t generation,uintptr_t helper,uintptr_t caller,void* source){
        return p::select_building_widget(armed,generation,scope,helper,caller,source,&original,local,count,seed);};
    need(select(true,1,0x9fcf20,0xda6f2,&original).source==&original,"inactive widget scope changed source");
    scope.active=true;
    need(select(false,1,0x9fcf20,0xda6f2,&original).source==&original,"unarmed scope changed source");
    need(select(true,1,0x9fcf20,0xda6f3,&original).source==&original,"wrong caller changed source");
    need(select(true,1,0x9fcf21,0xda6f2,&original).source==&original,"wrong helper changed source");
    need(select(true,1,0x9fcf20,0xda6f2,&other).source==&other&&count.nondefault==1,"nondefault source changed");
    bool threw=false;try{select(true,0,0x9fcf20,0xda6f2,&original);}catch(const std::runtime_error&){threw=true;}
    need(threw,"zero generation accepted");
    auto first=select(true,1,0x9fcf20,0xda6f2,&original);
    need(first.source==&local.random&&first.count==&count&&initialized==1,"exact scope did not route to local source");
    ++first.count->completed;
    auto second=select(true,1,0x9fcf20,0xda6f2,&original);++second.count->completed;
    need(initialized==1&&count.attempted==2&&count.completed==2,"same generation reinitialized or lost completion");
    auto third=select(true,2,0x9fcf20,0xda6f2,&original);++third.count->completed;
    need(initialized==2&&local.generation==2,"new generation did not reinitialize");
    scope.active=false;
    need(select(true,2,0x9fcf20,0xda6f2,&original).source==&original,"BuildingTick path outside widget scope changed source");
    std::cout<<"exact widget selector, fallthrough and generation cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
