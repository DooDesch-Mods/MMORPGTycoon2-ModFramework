#pragma once
#include <nlohmann/json.hpp>
#include "subscriber_passive_motion.h"
#include "subscriber_activity.h"
#include "npc_census.h"
#include "npc_quest_records.h"
#include "developer_staff/passive_read.h"
#include "tutorial_passive_read.h"
#include <bit>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace mt2fw::mt2::determinism {
// Reads only. Every observed owner, field and ordered record is re-read at finish.
class WorkWitness {
public:
    using Json=nlohmann::json;
    using Bytes=std::vector<uint8_t>;
    using Read=std::function<Bytes(uintptr_t,size_t)>;
private:
    static constexpr const char* fixture_dispatch_sha="3b4f0014ac86678f30e78d944b0d6f2fed9524fe95f083e1fb768355c1fb0fc5";
    inline static const std::array<std::array<std::array<uint32_t,3>,2>,2> fixture_paths{{
        {{{1172510975u,1114772835u,1175535523u},{1172646251u,1114772229u,1175603672u}}},
        {{{1172510975u,1114772835u,1175535523u},{1172628840u,1114617560u,1175442126u}}}
    }};
    std::map<uintptr_t,std::string> dispatch_keys;
    struct Sample{uintptr_t address;Bytes bytes;};
    Read memory;uintptr_t image,game,state=0;
    std::map<uintptr_t,std::string> building_keys;
    std::vector<Sample> samples;
    std::map<std::string,uintptr_t> identities;
    std::set<std::string> missing;
    static void need(bool condition,const std::string& cause){if(!condition)throw std::runtime_error("Determinism work witness: "+cause);}
    Bytes bytes(uintptr_t p,size_t n){
        need(p&&n&&p<=UINTPTR_MAX-n,"native read extent invalid");
        auto value=memory(p,n);need(value.size()==n,"short native read address="+std::to_string(p)+" required="+std::to_string(n)+" actual="+std::to_string(value.size()));
        samples.push_back({p,value});return value;
    }
    template<class T>T at(uintptr_t p){auto value=bytes(p,sizeof(T));T out;std::memcpy(&out,value.data(),sizeof out);return out;}
    static uintptr_t offset(uintptr_t p,size_t n){need(p&&p<=UINTPTR_MAX-n,"owner address overflow");return p+n;}
    uintptr_t ptr(uintptr_t p,size_t n=0){return at<uintptr_t>(offset(p,n));}
    uint64_t code(uintptr_t p){need(p>=image&&p-image<0x2000000,"callback/address point outside pinned image");return p-image;}
    void type(uintptr_t p,uintptr_t ap,const char* name){need(ptr(p)==image+ap,std::string(name)+" address point differs");}
    std::string known(const std::string& key,uintptr_t receiver){
        auto [item,inserted]=identities.emplace(key,receiver);
        need(inserted||item->second==receiver,"canonical receiver key is ambiguous: "+key);return key;
    }
    Json unresolved(uint64_t ap,uint64_t callback,const char* why){
        missing.insert(why);return {{"resolved",false},{"owner",nullptr},{"class_rva",ap},{"callback_rva",callback},{"cause",why}};
    }
public:
    WorkWitness(Read read,uintptr_t base,uintptr_t owner,std::map<uintptr_t,std::string> saved_buildings={}):memory(std::move(read)),image(base),game(owner),building_keys(std::move(saved_buildings)){
        need(image&&image<=UINTPTR_MAX-0x2000000,"image range invalid");
        need(ptr(image,0x13df8f0)==game,"current Game owner differs");type(game,0x1743740,"Game");
        state=ptr(image,0x13df2a0);type(state,0x16de920,"GameState");
    }
    Json clock(){
        const auto owner=ptr(image,0x13df9a0);need(ptr(state,0x158)==owner,"Clock current GameState owner differs");type(owner,0x17499d0,"Clock");
        Json out;
        const std::array<const char*,6> names{"year","month","day","flat_day","seconds","saved_days_per_month"};
        for(size_t i=0;i<names.size();++i)out[names[i]]=at<int32_t>(offset(owner,0x38+i*4));
        const auto flag=at<uint8_t>(offset(owner,0x50));need(flag<=1,"Clock flag_50 is not bool");out["flag_50"]=bool(flag);
        out["subsecond_integer"]=at<int32_t>(offset(owner,0x54));
        const std::array<const char*,4> float_names{"scale_bits","pause_scale_bits","scaled_delta_bits","field_64_bits"};
        for(size_t i=0;i<float_names.size();++i)out[float_names[i]]=at<uint32_t>(offset(owner,0x58+i*4));
        const auto panel=ptr(owner,0x68);out["panel_present"]=panel!=0;
        return {{"comparable",out},{"local_evidence",{{"clock",owner},{"panel",panel}}},{"complete_clock_graph",false}};
    }
    Json receiver(uintptr_t receiver,uintptr_t ap,uintptr_t callback,bool virtual_dispatch=true){
        need(ptr(receiver)==ap,"dispatch receiver address point changed");
        if(virtual_dispatch)need(ptr(ap,0x10)==callback,"dispatch vtable callback differs");
        const auto ap_rva=code(ap),callback_rva=code(callback);
        if(dispatch_keys.contains(receiver)){
            const bool quest=ap_rva==0x174acf0,path=ap_rva==0x1744268;
            need((quest&&callback_rva==0x4d9260)||(path&&callback_rva==0x43e7d0),"Fixture dispatch class/callback differs");
            const auto adjustment=quest?0x28U:0x210U;need(receiver>=adjustment,"Fixture dispatch base adjustment invalid");
            type(receiver-adjustment,quest?0x174ac10:0x1743f10,quest?"Quest":"Path");
            return {{"resolved",true},{"owner",known(dispatch_keys.at(receiver),receiver)},{"class_rva",ap_rva},{"callback_rva",callback_rva}};
        }
        const auto software=ptr(image,0x13df500);
        if(receiver==offset(software,0x18)){
            type(software,0x1715450,"Software");
            return {{"resolved",true},{"owner",known("software/ticker",receiver)},{"class_rva",ap_rva},{"callback_rva",callback_rva}};
        }
        if(ap_rva==0x1705a60){
            if(virtual_dispatch)need(callback_rva==0x975b0,"CharacterType native dispatch callback differs");
            need(receiver>0x20,"CharacterType base adjustment invalid");const auto full=receiver-0x20;type(full,0x1705990,"CharacterType");
            const auto kind=at<int32_t>(offset(full,0x2c)),id=at<int32_t>(offset(full,0x30));
            need(kind>=0&&kind<5&&id>=0,"CharacterType key invalid");
            const auto manager=ptr(state,0x168);need(manager==ptr(image,0x13df7a0),"CharacterType manager singleton differs");type(manager,0x1732430,"CharacterType manager");
            const auto header=offset(manager,0x80+size_t(kind)*0x50),data=ptr(header,8);
            const auto count=at<int32_t>(offset(header,0x10)),capacity=at<int32_t>(offset(header,0x14));
            need(count>=0&&count<=100000&&capacity>=count&&id<count&&data,"CharacterType manager extent/key differs");
            need(ptr(data,size_t(id)*8)==full,"CharacterType current manager slot differs");
            const auto destroyed=at<uint8_t>(offset(full,0x490));need(destroyed<=1,"CharacterType destroyed flag is not bool");
            return {{"resolved",true},{"owner",known("character_type/"+std::to_string(kind)+"/"+std::to_string(id)+"/ticker",receiver)},
                {"class_rva",ap_rva},{"callback_rva",callback_rva},{"destroyed",bool(destroyed)}};
        }
        if(ap_rva==0x171bf20){
            if(virtual_dispatch)need(callback_rva==0x4f5350,"ScenarioManager native dispatch callback differs");
            need(receiver>0x38,"ScenarioManager base adjustment invalid");const auto full=receiver-0x38;
            need(ptr(state,0x1f0)==full,"ScenarioManager current GameState owner differs");type(full,0x171be40,"ScenarioManager");
            return {{"resolved",true},{"owner",known("scenario_manager/ticker",receiver)},{"class_rva",ap_rva},{"callback_rva",callback_rva}};
        }
        if(ap_rva==0x16d9780||ap_rva==0x16d9700){
            if(virtual_dispatch)need(callback_rva==(ap_rva==0x16d9780?0x40b820U:0x40bb00U),"Building native dispatch callback differs");
            const auto adjustment=ap_rva==0x16d9780?0x220U:0x30U;
            need(receiver>=adjustment,"Building base adjustment invalid");const auto full=receiver-adjustment;type(full,0x16d93c0,"Building");
            if(!building_keys.contains(full))return unresolved(ap_rva,callback_rva,"Building receiver lacks fixture-proven saved key");
            const auto key=building_keys.at(full)+(ap_rva==0x16d9780?"/ticker":"/updater");
            return {{"resolved",true},{"owner",known(key,receiver)},{"class_rva",ap_rva},{"callback_rva",callback_rva}};
        }
        return unresolved(ap_rva,callback_rva,"Dispatch receiver lacks a proven canonical owner route");
    }
    Json updates(){
        const auto manager=ptr(image,0x13df4a0);need(ptr(game,0x70)==manager,"UpdateManager game owner differs");type(manager,0x1709650,"UpdateManager");
        const auto paused=at<uint8_t>(offset(manager,0x68));need(paused<=1,"UpdateManager pause flag invalid");
        Json out={{"paused",bool(paused)}},local;
        const std::array<const char*,4> names{"entities","pending_entities","callbacks","pending_callbacks"};
        for(size_t list=0;list<4;++list){
            const auto header=offset(manager,0x10+list*0x18),data=ptr(header);
            const auto count=at<int32_t>(offset(header,8)),capacity=at<int32_t>(offset(header,12));
            need(count>=0&&count<=100000&&capacity>=count&&(!count||data),std::string("UpdateManager ")+names[list]+" extent invalid");
            const size_t stride=list<2?8:16;const auto raw=count?bytes(data,size_t(count)*stride):Bytes{};
            Json rows=Json::array(),addresses=Json::array();
            for(int32_t i=0;i<count;++i){
                uintptr_t object=0,callback=0;std::memcpy(&object,raw.data()+size_t(i)*stride,8);
                const auto ap=ptr(object);
                if(stride==8)callback=ptr(ap,0x10);else std::memcpy(&callback,raw.data()+size_t(i)*stride+8,8);
                rows.push_back(receiver(object,ap,callback,stride==8));addresses.push_back(object);
            }
            out[names[list]]=rows;local[names[list]]={{"data",data},{"capacity",capacity},{"receivers",addresses}};
        }
        return {{"comparable",out},{"local_evidence",local},{"complete_owner_state",false}};
    }
    Json tick_clock(unsigned slot){
        need(slot<4,"TickClock slot invalid");
        const auto ticks=ptr(image,0x13df350);need(ptr(game,0x150)==ticks,"TickManager game owner differs");type(ticks,0x16fdc40,"TickManager");
        const auto clock=ptr(ticks,8+size_t(slot)*8),data=ptr(clock,8);
        const auto count=at<int32_t>(offset(clock,0x10)),capacity=at<int32_t>(offset(clock,0x14));
        need(count>=0&&count<=100000&&capacity>=count&&(!count||data),"TickClock extent invalid");
        const auto raw=count?bytes(data,size_t(count)*8):Bytes{};
        Json callbacks=Json::array(),members=Json::array();
        for(int32_t i=0;i<count;++i){uintptr_t item=0;std::memcpy(&item,raw.data()+size_t(i)*8,8);
            const auto ap=ptr(item),callback=ptr(ap,0x10);callbacks.push_back(receiver(item,ap,callback));members.push_back(item);}
        Json comparable={{"count",count},{"callbacks",callbacks},
            {"period_bits",at<uint32_t>(offset(clock,0x18))},{"budget_bits",at<uint32_t>(offset(clock,0x1c))},
            {"rate_bits",at<uint32_t>(offset(clock,0x20))},{"next_index",at<int32_t>(offset(clock,0x24))}};
        return {{"comparable",comparable},{"local_evidence",{{"clock",clock},{"data",data},{"capacity",capacity},{"members",members}}}};
    }
    // Keys must come from the existing fixture load receipt, never runtime UIDs.
    Json fixture_building_poses(size_t expected){
        need(expected&&building_keys.size()==expected,"fixture Building receipt count differs");
        Json rows=Json::object(),local=Json::object();
        for(const auto& [object,key]:building_keys){
            need(!key.empty()&&!rows.contains(key),"fixture Building key is empty/ambiguous");type(object,0x16d93c0,"Building");
            const auto pose=at<std::array<uint32_t,10>>(offset(object,0x100));
            for(const auto value:pose)need((value&0x7f800000U)!=0x7f800000U,"Building pose is not finite");
            rows[key]=pose;local[key]=object;
        }
        return {{"comparable",rows},{"local_evidence",local},{"complete_building_graph",false}};
    }
    // Raw fields used by tutorial_development_read.cpp::project. No native getters.
    Json development_project(){
        const auto lounge=ptr(image,0x13df290),software=ptr(image,0x13df500);
        need(ptr(state,0x180)==lounge,"DevLounge current GameState owner differs");type(lounge,0x16de6c0,"DevLounge");
        need(ptr(state,0x1c8)==software,"Software current GameState owner differs");type(software,0x1715450,"Software");
        auto finite_bits=[&](uintptr_t p){const auto value=at<uint32_t>(p);need((value&0x7f800000U)!=0x7f800000U,"development scalar is not finite");return value;};
        const auto project=ptr(lounge,0xa8);
        Json out={{"project_present",project!=0},{"kind",nullptr},{"project_key",nullptr},
            {"work_bits",finite_bits(offset(lounge,0xb0))},{"lines",at<int32_t>(offset(lounge,0xb4))},
            {"bugs_total",at<int32_t>(offset(lounge,0xb8))},{"bugs_known",at<int32_t>(offset(lounge,0xbc))},
            {"desired_workers",at<int32_t>(offset(lounge,0xc4))},
            {"major_version",at<int32_t>(offset(software,0x88))},
            {"rating_bits",finite_bits(offset(software,0x314))},{"fun_bits",finite_bits(offset(software,0x318))}};
        if(project){
            type(project,0x16e48c0,"DevProject");const auto kind=at<int32_t>(offset(project,0x18));need(kind>=0&&kind<=3,"DevProject kind outside 0..3");
            const auto data=ptr(project,0x20);const auto size=at<uint64_t>(offset(project,0x28));need(size<=200,"DevProject key exceeds 200 bytes");
            const auto raw=bytes(data,size_t(size)+1);need(raw.back()==0,"DevProject key terminator differs");
            std::string key;for(size_t i=0;i<size;++i){need(raw[i]>=32&&raw[i]!=127,"DevProject key contains control byte");key.push_back(char(raw[i]));}
            static_cast<void>(Json(key).dump());out["kind"]=kind;out["project_key"]=key;
        }
        return {{"comparable",out},{"local_evidence",{{"lounge",lounge},{"software",software},{"project",project}}},{"complete_software_graph",false}};
    }
    // Pause drains running/waiting tasks, but not PumpFinishedTasks callbacks.
    Json finished_tasks(){
        const auto tasks=ptr(image,0x13df340);need(ptr(game,0x68)==tasks,"TaskManager game owner differs");type(tasks,0x16fdaf0,"TaskManager");
        const auto data=ptr(tasks,0xb8);const auto count=at<int32_t>(offset(tasks,0xc0));
        need(count>=0&&count<=100000&&(!count||data),"finished Task queue extent invalid");
        missing.insert("finished Task callback owners/state are not captured under their native mutex");
        return {{"comparable",{{"finished_count",count}}},{"local_evidence",{{"manager",tasks},{"finished_data",data}}},
            {"complete_callback_state",false},{"empty_observed",count==0}};
    }

