#include "gameplay/regions/level_census.h"
#include <cstring>
#include <iostream>
#include <map>

using namespace mt2fw::mt2::gameplay::regions;
struct Memory {
    std::map<uintptr_t,std::vector<unsigned char>> fields;
    template<class T>void put(uintptr_t address,const T& value){auto& bytes=fields[address];bytes.resize(sizeof(T));std::memcpy(bytes.data(),&value,sizeof(T));}
    template<class T>bool read(uintptr_t object,size_t offset,T& value,std::string& cause){
        auto found=fields.find(object+offset);if(found==fields.end()||found->second.size()!=sizeof(T)){cause="fixture read missing";return false;}
        std::memcpy(&value,found->second.data(),sizeof(T));return true;
    }
};
int main(){try{
    Memory memory;constexpr uintptr_t image=0x100000,region=0x10000;
    constexpr uintptr_t managers[]{0x20000,0x30000,0x40000},actors[]{0x50000,0x60000,0x70000};
    constexpr size_t owner[]{0xc8,0x80,0xf0},first[]{0x40,0x40,0x20},offset[]{0x998,0x988,0x968};
    constexpr uintptr_t manager_ap[]{0x16e6aa0,0x1714100,0x16fe170},actor_ap[]{0x1742980,0x16c0770,0x1745ce0};
    constexpr int buckets[]{3,1,4};
    for(size_t family=0;family<3;++family){
        memory.put(region+offset[family],managers[family]);memory.put(managers[family],image+manager_ap[family]);memory.put(managers[family]+owner[family],region);
        for(int bucket=0;bucket<buckets[family];++bucket){auto base=managers[family]+first[family]+size_t(bucket)*0x30;
            memory.put(base,bucket?uintptr_t(0):uintptr_t(0x80000+family*0x100));memory.put(base+8,int32_t(bucket?0:1));memory.put(base+12,int32_t(bucket?0:1));}
        auto actor=actors[family];memory.put(0x80000+family*0x100,actor);memory.put(actor,image+actor_ap[family]);memory.put(actor+0x178,region);
        memory.put(actor+0x18,uint64_t(9007199254740993ull+family));memory.put(actor+0x100,std::array<float,10>{0,0,0,1,10,20,30,1,1,1});
        memory.put(actor+(family==2?0x360:0x6f8),int32_t(family==0?14:family==1?5:1));
        if(family!=2)memory.put(actor+0x4a4,std::array<int32_t,2>{int32_t(family),2});
    }
    memory.put(actors[0]+0x80c,int32_t(0));memory.put(actors[0]+0x870,int32_t(2));memory.put(actors[0]+0x890,int32_t(7));
    auto read=[&](uintptr_t p,size_t o,auto& v,std::string& cause){return memory.read(p,o,v,cause);};
    auto require=[](bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);};
    levels::Census before;std::string cause;require(levels::census(image,region,{2,3},before,read,cause),cause);
    require(before.counts==std::array<uint64_t,3>{1,1,1},"Incomplete native actor family census");
    require(before.expected[0][0][2]=="9007199254740993"&&before.expected[0][1]==13&&before.expected[0][2]==3,"NPC offset level or exact UID projection differs");
    require(before.expected[1][1]==3&&before.expected[2][1]==2,"Monster/Boss range projection differs");
    memory.put(actors[0]+0x6f8,int32_t(13));memory.put(actors[0]+0x890,int32_t(3));memory.put(actors[1]+0x6f8,int32_t(3));memory.put(actors[2]+0x360,int32_t(2));
    levels::Census after;require(levels::census(image,region,{2,3},after,read,cause),cause);
    require(before.roster==after.roster&&before.expected==after.levels&&after.expected==after.levels,"Complete projected native readback differs");
    memory.put(actors[1]+0x18,uint64_t(9007199254740993ull));require(!levels::census(image,region,{2,3},after,read,cause)&&cause.find("UID")!=std::string::npos,"Duplicate native UID was admitted");
    memory.put(actors[1]+0x18,uint64_t(9007199254740994ull));memory.put(actors[0]+0x80c,int32_t(1));
    require(!levels::census(image,region,{2,3},after,read,cause)&&cause.find("bucket")!=std::string::npos,"Wrong native manager bucket was admitted");
    require(!levels::census(image,region,{0,3},after,read,cause),"Invalid level range was admitted");
    std::cout<<"Region full actor census, projection and native identity checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
