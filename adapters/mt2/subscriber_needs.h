#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <string>
namespace mt2mp::game::subscriber_needs {
using Bits=std::array<uint32_t,7>;
inline bool valid(uint32_t bits){const float value=std::bit_cast<float>(bits);return std::isfinite(value)&&value>=-100.0f&&value<=100.0f;}
inline bool valid(const Bits& values,std::string& error){for(size_t i=0;i<values.size();++i)if(!valid(values[i])){error="Subscriber Needs invalid float bits index="+std::to_string(i)+" bits="+std::to_string(values[i])+" required finite -100..100";return false;}return true;}
struct Owner {uintptr_t brain=0,needs=0;bool operator==(const Owner&)const=default;};
template<class Read>bool sample(Read read,uintptr_t image,uintptr_t subscriber,int32_t required_id,Owner& out,Bits& bits,std::string& error){
    out={};uintptr_t type=0,brain=0,reverse=0;int32_t id=-1;
    if(!read(subscriber,0,type,error)||type!=image+0x16e75f0||!read(subscriber,0x20,id,error)||id!=required_id||!read(subscriber,0x88,brain,error)||!brain||brain>UINTPTR_MAX-0x1b4||!read(brain,0,type,error)||type!=image+0x16fdd60||!read(brain,0x28,reverse,error)||reverse!=subscriber||!read(brain+0x180,0,type,error)||type!=image+0x16c0cf0){if(error.empty())error="Subscriber Needs actual ID/Brain/NeedSet reciprocal owner differs";return false;}
    for(size_t i=0;i<bits.size();++i)if(!read(brain+0x180,0x18+i*4,bits[i],error))return false;
    if(!valid(bits,error))return false;out={brain,brain+0x180};return true;
}
// Resolve the current manager array for each call. C1 construction and temporary
// NeedSets are not registered Subscriber receivers and keep their original writes.
template<class Read>bool owns_receiver(Read read,uintptr_t image,uintptr_t receiver,bool& owned,std::string& error){
    owned=false;uintptr_t manager=0,type=0,data=0;int32_t count=-1,capacity=-1;
    if(!read(image,0x13df690,manager,error)||!read(manager,0,type,error)||type!=image+0x1725f40||!read(manager,0x40,data,error)||!read(manager,0x48,count,error)||!read(manager,0x4c,capacity,error)||count<0||count>1000000||capacity<count||(count&&(!data||data>UINTPTR_MAX-size_t(count)*8))){if(error.empty())error="Subscriber Needs dynamic Manager array invalid";return false;}
    for(int32_t i=0;i<count;++i){uintptr_t subscriber=0,brain=0;if(!read(data,size_t(i)*8,subscriber,error)||!read(subscriber,0x88,brain,error))return false;if(!brain||brain>UINTPTR_MAX-0x180||brain+0x180!=receiver)continue;
        if(owned){error="Subscriber Needs receiver has duplicate Manager membership";return false;}int32_t id=-1;Owner owner;Bits values;
        if(!read(subscriber,0x20,id,error)||id<0||!sample(read,image,subscriber,id,owner,values,error))return false;owned=true;
    }
    uintptr_t final_data=0;int32_t final_count=-1,final_capacity=-1;
    if(!read(manager,0x40,final_data,error)||!read(manager,0x48,final_count,error)||!read(manager,0x4c,final_capacity,error)||final_data!=data||final_count!=count||final_capacity!=capacity){if(error.empty())error="Subscriber Needs Manager changed during receiver lookup";return false;}return true;
}
}
