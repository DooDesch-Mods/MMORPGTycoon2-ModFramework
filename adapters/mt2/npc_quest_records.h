#pragma once

#include "npc_census.h"
#include "npc_quest_state.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::npc_quest_records {

enum class TaskStatus { Absent, Pending, FinishedWaitingTick };

struct Local {
    uintptr_t quest=0;
    uintptr_t owner=0;
    uintptr_t destination=0;
    bool npc_list_member=false;
    bool owner_weak_confirmed=false;
    bool destination_weak_confirmed=false;
    bool manager_member=false;
    TaskStatus task=TaskStatus::Absent;
};

struct State {
    std::vector<Record> records;
    std::vector<Local> local;
};

inline bool reject(std::string& error,const std::string& reason){
    error=reason;return false;
}

template<class T>
bool get(npc_census::Memory& memory,uintptr_t base,size_t offset,T& value,
         const char* label,std::string& error){
    if(!base||base>UINTPTR_MAX-offset||
       base+offset>UINTPTR_MAX-(sizeof value-1))
        return reject(error,std::string("Quest ")+label+" address null or overflows");
    if(!memory.copy(base+offset,&value,sizeof value,error)){
        error=std::string("Quest ")+label+" unreadable: "+
            (error.empty()?"native memory copy failed without cause":error);
        return false;
    }
    return true;
}

inline bool read_string(npc_census::Memory& memory,uintptr_t quest,size_t offset,
                        const char* label,std::string& value,std::string& error){
    uintptr_t data=0;size_t length=0;
    if(!get(memory,quest,offset,data,label,error)||
       !get(memory,quest,offset+8,length,label,error))return false;
    if(!data||length>65536||data>UINTPTR_MAX-length)
        return reject(error,std::string("Quest ")+label+" native string span invalid");
    value.resize(length);
    if(length&&!memory.copy(data,value.data(),length,error)){
        error=std::string("Quest ")+label+" characters unreadable: "+error;
        return false;
    }
    if(value.find('\0')!=std::string::npos||!npc_census::utf8(value))
        return reject(error,std::string("Quest ")+label+" NUL or invalid UTF-8");
    return true;
}

inline bool read_daily(npc_census::Memory& memory,uintptr_t image,uintptr_t quest,
                       size_t offset,const char* label,DailyCount& out,std::string& error){
    uintptr_t ap=0,data=0;int32_t count=0,capacity=0;
    const uintptr_t daily=quest+offset;
    if(daily<quest)return reject(error,std::string("Quest ")+label+" address overflows");
    if(!get(memory,daily,0x38,ap,label,error))return false;
    if(ap!=image+0x1749370)
        return reject(error,std::string("Quest ")+label+" history array AP differs");
    uintptr_t daily_ap=0;
    if(!get(memory,daily,0,daily_ap,label,error))return false;
    if(daily_ap!=image+0x16e4710)
        return reject(error,std::string("Quest ")+label+" DailyCount AP differs");
    if(!get(memory,daily,0x40,data,label,error)||
       !get(memory,daily,0x48,count,label,error)||
       !get(memory,daily,0x4c,capacity,label,error)||
       !get(memory,daily,0x50,out.today,label,error)||
       !get(memory,daily,0x54,out.total,label,error))return false;
    if(count<0||capacity<count||capacity>100000||
       (count&&!data)||data>UINTPTR_MAX-size_t(count)*sizeof(int32_t))
        return reject(error,std::string("Quest ")+label+" history pointer/count/capacity differs");
    out.history.resize(size_t(count));
    if(count&&!memory.copy(data,out.history.data(),size_t(count)*sizeof(int32_t),error)){
        error=std::string("Quest ")+label+" history unreadable: "+error;
        return false;
    }
    return true;
}

inline bool read_rating(npc_census::Memory& memory,uintptr_t image,uintptr_t quest,
                        Rating& out,std::string& error){
    uintptr_t ap=0;uint8_t set=2;
    if(!get(memory,quest,0x1e0,ap,"rating AP",error)||
       !get(memory,quest,0x1f8,out.beauty,"rating beauty",error)||
       !get(memory,quest,0x1fc,out.convenience,"rating convenience",error)||
       !get(memory,quest,0x200,out.danger,"rating danger",error)||
       !get(memory,quest,0x204,set,"rating set",error))return false;
    if(ap!=image+0x1787870||set>1||!std::isfinite(out.beauty)||
       !std::isfinite(out.convenience)||!std::isfinite(out.danger))
        return reject(error,"Quest rating AP, bool or finite values differ");
    out.set=set!=0;return true;
}