    Json developers(bool before_first_mode_update=false){
        namespace staff=mt2fw::game::developer_staff;
        struct Port final:staff::passive::Memory {
            std::function<bool(uintptr_t,void*,size_t,std::string&)> read;
            bool copy(uintptr_t p,void* out,size_t n,std::string& e)override{return read(p,out,n,e);}
        } port;
        port.read=[&](uintptr_t p,void* out,size_t n,std::string&){const auto value=bytes(p,n);std::memcpy(out,value.data(),n);return true;};
        staff::State fields;staff::Local local;std::string error;
        const auto employment=before_first_mode_update?staff::passive::EmploymentObservation::Raw:staff::passive::EmploymentObservation::RequirePrefix;
        need(staff::passive::observe(port,state,image,fields,local,error,employment),error);
        Json rows=Json::array();
        for(const auto& row:fields.rows){std::array<uint32_t,3> position;
            for(size_t i=0;i<3;++i)position[i]=std::bit_cast<uint32_t>(row.position[i]);
            rows.push_back({{"id",row.id},{"slot",row.slot},{"state",row.state},{"employed",row.employed},
                {"active",row.active},{"on_map",row.on_map},{"job_link",row.job_link},{"position_bits",position}});
        }
        missing.insert("Developer job targets, paths and full transient actor state are not captured");
        return {{"comparable",{{"business_count",fields.business_count},{"lounge_count",fields.lounge_count},
            {"indoor_count",local.inside.size()},{"outdoor_count",local.outside.size()},{"developers",rows}}},
            {"local_evidence",{{"lounge",local.lounge},{"business",local.business},{"catalogue",local.data},
                {"receivers",local.developers},{"inside",local.inside},{"outside",local.outside}}}};
    }
    Json tutorial(){
        struct Port final:mt2fw::game::tutorial_observer::passive::Memory {
            std::function<bool(uintptr_t,void*,size_t,std::string&)> read;
            bool copy(uintptr_t p,void* out,size_t n,std::string& e)override{return read(p,out,n,e);}
        } port;
        port.read=[&](uintptr_t p,void* out,size_t n,std::string&){const auto value=bytes(p,n);std::memcpy(out,value.data(),n);return true;};
        Json fields;std::string error;
        need(mt2fw::game::tutorial_observer::passive::observe(port,image,state,fields,error),error);
        missing.insert("Tutorial history contents, inactive step payloads and non-tutorial scenario state are not captured");
        return {{"comparable",fields},{"local_evidence",{{"state",state}}}};
    }

