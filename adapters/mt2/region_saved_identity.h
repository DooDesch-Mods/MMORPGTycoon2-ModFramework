#pragma once
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace mt2mp::game::region_saved_identity {
struct Index {
    uintptr_t data=0,owned=0;
    int32_t count=0,capacity=0,owned_count=0,owned_capacity=0;
    std::vector<std::pair<int32_t,uintptr_t>> saved;
    std::vector<uintptr_t> owned_members;
    bool operator==(const Index&)const=default;
};
namespace detail {
template<class Read>
bool scan(uintptr_t image,uintptr_t map,uintptr_t root,int32_t requested,uintptr_t expected,
             uintptr_t& selected,Read&& read,std::string& error,bool require_target,Index* complete) {
    selected=0;if(complete)*complete={};Index next;
    const auto reject=[&](const std::string& cause){error=cause;return false;};
    if((require_target&&requested<0)||!map||map>std::numeric_limits<uintptr_t>::max()-0x364||
       root!=map+0x1c0)return reject("saved region Map or root shard differs");
    uintptr_t map_ap=0,root_ap=0,saved_ap=0,data=0,owned=0;
    int32_t root_type=-1,count=-1,capacity=-1,owned_count=-1,owned_capacity=-1;
    if(!read(map,0,map_ap,error)||!read(root,0,root_ap,error)||!read(root,0x48,root_type,error)||
       !read(map+0x230,0,saved_ap,error)||!read(map+0x230,8,data,error)||
       !read(map+0x230,0x10,count,error)||!read(map+0x230,0x14,capacity,error)||
       !read(map,0x358,owned,error)||!read(map,0x360,owned_count,error)||
       !read(map,0x364,owned_capacity,error))return false;
    if(map_ap!=image+0x1742860||root_ap!=image+0x174ad20||root_type!=0||
       saved_ap!=image+0x16f04e0)return reject("saved region Map, root or array address point differs");
    if(count<1||count>4096||capacity<count||!data||owned_count<0||owned_count>count||
       owned_capacity<owned_count||(owned_count&&!owned))
        return reject("saved region or owned subset array bounds differ");
    std::set<int32_t> ids;std::set<uintptr_t> saved;
    for(int32_t index=0;index<count;++index){
        uintptr_t region=0,ap=0,shard=0;int32_t id=-1;
        if(!read(data,size_t(index)*8,region,error)||!read(region,0,ap,error)||
           !read(region,0x70,shard,error)||!read(region,0x834,id,error))return false;
        if(!region||ap!=image+0x1787930||shard!=root||id<0||
           !saved.insert(region).second||!ids.insert(id).second)
            return reject("saved region address point, shard, ID or pointer differs index="+std::to_string(index));
        if(id==requested)selected=region;
        if(complete)next.saved.emplace_back(id,region);
    }
    std::set<uintptr_t> owned_unique;
    for(int32_t index=0;index<owned_count;++index){
        uintptr_t region=0,ap=0,shard=0;
        if(!read(owned,size_t(index)*8,region,error)||!read(region,0,ap,error)||
           !read(region,0x70,shard,error))return false;
        if(!region||ap!=image+0x1787930||shard!=root||!owned_unique.insert(region).second||
           !saved.contains(region))
            return reject("owned region is duplicate or absent from saved Map.region index="+std::to_string(index));
        if(complete)next.owned_members.push_back(region);
    }
    uintptr_t current_data=0,current_owned=0;int32_t current_count=-1,current_owned_count=-1;
    if(!read(map+0x230,8,current_data,error)||!read(map+0x230,0x10,current_count,error)||
       !read(map,0x358,current_owned,error)||!read(map,0x360,current_owned_count,error))return false;
    if(current_data!=data||current_count!=count||current_owned!=owned||current_owned_count!=owned_count)
        return reject("saved region arrays changed during identity scan");
    if(require_target&&(!selected||(expected&&selected!=expected)))
        return reject("region ID is absent or maps to another native receiver");
    if(complete){next.data=data;next.owned=owned;next.count=count;next.capacity=capacity;
        next.owned_count=owned_count;next.owned_capacity=owned_capacity;*complete=std::move(next);}
    return true;
}
}
template<class Read>
bool resolve(uintptr_t image,uintptr_t map,uintptr_t root,int32_t requested,uintptr_t expected,
             uintptr_t& selected,Read&& read,std::string& error){
    return detail::scan(image,map,root,requested,expected,selected,read,error,true,nullptr);
}
// This index belongs to one synchronous traversal. Revalidate it before publishing
// results; no caller retains it across an original effect or another frame.
template<class Read>
bool capture(uintptr_t image,uintptr_t map,uintptr_t root,Index& out,Read&& read,std::string& error){
    uintptr_t ignored=0;return detail::scan(image,map,root,0,0,ignored,read,error,false,&out);
}
template<class Read>
bool verify(uintptr_t image,uintptr_t map,uintptr_t root,const Index& expected,Read&& read,std::string& error){
    Index current;if(!capture(image,map,root,current,read,error))return false;
    if(current!=expected){error="saved region complete index changed during pure traversal";return false;}
    return true;
}
}