// Success means the lookup completed; present is assigned only on success.
inline bool manager_lookup(npc_census::Memory& memory,uintptr_t image,uintptr_t manager,
                           uintptr_t quest,bool& present,std::string& error){
    uintptr_t ap=0,table=0;int32_t buckets=0,shift=0;
    if(!get(memory,manager,0,ap,"manager AP",error)||
       !get(memory,manager,0x18,table,"manager table",error)||
       !get(memory,manager,0x20,buckets,"manager buckets",error)||
       !get(memory,manager,0x24,shift,"manager shift",error))return false;
    if(ap!=image+0x17020b0||!table||buckets<=0||buckets>1000000||
       (buckets&(buckets-1))||shift<0||shift>31||
       uint64_t(buckets)!=(uint64_t(1)<<(32-shift))||
       table>UINTPTR_MAX-size_t(buckets)*24)
        return reject(error,"QuestManager class or hash table header differs");
    const uint32_t raw_hash=uint32_t(quest);
    const uint32_t bucket_hash=raw_hash*0x9e3779ffu;
    const size_t bucket=size_t(bucket_hash>>shift);
    if(bucket>=size_t(buckets))return reject(error,"QuestManager hash bucket outside table");
    uintptr_t node=0;
    if(!get(memory,table,bucket*24+0x10,node,"manager bucket",error))return false;
    std::set<uintptr_t> seen;
    while(node){
        if(seen.size()>=100000||!seen.insert(node).second)
            return reject(error,"QuestManager hash chain cycles or exceeds bound");
        uintptr_t key=0,next=0;uint32_t node_hash=0;
        if(!get(memory,node,0,key,"manager node key",error)||
           !get(memory,node,8,node_hash,"manager node hash",error)||
           !get(memory,node,0x10,next,"manager node next",error))return false;
        if(key==quest){
            if(node_hash!=raw_hash)return reject(error,"QuestManager member raw hash differs expected="+
                std::to_string(raw_hash)+" actual="+std::to_string(node_hash));
            present=true;return true;
        }
        node=next;
    }
    present=false;return true;
}

inline bool manager_member(npc_census::Memory& memory,uintptr_t image,uintptr_t manager,
                           uintptr_t quest,std::string& error){
    bool present=false;
    if(!manager_lookup(memory,image,manager,quest,present,error))return false;
    return present||reject(error,"QuestManager attached Quest absent from hash set");
}

