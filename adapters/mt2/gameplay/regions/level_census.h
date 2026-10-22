#pragma once
#include "../context.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace mt2fw::mt2::gameplay::regions::levels {
using json=Json;
struct Census {json roster=json::array(),levels=json::array(),expected=json::array();std::array<uint64_t,3> counts{};};
inline int32_t projected(int32_t value,int32_t low,int32_t high){return std::clamp(value,low,high);}
// Pointer identities stay local. The canonical multiset binds each level to its
// manager bucket, definition and pose without relying on allocation order.
template<class Read>bool census(uintptr_t image,uintptr_t region,const std::array<int32_t,2>& range,Census& out,Read read,std::string& e){
    e.clear();
    auto fail=[&](const std::string& s){e=s;return false;};
    if(range[0]<0||(range[0]==0&&range[1]!=0)||range[1]<range[0]||range[1]>INT32_MAX/10-10)return fail("region level census range outside domain");
    Census result;std::set<uintptr_t> unique;std::set<uint64_t> uids;
    struct Family {size_t offset,owner,first;uintptr_t ap,actor_ap;int buckets;};
    constexpr Family families[]{{0x998,0xc8,0x40,0x16e6aa0,0x1742980,3},{0x988,0x80,0x40,0x1714100,0x16c0770,1},{0x968,0xf0,0x20,0x16fe170,0x1745ce0,4}};
    for(size_t family=0;family<3;++family){const auto& f=families[family];uintptr_t manager=0,ap=0,owner=0;
        if(!read(region,f.offset,manager,e)||!read(manager,0,ap,e)||!read(manager,f.owner,owner,e))return false;
        if(ap!=image+f.ap||owner!=region)return fail("region level manager class/owner differs family="+std::to_string(family));
        for(int bucket=0;bucket<f.buckets;++bucket){const auto offset=f.first+size_t(bucket)*0x30;uintptr_t data=0;int32_t count=0,capacity=0;
            if(!read(manager,offset,data,e)||!read(manager,offset+8,count,e)||!read(manager,offset+12,capacity,e))return false;
            if(count<0||count>100000||capacity<count||(count&&!data)||result.counts[family]+uint64_t(count)>100000)return fail("region level actor array bound differs");
            result.counts[family]+=uint64_t(count);
            for(int32_t i=0;i<count;++i){uintptr_t actor=0,actor_region=0;uint64_t uid=0;int32_t level=0,quest=-1,quests=0;std::array<int32_t,2> definition{};std::array<float,10> pose{};
                if(!read(data,size_t(i)*8,actor,e)||!read(actor,0,ap,e)||!read(actor,0x178,actor_region,e)||!read(actor,0x100,pose,e)||!read(actor,family==2?0x360:0x6f8,level,e))return false;
                if(ap!=image+f.actor_ap||actor_region!=region||!unique.insert(actor).second||level<0||level>INT32_MAX/10)return fail("region level actor class/membership/level differs");
                if(!read(actor,0x18,uid,e)||!uid||!uids.insert(uid).second)return fail("region level actor UID is absent or duplicated");
                for(float x:pose)if(!std::isfinite(x)||std::abs(x)>1000000)return fail("region level actor pose not finite or bounded");
                if(family!=2&&!read(actor,0x4a4,definition,e))return false;
                if(family==0){int32_t type=-1;if(!read(actor,0x80c,type,e)||type!=bucket||!read(actor,0x890,quest,e)||!read(actor,0x870,quests,e))return fail(e.empty()?"region NPC bucket differs":e);
                    if(quest<0||quests<0||quests>100000)return fail("region NPC quest level/count outside domain");}
                const json identity=json::array({family,bucket,std::to_string(uid),definition,pose,quests});
                result.roster.push_back(identity);result.levels.push_back(json::array({identity,level,quest}));
                const auto next=projected(level,range[0]+(family==0?10:0),range[1]+(family==0?10:0));
                const auto next_quest=family==0&&next!=level?projected(quest,range[0],range[1]):quest;
                result.expected.push_back(json::array({identity,next,next_quest}));
            }
            uintptr_t again=0;int32_t again_count=0;
            if(!read(manager,offset,again,e)||!read(manager,offset+8,again_count,e))return false;
            if(again!=data||again_count!=count)return fail("region level actor array changed during census");
        }
    }
    for(auto* rows:{&result.roster,&result.levels,&result.expected})std::sort(rows->begin(),rows->end());
    out=std::move(result);return true;
}
}