    Json subscribers(){
        namespace passive=mt2fw::game::subscriber_passive;
        namespace region_identity=mt2fw::game::region_saved_identity;
        struct Memory {
            std::function<Bytes(uintptr_t,size_t)> read;
            bool copy(uintptr_t p,void* out,size_t n,std::string&){auto value=read(p,n);std::memcpy(out,value.data(),n);return true;}
        } port{[&](uintptr_t p,size_t n){return bytes(p,n);}};
        auto read=[&](uintptr_t p,size_t n,auto& value,std::string&){value=at<std::remove_reference_t<decltype(value)>>(offset(p,n));return true;};
        const auto world=ptr(state,0x160);need(world==ptr(image,0x13df9c0),"Subscriber current World singleton differs");type(world,0x174aef0,"World");
        const auto map=ptr(world,0x158);need(map==ptr(image,0x13df8c0),"Subscriber current Map singleton differs");type(map,0x1742860,"Map");
        const auto root=offset(map,0x1c0);region_identity::Index regions;std::string error;
        need(region_identity::capture(image,map,root,regions,read,error),error);
        std::map<uintptr_t,std::pair<uintptr_t,std::vector<uintptr_t>>> managers;
        auto world_reader=[&](passive::MotionOwner& owner,std::string&){owner.map=map;owner.root=root;return true;};
        auto region_reader=[&](const passive::MotionOwner&,int32_t id,uintptr_t expected,uintptr_t& selected,std::string& e){
            selected=0;for(const auto& [saved,pointer]:regions.saved)if(saved==id)selected=pointer;
            if(!selected||selected!=expected){e="Subscriber current saved Region ID differs";return false;}
            if(at<uint8_t>(offset(selected,0x850))!=1){e="Subscriber motion target Region is not natively owned";return false;}return true;
        };
        auto member=[&](passive::MotionOwner& owner,std::string& e){
            auto found=managers.find(owner.region);
            if(found==managers.end()){std::pair<uintptr_t,std::vector<uintptr_t>> value;
                if(!passive::motion_manager(port,read,image,owner.region,value.first,value.second,e))return false;
                found=managers.emplace(owner.region,std::move(value)).first;
            }
            owner.manager=found->second.first;
            if(std::count(found->second.second.begin(),found->second.second.end(),owner.toon)!=1){e="Subscriber motion Toon absent from registered Region manager";return false;}return true;
        };
        struct Census {
            struct Row {int32_t serialized_id=-1;std::optional<passive::Motion> motion;};
            uintptr_t subscriber_manager=0,subscriber_data=0;int32_t next_id=0;
            std::vector<Row> rows;std::vector<uintptr_t> subscribers;std::vector<size_t> subscriber_slots;
            std::vector<mt2fw::game::subscriber_needs::Owner> needs_owners;
        } census;
        auto motion=[&](uintptr_t subscriber,std::optional<passive::Motion>& pose,std::string& e){
            return passive::motion(port,read,image,subscriber,world_reader,region_reader,member,pose,e);
        };
        need(passive::census(read,image,motion,census,error),error);
        Json rows=Json::array();
        for(size_t i=0;i<census.rows.size();++i){const auto& row=census.rows[i];
            mt2fw::game::subscriber_activity::State activity;
            need(mt2fw::game::subscriber_activity::sample(read,image,census.subscribers[i],row.serialized_id,activity,error),error);
            Json pose=nullptr;if(row.motion){std::array<uint32_t,10> bits{};size_t slot=0;
                for(float value:row.motion->orientation)bits[slot++]=std::bit_cast<uint32_t>(value);
                for(float value:row.motion->position)bits[slot++]=std::bit_cast<uint32_t>(value);
                for(float value:row.motion->scale)bits[slot++]=std::bit_cast<uint32_t>(value);
                pose={{"region",row.motion->region},{"pose_bits",bits}};
            }
            rows.push_back({{"saved_id",row.serialized_id},{"registered_motion",pose},{"satisfaction",activity.satisfaction},
                {"addiction",activity.addiction},{"logged_in_time_bits",activity.logged_in_time_bits}});
        }
        need(region_identity::verify(image,map,root,regions,read,error),error);
        missing.insert("Subscriber AI, destinations, combat/quest state and offline Toon state are not fully captured");
        return {{"comparable",{{"next_saved_id",census.next_id},{"rows",rows}}},
            {"local_evidence",{{"manager",census.subscriber_manager},{"data",census.subscriber_data},{"subscribers",census.subscribers},{"slots",census.subscriber_slots}}}};
    }