template<class Destination>
bool read(npc_census::Memory& memory,uintptr_t image,uintptr_t npc,
          Destination&& destination_key,State& output,std::string& error){
    error.clear();
    if(!image||image>UINTPTR_MAX-0x1787870||!npc)
        return reject(error,"Quest reader image or NPC address invalid");
    uintptr_t npc_ap=0,manager=0,entries=0,npc_proxy=0,owner_target=0;
    int32_t count=0,capacity=0,npc_type=-1;
    if(!get(memory,npc,0,npc_ap,"NPC AP",error)||
       !get(memory,npc,0x80c,npc_type,"NPC type",error)||
       !get(memory,npc,0x28,npc_proxy,"NPC weak proxy",error)||
       !get(memory,npc,0x868,entries,"NPC Quest array",error)||
       !get(memory,npc,0x870,count,"NPC Quest count",error)||
       !get(memory,npc,0x874,capacity,"NPC Quest capacity",error)||
       !get(memory,image,0x13df410,manager,"QuestManager singleton",error))return false;
    if(npc_ap!=image+0x1742980||npc_type!=0||!npc_proxy||!manager||
       count<0||capacity<count||capacity>100000||
       (count&&!entries)||entries>UINTPTR_MAX-size_t(count)*8)
        return reject(error,"Quest NPC class, type, list or manager differs");
    if(!get(memory,npc_proxy,0,owner_target,"NPC weak target",error)||
       owner_target!=npc+0x20)
        return reject(error,"Quest NPC weak target differs");
    State result;std::set<uintptr_t> seen_quests;std::set<int32_t> seen_sequences;
    for(int32_t index=0;index<count;++index){
        Local local;Record record;uintptr_t ap=0,proxy=0,weak_target=0,task=0;
        if(!get(memory,entries,size_t(index)*8,local.quest,"NPC Quest entry",error))return false;
        if(!local.quest||!seen_quests.insert(local.quest).second)
            return reject(error,"Quest NPC array entry null or duplicate");
        local.npc_list_member=true;
        if(!get(memory,local.quest,0,ap,"AP",error)||
           !get(memory,local.quest,0x34,record.type,"type",error)||
           !get(memory,local.quest,0x78,record.action,"action",error)||
           !get(memory,local.quest,0x7c,record.monsters_to_kill,"monstersToKill",error)||
           !get(memory,local.quest,0x80,record.monster_type,"monsterType",error)||
           !get(memory,local.quest,0x84,record.sequence_id,"sequenceId",error)||
           !get(memory,local.quest,0x88,record.xp_reward,"xpReward",error)||
           !get(memory,local.quest,0x8c,record.gold_reward,"goldReward",error)||
           !get(memory,local.quest,0xb8,local.owner,"owner",error)||
           !get(memory,local.quest,0xc0,proxy,"owner weak proxy",error)||
           !get(memory,local.quest,0xf0,local.destination,"destination",error))return false;
        if(ap!=image+0x174ac10||local.owner!=npc||proxy!=npc_proxy||
           record.sequence_id<0||!seen_sequences.insert(record.sequence_id).second)
            return reject(error,"Quest AP, NPC owner or sequenceId differs");
        if(!get(memory,proxy,0,weak_target,"owner weak target",error)||
           weak_target!=npc+0x20)
            return reject(error,"Quest owner weak target differs");
        local.owner_weak_confirmed=true;
        if(!local.destination||
           !get(memory,local.quest,0xf8,proxy,"destination weak proxy",error)||
           !get(memory,local.destination,0x28,weak_target,"destination proxy",error)||
           !proxy||proxy!=weak_target||
           !get(memory,proxy,0,weak_target,"destination weak target",error)||
           weak_target!=local.destination+0x20)
            return reject(error,error.empty()?"Quest destination weak binding differs":error);
        local.destination_weak_confirmed=true;
        if(!read_string(memory,local.quest,0x38,"name",record.name,error)||
           !read_string(memory,local.quest,0x58,"description",record.description,error)||
           !read_daily(memory,image,local.quest,0x100,"accepted",record.accepted,error)||
           !read_daily(memory,image,local.quest,0x170,"completed",record.completed,error)||
           !read_rating(memory,image,local.quest,record.rating,error))return false;
        if(!get(memory,local.quest,0x218,task,"task",error))return false;
        if(task){uintptr_t task_ap=0;uint8_t finished=2;
            if(!get(memory,task,0,task_ap,"task AP",error)||
               !get(memory,task,0xe,finished,"task finished",error))return false;
            if(task_ap!=image+0x1714ac0||finished>1)
                return reject(error,"Quest PathfinderTask AP or finished bool differs");
            local.task=finished?TaskStatus::FinishedWaitingTick:TaskStatus::Pending;
        }
        if(!manager_member(memory,image,manager,local.quest,error))return false;
        local.manager_member=true;
        if(!destination_key(local.destination,record.destination_key,error)){
            if(error.empty())error="Quest canonical destination callback failed without cause";
            return false;
        }
        if(record.destination_key.empty())
            return reject(error,"Quest canonical destination key absent");
        result.records.push_back(std::move(record));result.local.push_back(local);
    }
    uintptr_t after_manager=0,after_entries=0;int32_t after_count=0;
    if(!get(memory,image,0x13df410,after_manager,"QuestManager reread",error)||
       !get(memory,npc,0x868,after_entries,"NPC Quest array reread",error)||
       !get(memory,npc,0x870,after_count,"NPC Quest count reread",error))return false;
    if(after_manager!=manager||after_entries!=entries||after_count!=count)
        return reject(error,"Quest manager or NPC Quest array changed during readback");
    output=std::move(result);error.clear();return true;
}

} // namespace mt2mp::npc_quest_records
