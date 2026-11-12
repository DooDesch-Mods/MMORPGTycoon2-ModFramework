#ifndef MT2FW_DEVELOPER_STAFF_PASSIVE_READ_H
#define MT2FW_DEVELOPER_STAFF_PASSIVE_READ_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
namespace mt2fw::game::developer_staff {
struct Row {
    int32_t id=-1,slot=-1,state=0;
    bool employed=false,active=false,on_map=false,job_link=false;
    std::array<float,3> position{};
    bool operator==(const Row&)const=default;
};
struct State {
    int32_t business_count=0,lounge_count=0;
    std::vector<Row> rows;
    bool operator==(const State&)const=default;
};
struct Local {
    uintptr_t lounge=0,business=0,data=0,indoor=0,outdoor=0;
    int32_t capacity=0;
    std::vector<uintptr_t> developers,inside,outside;
    bool operator==(const Local&)const=default;
};
namespace passive {
struct Memory {
    virtual ~Memory()=default;
    virtual bool copy(uintptr_t,void*,size_t,std::string&)=0;
};
inline bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool at(Memory& p,uintptr_t address,size_t offset,T& value,std::string& e){
    if(!address||address>UINTPTR_MAX-offset||address+offset>UINTPTR_MAX-sizeof(T))return fail(e,"Developer read address range invalid");
    return p.copy(address+offset,&value,sizeof(T),e);
}
inline bool type(Memory& p,uintptr_t address,uintptr_t expected,std::string& e){uintptr_t actual=0;return at(p,address,0,actual,e)&&(actual==expected||fail(e,"Developer owned class addresspoint differs"));}
inline bool array(Memory& p,uintptr_t owner,size_t offset,std::vector<uintptr_t>& rows,int32_t& capacity,std::string& e){
    uintptr_t data=0;int32_t count=0;
    if(!at(p,owner,offset,data,e)||!at(p,owner,offset+8,count,e)||!at(p,owner,offset+12,capacity,e))return false;
    if(count<0||count>50||capacity<count||capacity>4096||(capacity&&!data))return fail(e,"Developer owned array bounds differ");
    rows.clear();std::set<uintptr_t> unique;
    for(int32_t i=0;i<count;++i){uintptr_t value=0;if(!at(p,data,size_t(i)*8,value,e))return false;if(!value||!unique.insert(value).second)return fail(e,"Developer owned array contains null/duplicate receiver");rows.push_back(value);}
    return true;
}
// Raw keeps loaded employment flags before Mode.Update applies the lounge count.
enum class EmploymentObservation { RequirePrefix, Raw };
inline bool observe(Memory& p,uintptr_t state,uintptr_t image,State& out,Local& local,std::string& e,EmploymentObservation employment=EmploymentObservation::RequirePrefix){
    out={};local={};uintptr_t singleton=0;
    if(!at(p,state,0x180,local.lounge,e)||!at(p,state,0x1c0,local.business,e)||
       !type(p,local.lounge,image+0x16de6c0,e)||!type(p,local.business,image+0x16d97e0,e)||
       !at(p,image,0x13df290,singleton,e))return false;
    if(singleton!=local.lounge)return fail(e,"Developer actual lounge singleton/GameState differs");
    if(!at(p,local.business,0x4c,out.business_count,e)||!at(p,local.lounge,0x310,out.lounge_count,e)||
       !at(p,local.lounge,0x70,local.data,e)||!array(p,local.lounge,0x70,local.developers,local.capacity,e)||
       !at(p,local.lounge,0x98,local.indoor,e)||!at(p,local.lounge,0xa0,local.outdoor,e))return false;
    if(out.business_count<0||out.business_count>50||out.lounge_count<0||out.lounge_count>50||local.developers.size()!=50||local.indoor==local.outdoor)
        return fail(e,"Developer fixed pool/count/manager owners differ");
    int32_t capacity=0;
    if(!type(p,local.indoor,image+0x16e47f0,e)||!type(p,local.outdoor,image+0x16e47f0,e)||
       !array(p,local.indoor,0x38,local.inside,capacity,e)||!array(p,local.outdoor,0x38,local.outside,capacity,e))return false;
    std::set<uintptr_t> members(local.inside.begin(),local.inside.end());
    for(auto value:local.outside)if(!members.insert(value).second)return fail(e,"Developer appears in both native managers");
    if(members!=std::set<uintptr_t>(local.developers.begin(),local.developers.end()))return fail(e,"Developer manager union differs from actual50-entry catalogue");
    std::set<int32_t> ids;int32_t slot=0;
    for(auto address:local.developers){Row row;uint8_t employed=0,active=0,on_map=0;uintptr_t job=0;row.slot=slot++;
        if(!type(p,address,image+0x17422e0,e)||!at(p,address,0x4c0,row.id,e)||!at(p,address,0x4c4,row.state,e)||
           !at(p,address,0x4d8,employed,e)||!at(p,address,0x4d9,active,e)||!at(p,address,0x368,on_map,e)||!at(p,address,0x4c8,job,e))return false;
        for(size_t i=0;i<3;++i){
            if(!at(p,address,0x110+i*4,row.position[i],e))return false;
            if(!std::isfinite(row.position[i]))return fail(e,"Developer native position is not finite");
        }
        const bool duplicate_id=ids.contains(row.id);
        if(row.id<0||row.id>49||!ids.insert(row.id).second||row.state<0||row.state>7||employed>1||active>1||on_map>1||(employment==EmploymentObservation::RequirePrefix&&bool(employed)!=(row.slot<out.lounge_count)))
            return fail(e,"Developer persisted ID/state/employment prefix differs: address="+std::to_string(address)+
                " slot="+std::to_string(row.slot)+" id="+std::to_string(row.id)+" duplicate_id="+std::to_string(duplicate_id)+
                " state="+std::to_string(row.state)+" employed="+std::to_string(employed)+" active="+std::to_string(active)+
                " on_map="+std::to_string(on_map)+" lounge_count="+std::to_string(out.lounge_count)+
                " business_count="+std::to_string(out.business_count)+" expected_employed="+std::to_string(row.slot<out.lounge_count));
        if(bool(on_map)!=(std::find(local.outside.begin(),local.outside.end(),address)!=local.outside.end()))return fail(e,"Developer map flag and actual manager membership differ");
        row.employed=employed!=0;row.active=active!=0;row.on_map=on_map!=0;row.job_link=job!=0;out.rows.push_back(row);
    }
    return true;
}
}
}
#endif
