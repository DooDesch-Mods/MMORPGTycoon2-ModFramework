#ifndef MT2MP_TUTORIAL_PASSIVE_READ_H
#define MT2MP_TUTORIAL_PASSIVE_READ_H
#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#else
#include "vendor/nlohmann/json.hpp"
#endif
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <vector>
namespace mt2mp::game::tutorial_observer::passive {
struct Memory {
    virtual ~Memory()=default;
    virtual bool copy(uintptr_t,void*,size_t,std::string&)=0;
    virtual bool owned_sequence_entry(uintptr_t,std::string& error){error="tutorial Sequence Update code guard differs";return false;}
};
constexpr int32_t max_scenarios=64,max_steps=2048,max_history=512;
constexpr size_t max_bytes=256*1024;
struct Witness {uintptr_t address=0;std::vector<unsigned char> bytes;};
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
class Reader {
    Memory& port_;
    std::vector<Witness> reads_;
    size_t bytes_=0;
public:
    explicit Reader(Memory& port):port_(port){}
    bool bytes(uintptr_t address,void* target,size_t count,std::string& error){
        if(!address||!count||count>max_bytes-bytes_||address>std::numeric_limits<uintptr_t>::max()-count)
            return fail(error,"tutorial read address/byte budget invalid required<=262144");
        Witness witness{address,std::vector<unsigned char>(count)};
        if(!port_.copy(address,witness.bytes.data(),count,error))return false;
        std::memcpy(target,witness.bytes.data(),count);bytes_+=count;reads_.push_back(std::move(witness));return true;
    }
    template<class T>bool at(uintptr_t owner,size_t offset,T& value,std::string& error){
        if(!owner||owner>std::numeric_limits<uintptr_t>::max()-offset)return fail(error,"tutorial owner/offset invalid");
        return bytes(owner+offset,&value,sizeof(value),error);
    }
    bool vtable(uintptr_t owner,uintptr_t expected,std::string& error){
        uintptr_t actual=0;if(!at(owner,0,actual,error))return false;
        return actual==expected||fail(error,"tutorial native owner vtable differs");
    }
    bool string(uintptr_t owner,size_t offset,std::string& value,std::string& error){
        uintptr_t data=0;uint64_t count=0;
        if(!at(owner,offset,data,error)||!at(owner,offset+8,count,error))return false;
        if(count>200)return fail(error,"tutorial native string bytes="+std::to_string(count)+" required<=200");
        std::array<char,201> text{};
        if(!bytes(data,text.data(),size_t(count)+1,error))return false;
        if(text[size_t(count)]!=0||std::memchr(text.data(),0,size_t(count)))return fail(error,"tutorial native string terminator differs");
        value.assign(text.data(),size_t(count));
        for(unsigned char c:value)if(c<32||c==127)return fail(error,"tutorial native string contains control byte");
        try{static_cast<void>(nlohmann::json(value).dump());}
        catch(const nlohmann::json::exception&){return fail(error,"tutorial native string is not valid UTF-8");}
        return true;
    }
    bool array(uintptr_t owner,size_t offset,int32_t limit,uintptr_t& data,int32_t& count,int32_t& capacity,std::string& error){
        if(!at(owner,offset,data,error)||!at(owner,offset+8,count,error)||!at(owner,offset+12,capacity,error))return false;
        if(count<0||count>limit||capacity<count||capacity>limit||(!data&&count))
            return fail(error,"tutorial array count="+std::to_string(count)+" capacity="+std::to_string(capacity)+" required 0<=count<=capacity<="+std::to_string(limit));
        return true;
    }
    bool owned_sequence_entry(uintptr_t address,std::string& error){return port_.owned_sequence_entry(address,error);}
    bool stable(std::string& error){
        for(const auto& witness:reads_){std::vector<unsigned char> again(witness.bytes.size());
            if(!port_.copy(witness.address,again.data(),again.size(),error))return false;
            if(again!=witness.bytes)return fail(error,"tutorial native owner/array/cursor/string changed during read");
        }
        return true;
    }
};
struct Type {uintptr_t vtable;const char* name;};
constexpr Type step_types[]={
    {0x16fd930,"mmoStepCommand"},{0x17206c0,"mmoStepCompleteTech"},{0x1702e50,"mmoStepCutscene"},
    {0x16dbe60,"mmoStepFade"},{0x1725e70,"mmoStepHighlightItem"},{0x171c5a0,"mmoStepHighlightUI"},
    {0x17091e0,"mmoStepLockItems"},{0x16fda00,"mmoStepMessage"},{0x16dbf30,"mmoStepSave"},
    {0x16e09f0,"mmoStepSound"},{0x16dc000,"mmoStepWait"}};
inline bool guard(Reader& reader,uintptr_t image,std::string& error){
    constexpr unsigned char instance_entry[]={0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x38,0x48,0x83,0x79,0x58};
    constexpr unsigned char sequence_entry[]={0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xa8};
    std::array<unsigned char,16> actual{};
    if(!reader.bytes(image+0x4f5dc0,actual.data(),actual.size(),error))return false;
    if(std::memcmp(actual.data(),instance_entry,actual.size()))return fail(error,"tutorial ScenarioSequenceInstance Update code guard differs");
    if(!reader.bytes(image+0x4fc4a0,actual.data(),actual.size(),error))return false;
    if(std::memcmp(actual.data(),sequence_entry,actual.size())&&!reader.owned_sequence_entry(image+0x4fc4a0,error))return false;
    return true;
}
inline bool tutorial(const std::string& id){return id=="initial_tutorial"||id=="leveltwo_tutorial";}
inline bool instance(Reader& reader,uintptr_t image,uintptr_t owner,const std::string& id,nlohmann::json& row,std::string& error){
    if(!reader.vtable(owner,image+0x173ce70,error))return false;
    uint8_t complete=0;if(!reader.at(owner,0x38,complete,error))return false;
    if(complete>1)return fail(error,"tutorial scenario complete byte invalid");
    uintptr_t history=0,sequence=0;int32_t history_count=0,history_capacity=0;
    if(!reader.array(owner,0x48,max_history,history,history_count,history_capacity,error)||!reader.at(owner,0x58,sequence,error)||
       !reader.vtable(sequence,image+0x16dbd80,error)||!reader.vtable(sequence+0x18,image+0x16ef9a0,error))return false;
    if(history_count){std::vector<unsigned char> header(size_t(history_count)*32);if(!reader.bytes(history,header.data(),header.size(),error))return false;}
    uintptr_t steps=0,current=0;int32_t count=0,capacity=0,next=0;uint8_t sequence_complete=0;
    if(!reader.array(sequence,0x20,max_steps,steps,count,capacity,error)||!reader.at(sequence,0x48,current,error)||
       !reader.at(sequence,0x50,next,error)||!reader.at(sequence,0x54,sequence_complete,error))return false;
    if(next<0||next>count||sequence_complete>1||(complete&&!sequence_complete)||(sequence_complete&&next!=count))
        return fail(error,"tutorial cursor/completion count differs from native step array");
    row={{"scenario_id",id},{"complete",complete!=0},{"sequence_complete",sequence_complete!=0},
         {"step_count",count},{"next_step_index",next},{"history_count",history_count},
         {"current_step_index",nullptr},{"current_step_type",nullptr},{"current_step_id",nullptr},{"current_step_complete",nullptr}};
    if(!current){if(next!=0)return fail(error,"tutorial null current step has nonzero next index");return true;}
    if(next==0||!steps)return fail(error,"tutorial current step has no indexed native owner");
    std::vector<uintptr_t> entries(size_t(count),uintptr_t{});
    if(!reader.bytes(steps,entries.data(),entries.size()*sizeof(uintptr_t),error))return false;
    if(entries[size_t(next-1)]!=current)return fail(error,"tutorial current pointer differs from indexed native step owner");
    size_t occurrences=0;for(const auto entry:entries)if(entry==current)++occurrences;
    if(occurrences!=1)return fail(error,"tutorial current step appears more than once in native owner array");
    uintptr_t vtable=0;uint8_t step_complete=0;std::string step_id;
    if(!reader.at(current,0,vtable,error)||!reader.at(current,0x18,step_complete,error)||!reader.string(current,0x20,step_id,error))return false;
    const char* type=nullptr;for(const auto& candidate:step_types)if(vtable==image+candidate.vtable){type=candidate.name;break;}
    if(!type||step_complete>1)return fail(error,"tutorial current step vtable/complete byte invalid");
    row["current_step_index"]=next-1;row["current_step_type"]=type;row["current_step_id"]=step_id;row["current_step_complete"]=step_complete!=0;
    return true;
}
inline bool observe(Memory& port,uintptr_t image,uintptr_t current_state,nlohmann::json& result,std::string& error){
    result=nullptr;
    try{
        if(!image||image>std::numeric_limits<uintptr_t>::max()-0x1800000)return fail(error,"tutorial native image unavailable");
        Reader reader(port);if(!guard(reader,image,error))return false;
        uintptr_t state=0,log=0,owned=0;
        if(!reader.at(image,0x13df2a0,state,error))return false;
        if(state!=current_state)return fail(error,"tutorial GameState singleton differs from current world origin");
        if(!reader.at(image,0x13df330,log,error)||!reader.at(state,0x1e8,owned,error))return false;
        if(!log||owned!=log)return fail(error,"tutorial ScenarioLog owner differs from current GameState");
        if(!reader.vtable(log,image+0x16fd7b0,error)||!reader.vtable(log+0x18,image+0x16ed6c0,error))return false;
        uintptr_t active=0,completed=0;int32_t count=0,capacity=0,completed_count=0,completed_capacity=0;
        if(!reader.array(log,0x20,max_scenarios,active,count,capacity,error)||
           !reader.array(log,0x50,max_scenarios,completed,completed_count,completed_capacity,error))return false;
        nlohmann::json rows=nlohmann::json::array();std::set<std::string> seen;std::set<uintptr_t> owners;bool active_tutorial=false;
        for(int32_t i=0;i<count;++i){uintptr_t owner=0;std::string id;
            if(!reader.at(active,size_t(i)*8,owner,error))return false;
            if(!owner||!owners.insert(owner).second)return fail(error,"tutorial active scenario owner absent or duplicated");
            uintptr_t scenario_type=0;if(!reader.at(owner,0,scenario_type,error))return false;
            if(scenario_type!=image+0x173ce70&&scenario_type!=image+0x1730e90&&scenario_type!=image+0x17384e0&&scenario_type!=image+0x1720450)
                return fail(error,"tutorial active scenario native type unsupported");
            if(!reader.string(owner,0x18,id,error))return false;
            if(id.empty()||!seen.insert(id).second)return fail(error,"tutorial active scenario ID absent or duplicated");
            if(!tutorial(id))continue;
            nlohmann::json row;if(!instance(reader,image,owner,id,row,error))return false;
            active_tutorial=active_tutorial||!row.at("complete").get<bool>();rows.push_back(std::move(row));
        }
        bool initial_complete=false,leveltwo_complete=false;std::set<std::string> completed_ids;
        for(int32_t i=0;i<completed_count;++i){std::string id;
            if(!reader.string(completed,size_t(i)*32,id,error))return false;
            if(id.empty()||!completed_ids.insert(id).second)return fail(error,"tutorial completed scenario ID absent or duplicated");
            if(id=="initial_tutorial")initial_complete=true;if(id=="leveltwo_tutorial")leveltwo_complete=true;
        }
        if(!reader.stable(error))return false;
        result={{"schema",1},{"state",active_tutorial?"active":"inactive"},
            {"active_scenario_count",count},{"completed_scenario_count",completed_count},
            {"initial_tutorial_complete",initial_complete},{"leveltwo_tutorial_complete",leveltwo_complete},{"tutorials",std::move(rows)}};
        error.clear();return true;
    }catch(const std::exception& exception){result=nullptr;return fail(error,std::string("tutorial read exception: ")+exception.what());}
}
}
#endif