    Json npcs(const std::map<uintptr_t,std::string>& npc_identities){
        namespace census=mt2fw::npc_census;
        need(npc_identities.size()==2,"NPC witness requires two pinned saved load identities");
        struct Port: census::Memory {
            std::function<Bytes(uintptr_t,size_t)> read;
            bool copy(uintptr_t p,void* out,size_t n,std::string&)override{auto b=read(p,n);std::memcpy(out,b.data(),n);return true;}
        } port;port.read=[&](uintptr_t p,size_t n){return bytes(p,n);};
        const auto world=ptr(state,0x160);type(world,0x174aef0,"World");need(world==ptr(image,0x13df9c0),"NPC World singleton differs");
        const auto map=ptr(world,0x158);type(map,0x1742860,"Map");need(map==ptr(image,0x13df8c0),"NPC Map singleton differs");
        auto read=[&](uintptr_t p,size_t n,auto& value,std::string&){value=at<std::remove_reference_t<decltype(value)>>(offset(p,n));return true;};
        mt2fw::game::region_saved_identity::Index index;std::string error;
        need(mt2fw::game::region_saved_identity::capture(image,map,offset(map,0x1c0),index,read,error),error);
        uintptr_t region=0;for(const auto& [id,p]:index.saved)if(id==67)region=p;need(region!=0,"NPC saved Region 67 absent");
        census::State sampled;need(census::read(port,image,region,sampled,error),error);need(sampled.rows.size()==2,"NPC fixture current census count differs");
        std::map<std::string,Json> sorted;Json local=Json::array();
        for(const auto& row:sampled.rows){auto found=npc_identities.find(row.local_pointer);need(found!=npc_identities.end(),"NPC current member lacks saved load receipt");
            std::array<uint32_t,10> pose{};size_t slot=0;
            for(float f:row.rotation){need(std::isfinite(f),"NPC quaternion not finite");pose[slot++]=std::bit_cast<uint32_t>(f);}
            for(float f:row.position){need(std::isfinite(f),"NPC position not finite");pose[slot++]=std::bit_cast<uint32_t>(f);}
            for(float f:row.scale){need(std::isfinite(f)&&f>0,"NPC scale invalid");pose[slot++]=std::bit_cast<uint32_t>(f);}
            need(sorted.emplace(found->second,Json{{"saved_key",found->second},{"region",67},{"type",row.type},{"character_type",row.character_type},
                {"level",row.level},{"quest_count",row.quest_count},{"name",row.name},{"pose_bits",pose}}).second,"NPC saved key duplicates");
            local.push_back({{"saved_key",found->second},{"pointer",row.local_pointer},{"uid",row.local_uid},{"tag",row.local_tag},{"character_slot",row.character_slot}});
        }
        Json rows=Json::array();for(const auto& [key,row]:sorted){(void)key;rows.push_back(row);}
        missing.insert("NPC AI, destinations, quest content and combat state are not fully captured");
        return {{"comparable",rows},{"local_evidence",{{"manager",sampled.manager},{"rows",local},{"next_local_tag",sampled.next_local_tag}}}};
    }

