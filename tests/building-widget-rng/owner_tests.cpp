#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
constexpr unsigned MT2FW_MT2_STEP_HELD=3,MT2FW_MT2_STEP_ENTERED=4;
unsigned current_thread=7;unsigned GetCurrentThreadId(){return current_thread;}
unsigned thread=7;bool configured=true;
struct Driver {bool active=true;bool frozen()const{return active;}} driver_value,*driver=&driver_value;
struct Receipt {unsigned phase=3;uint64_t lease=1,session=2,thread=7,world_epoch=3,generation=4;};
struct Steps {Receipt value;Receipt snapshot(){return value;}} steps,*native_steps=&steps;
struct Bootstrap {struct Identity {uint64_t session=2,generation=4;} identity;uint64_t world_epoch=3;uintptr_t game=10,mode=20,state=30,world=40,map=50;} bootstrap;
struct Lifecycle {bool bootstrap_held=true;uint64_t generation=4;} lifecycle;
uintptr_t image=0x100000,owner=10;std::map<uintptr_t,uint64_t> memory;uintptr_t lookup_result=60;unsigned lookup_exception=0;
void require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
template<class T>T read(uintptr_t address){if(!memory.contains(address))throw std::runtime_error("native read failed");return T(memory.at(address));}
uint32_t building_widget_lookup(uintptr_t entry,uint64_t uid,uintptr_t& result){require(entry==image+0x47ce20&&uid==90,"lookup identity changed");result=lookup_result;return lookup_exception;}
#include "widget_owner_actual.inc"
void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void reset(){current_thread=thread=7;configured=true;driver_value.active=true;steps.value=Receipt{};bootstrap=Bootstrap{};lifecycle=Lifecycle{};owner=10;
    lookup_result=60;lookup_exception=0;memory={{image+0x13df8f0,10},{10+0x28,20},{20+0x58,30},{30+0x160,40},{40+0x158,50},
        {60,image+0x16d93c0},{60+0x18,90},{60+0x170,80},{image+0x13df440,70},{70+0x60,80}};}
int main()try{std::string e;reset();need(building_widget_owner((void*)60,e)&&e.empty(),"current Building owner rejected");
    steps.value.phase=MT2FW_MT2_STEP_ENTERED;need(building_widget_owner((void*)60,e),"entered owner rejected");
    reset();current_thread=8;need(!building_widget_owner((void*)60,e)&&e.empty(),"wrong thread did not pass through");
    reset();lifecycle.bootstrap_held=false;need(!building_widget_owner((void*)60,e)&&e.empty(),"preload did not pass through");
    reset();steps.value.phase=5;need(!building_widget_owner((void*)60,e)&&e.empty(),"failed phase did not pass through");
    reset();memory[60]=0;need(!building_widget_owner((void*)60,e)&&e.empty(),"foreign family did not pass through");
    reset();memory[60+0x170]=81;need(!building_widget_owner((void*)60,e)&&e.empty(),"foreign root did not pass through");
    reset();lookup_result=61;need(!building_widget_owner((void*)60,e)&&e.empty(),"foreign UID did not pass through");
    reset();steps.value.session=99;need(!building_widget_owner((void*)60,e)&&!e.empty(),"foreign active session lacked failure");
    reset();steps.value.generation=99;need(!building_widget_owner((void*)60,e)&&!e.empty(),"foreign active generation lacked failure");
    reset();memory[30+0x160]=41;need(!building_widget_owner((void*)60,e)&&!e.empty(),"changed active world lacked failure");
    reset();lookup_exception=42;need(!building_widget_owner((void*)60,e)&&e.find("42")!=std::string::npos,"UID lookup error lost");
    std::cout<<"actual ordered Building widget owner positive and negative cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
