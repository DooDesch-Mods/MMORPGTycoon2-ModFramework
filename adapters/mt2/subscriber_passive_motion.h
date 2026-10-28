#ifndef MT2MP_SUBSCRIBER_PASSIVE_MOTION_H
#define MT2MP_SUBSCRIBER_PASSIVE_MOTION_H
#include "subscriber_motion.h"
#include "subscriber_needs.h"
#include "region_saved_identity.h"
#if __has_include(<nlohmann/json.hpp>)
#include <nlohmann/json.hpp>
#else
#include "vendor/nlohmann/json.hpp"
#endif
#include <algorithm>
#include <bit>
#include <optional>
#include <set>
#include <vector>
namespace mt2mp::game::subscriber_passive {
using Motion=subscriber_motion::Pose;
struct MotionOwner {uintptr_t map=0,root=0,toon=0,region=0,manager=0;};
inline bool fail(std::string& e,const std::string& s){e=s;return false;}
template<class Read>bool type(Read& at,uintptr_t p,uintptr_t image,uintptr_t expected,std::string& e){uintptr_t value=0;return at(p,0,value,e)&&(value==image+expected||fail(e,"economic actual class addresspoint differs"));}
template<class Memory,class Read,class World,class Region,class Member>
bool motion(Memory& memory,Read& at,uintptr_t image,uintptr_t subscriber,World&& world,Region&& region_reader,Member&& member,
            std::optional<Motion>& pose,std::string& e,MotionOwner* selected=nullptr){
    auto ap=[&](uintptr_t object,uintptr_t base,uintptr_t expected,std::string& error){return type(at,object,base,expected,error);};

    pose.reset();uintptr_t toon=0;if(!at(subscriber,0x80,toon,e))return false;
    if(!toon)return true;
    uintptr_t backlink=0;uint8_t logged=0;if(!ap(toon,image,0x1744e90,e)||!at(toon,0x940,backlink,e)||backlink!=subscriber||!at(toon,0x825,logged,e)||logged>1)return fail(e,e.empty()?"Subscriber motion reciprocal native Toon/login differs":e);
    if(!logged)return true;
    Motion value;MotionOwner owner;owner.toon=toon;uintptr_t shard=0,last=0,render=0,callback=0;
    if(!world(owner,e)||!at(toon,0x170,shard,e)||!at(toon,0x178,owner.region,e)||!at(toon,0x9a8,last,e)||!at(owner.region,0x834,value.region,e))return false;
    if(shard!=owner.root||last!=owner.region)return fail(e,"Subscriber motion requires reciprocal registered root-shard Toon");
    uintptr_t region=0;if(!region_reader(owner,value.region,owner.region,region,e)||!at(toon,0x30,render,e)||!at(render,0x38,callback,e))return false;
    if(callback!=image+0x3e82a0)return fail(e,"Subscriber motion native Toon transform callback differs");
    if(!memory.copy(toon+0x100,value.orientation.data(),sizeof value.orientation,e)||!memory.copy(toon+0x110,value.position.data(),sizeof value.position,e)||!memory.copy(toon+0x11c,value.scale.data(),sizeof value.scale,e))return false;
    if(!subscriber_motion::valid(value,e)){
        const auto original=e;int32_t subscriber_id=-1;uint64_t toon_uid=0;std::string id_error,uid_error;
        const bool id_read=at(subscriber,0x20,subscriber_id,id_error);const bool uid_read=at(toon,0x18,toon_uid,uid_error);
        double norm_squared=0;std::array<uint32_t,4> bits{};for(size_t i=0;i<value.orientation.size();++i){const auto component=value.orientation[i];norm_squared+=double(component)*component;bits[i]=std::bit_cast<uint32_t>(component);}
        nlohmann::json detail={{"subscriber_id_read",id_read},{"toon_uid_read",uid_read},{"orientation",value.orientation},{"orientation_bits",bits},{"norm_squared",norm_squared},{"region",value.region}};
        if(id_read)detail["subscriber_id"]=subscriber_id;else detail["subscriber_id_read_error"]=id_error;
        if(uid_read)detail["toon_uid"]=toon_uid;else detail["toon_uid_read_error"]=uid_error;
        return fail(e,original+" native_motion_sample="+detail.dump());
    }
    if(!member(owner,e))return false;
    pose=value;if(selected)*selected=owner;return true;
}
template<class Memory,class Read>
bool motion_manager(Memory& memory,Read& at,uintptr_t image,uintptr_t region,uintptr_t& manager,std::vector<uintptr_t>& toons,std::string& e){
    uintptr_t data=0;int32_t count=-1,capacity=-1;uint8_t updating=1;
    if(!at(region,0x9c0,manager,e)||!type(at,manager,image,0x16fde30,e)||!at(manager,0xc8,updating,e)||!at(manager,0x28,data,e)||!at(manager,0x30,count,e)||!at(manager,0x34,capacity,e))return false;
    if(updating||count<0||capacity<count||count>1000000||(count&&(!data||data>UINTPTR_MAX-size_t(count)*sizeof(uintptr_t))))return fail(e,"Subscriber motion ToonManager is updating or array bounds differ");
    toons.clear();toons.resize(size_t(count));
    if(count&&!memory.copy(data,toons.data(),toons.size()*sizeof(uintptr_t),e)){toons.clear();return false;}
    std::set<uintptr_t> unique;
    for(auto toon:toons)if(!toon||!unique.insert(toon).second)return fail(e,"Subscriber motion ToonManager null/duplicate receiver");
    return true;
}
template<class Read,class MotionReader,class Output>
bool census(Read& at,uintptr_t image,MotionReader&& sample_motion,Output& out,std::string& e){
    out.rows.clear();out.subscribers.clear();out.subscriber_slots.clear();out.needs_owners.clear();
    int32_t count=-1,capacity=-1;
    if(!at(image,0x13df690,out.subscriber_manager,e)||!type(at,out.subscriber_manager,image,0x1725f40,e)||
       !at(out.subscriber_manager,0x40,out.subscriber_data,e)||!at(out.subscriber_manager,0x48,count,e)||
       !at(out.subscriber_manager,0x4c,capacity,e)||count<0||capacity<count||count>1000000||
       (count&&(!out.subscriber_data||out.subscriber_data>UINTPTR_MAX-size_t(count)*8)))
        return fail(e,e.empty()?"economic motion actual Subscriber manager bounds differ":e);
    if(!at(out.subscriber_manager,0x68,out.next_id,e))return false;
    if(out.next_id<0)return fail(e,"economic motion native nextID invalid");
    struct Entry {typename Output::Row row;uintptr_t owner;size_t slot;subscriber_needs::Owner needs;};
    std::vector<Entry> entries;
    for(int32_t i=0;i<count;++i){
        Entry entry{};entry.slot=size_t(i);
        if(!at(out.subscriber_data,entry.slot*8,entry.owner,e)||!type(at,entry.owner,image,0x16e75f0,e)||
           !at(entry.owner,0x20,entry.row.serialized_id,e))return false;
        if(entry.row.serialized_id<0||entry.row.serialized_id>=out.next_id)return fail(e,"economic motion Subscriber ID outside nextID");
        subscriber_needs::Bits bits;
        if(!subscriber_needs::sample(at,
            image,entry.owner,entry.row.serialized_id,entry.needs,bits,e)||
           !sample_motion(entry.owner,entry.row.motion,e))return false;
        entries.push_back(std::move(entry));
    }
    std::sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){return a.row.serialized_id<b.row.serialized_id;});
    int32_t previous=-1;
    for(auto& entry:entries){
        if(entry.row.serialized_id<=previous)return fail(e,"economic motion canonical Subscriber ID duplicate");
        previous=entry.row.serialized_id;out.rows.push_back(std::move(entry.row));out.subscribers.push_back(entry.owner);
        out.subscriber_slots.push_back(entry.slot);out.needs_owners.push_back(entry.needs);
    }
    return true;
}
}
#endif