    Json bind_fixture_dispatch(const std::map<uintptr_t,std::string>& npc_keys){
        namespace quests=mt2fw::npc_quest_records;
        need(dispatch_keys.empty()&&npc_keys.size()==2,"Fixture dispatch binding requires two saved NPC owners and one fresh binding");
        struct Port:mt2fw::npc_census::Memory {
            std::function<Bytes(uintptr_t,size_t)> read;
            bool copy(uintptr_t p,void* out,size_t n,std::string&)override{auto b=read(p,n);std::memcpy(out,b.data(),n);return true;}
        } port;port.read=[&](uintptr_t p,size_t n){return bytes(p,n);};
        auto array=[&](uintptr_t owner,size_t off,size_t bound){
            const auto data=ptr(owner,off);const auto count=at<int32_t>(offset(owner,off+8)),cap=at<int32_t>(offset(owner,off+12));
            need(count>=0&&size_t(count)<=bound&&cap>=count&&(!count||data),"Fixture dispatch array extent invalid");
            std::vector<uintptr_t> result;std::set<uintptr_t> seen;
            for(int32_t i=0;i<count;++i){auto p=ptr(data,size_t(i)*8);need(p&&seen.insert(p).second,"Fixture dispatch array member duplicate/null");result.push_back(p);}return result;
        };
        std::set<std::string> keys;std::set<uintptr_t> quest_pointers;
        const auto quest_manager=ptr(image,0x13df410);std::string error;
        for(const auto& [npc,key]:npc_keys){
            need(key==std::string(fixture_dispatch_sha)+":world:npc:34"||key==std::string(fixture_dispatch_sha)+":world:npc:39","Fixture dispatch NPC key differs");
            type(npc,0x1742980,"NPC");need(at<int32_t>(offset(npc,0x80c))==0,"Fixture dispatch NPC is not QuestGiver");
            const auto proxy=ptr(npc,0x28);need(ptr(proxy)==offset(npc,0x20),"Quest NPC weak owner differs");
            const auto members=array(npc,0x868,100000);need(members.size()==2,"Fixture requires two persisted Quests per NPC");
            std::set<int32_t> sequences;
            for(auto quest:members){type(quest,0x174ac10,"Quest");need(quest_pointers.insert(quest).second,"Quest belongs to multiple saved NPCs");
                const auto sequence=at<int32_t>(offset(quest,0x84));need(sequence>=0&&sequence<2&&sequences.insert(sequence).second,"Quest persisted sequence ID invalid/duplicate");
                need(ptr(quest,0xb8)==npc&&ptr(quest,0xc0)==proxy&&ptr(proxy)==offset(npc,0x20),"Quest reciprocal weak owner differs");
                need(quests::manager_member(port,image,quest_manager,quest,error),error);
                const auto owner_key=key+"/quest/sequence{"+std::to_string(sequence)+"}/ticker";
                need(keys.insert(owner_key).second&&dispatch_keys.emplace(offset(quest,0x28),owner_key).second,"Quest canonical dispatch key repeats");
            }
        }
        const auto world=ptr(state,0x160);type(world,0x174aef0,"World");need(world==ptr(image,0x13df9c0),"Path World singleton differs");
        const auto map=ptr(world,0x158);type(map,0x1742860,"Map");need(map==ptr(image,0x13df8c0),"Path Map singleton differs");
        auto read=[&](uintptr_t p,size_t n,auto& value,std::string&){value=at<std::remove_reference_t<decltype(value)>>(offset(p,n));return true;};
        mt2fw::game::region_saved_identity::Index regions;
        need(mt2fw::game::region_saved_identity::capture(image,map,offset(map,0x1c0),regions,read,error),error);
        uintptr_t region=0;for(const auto& [id,p]:regions.saved)if(id==67)region=p;need(region!=0,"Path saved Region absent");
        const auto manager=ptr(region,0x9a0);need(ptr(manager,0x6a0)==region,"Path manager reciprocal Region differs");
        std::vector<uintptr_t> paths,nodes;
        for(size_t kind=0;kind<11;++kind){auto rows=array(manager,0x28+kind*0x30,1000000);
            need(rows.size()==(kind==0?2U:0U),"Fixture Path type/count changed");if(kind==0)paths=rows;}
        for(size_t kind=0;kind<6;++kind){auto rows=array(manager,0x238+kind*0x30,1000000);
            need(rows.size()==(kind==0?3U:0U),"Fixture Intersection type/count changed");if(kind==0)nodes=rows;}
        std::set<uintptr_t> node_set(nodes.begin(),nodes.end());std::map<uintptr_t,std::set<uintptr_t>> expected_edges;
        std::map<size_t,uintptr_t> saved_nodes;std::map<uintptr_t,size_t> node_ids;
        std::set<size_t> matched;
        for(auto path:paths){type(path,0x1743f10,"Path");
            need(at<int32_t>(offset(path,0x21c))==0&&at<uint8_t>(offset(path,0x46e))==0,"Path type/preview differs");
            need(ptr(path,0x170)==offset(map,0x1c0)&&ptr(path,0x178)==region&&ptr(path,0x2f8)==region,"Path reciprocal saved Region differs");
            const auto start=at<std::array<uint32_t,3>>(offset(path,0x278)),end=at<std::array<uint32_t,3>>(offset(path,0x284));
            size_t match=fixture_paths.size();
            for(size_t i=0;i<fixture_paths.size();++i)if(start==fixture_paths[i][0]&&end==fixture_paths[i][1]){need(match==fixture_paths.size(),"Immutable Path geometry ambiguous");match=i;}
            need(match<fixture_paths.size()&&matched.insert(match).second,"Path geometry missing/changed/duplicate");
            size_t endpoint_index=0;
            for(size_t off:{size_t(0x268),size_t(0x270)}){const auto node=ptr(path,off);need(node_set.contains(node),"Path endpoint absent from owned Intersection bucket");type(node,0x1700a10,"Intersection");
                need(at<int32_t>(offset(node,0x298))==0&&ptr(node,0x170)==offset(map,0x1c0)&&ptr(node,0x178)==region,"Intersection reciprocal owner/type differs");
                const size_t saved_id=endpoint_index?42+match:41;
                const auto [saved_it,saved_new]=saved_nodes.emplace(saved_id,node);
                need(saved_new||saved_it->second==node,"Pinned Intersection shared endpoint differs");
                const auto [node_it,node_new]=node_ids.emplace(node,saved_id);
                need(node_new||node_it->second==saved_id,"Pinned Intersection saved IDs alias one node");
                ++endpoint_index;expected_edges[node].insert(path);
            }
            std::string geometric=std::string(fixture_dispatch_sha)+":world:path:region{67}/Road/start{";
            for(auto word:start)geometric+=std::to_string(word)+",";geometric+="}/end{";for(auto word:end)geometric+=std::to_string(word)+",";geometric+="}/ticker";
            need(keys.insert(geometric).second&&dispatch_keys.emplace(offset(path,0x210),geometric).second,"Path canonical dispatch key repeats");
        }
        for(auto node:nodes){const auto edges=array(node,0x2c8,65536);need(edges.size()==expected_edges[node].size()&&std::set<uintptr_t>(edges.begin(),edges.end())==expected_edges[node],"Intersection edges do not reciprocate pinned Paths");}
        need(saved_nodes.size()==3&&node_ids.size()==node_set.size(),"Pinned Intersection graph coverage differs");
        Json comparable=Json::array(),local=Json::array();
        for(const auto& [id,node]:saved_nodes){
            const auto actual=at<std::array<uint32_t,3>>(offset(node,0x110));
            const auto& expected=id==41?fixture_paths[0][0]:fixture_paths[id-42][1];
            std::array<double,3> delta{};
            for(size_t axis=0;axis<3;++axis)delta[axis]=double(std::bit_cast<float>(actual[axis]))-double(std::bit_cast<float>(expected[axis]));
            const auto key=std::string(fixture_dispatch_sha)+":world:intersection:"+std::to_string(id);
            comparable.push_back({{"owner",key},{"position_bits",actual}});
            local.push_back({{"saved_intersection",id},{"node",node},{"position_offset",0x110},
                {"actual_bits",actual},{"expected_bits",expected},{"persisted_position_matches",actual==expected},{"position_delta",delta}});
        }
        need(dispatch_keys.size()==6,"Fixture dispatch coverage differs");
        return {{"comparable",comparable},{"local_evidence",local}};
    }

    void finish(){
        for(const auto& sample:samples){const auto current=memory(sample.address,sample.bytes.size());
            need(current==sample.bytes,"observed owner/field/ordered roster changed or short read at address="+std::to_string(sample.address));}
    }
    Json gaps()const{return Json(missing);}
};
}
