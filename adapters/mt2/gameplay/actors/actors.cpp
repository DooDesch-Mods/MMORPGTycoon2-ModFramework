// Copyright (c) 2026 DooDesch. Original MIT standalone native adapter.
// ABI evidence: Multiplayer 8a13602 src/game/npc_census.h,
// npc_property_runtime.inc, npc_quest_records.h and boss_phase_*.
#include "actors.h"
#include "model.h"
#include "../../internal/mt2mp/game/native_engine.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace mt2fw::mt2::gameplay::actors {
namespace {
using model::need;
struct Text {const char* data; size_t size; std::array<uint8_t,16> storage{};};
uint32_t boundary(void(*fn)(void*),void* value) noexcept {
    __try {fn(value);return 0;} __except(EXCEPTION_EXECUTE_HANDLER) {return GetExceptionCode();}
}
template<class F> void native(F&& fn) {
    struct Call {std::remove_reference_t<F>* fn;std::exception_ptr error;};Call call{&fn,{}};
    auto invoke=[](void* p){auto& c=*static_cast<Call*>(p);try{(*c.fn)();}catch(...){c.error=std::current_exception();}};
    auto fault=boundary(invoke,&call);need(!fault,"actor native call failed SEH="+std::to_string(fault));if(call.error)std::rethrow_exception(call.error);
}
template<class R,class... A> R call(Context& c,uintptr_t rva,A... args) {c.verify_original(rva);return reinterpret_cast<R(*)(A...)>(c.image+rva)(args...);}
template<class T> void put(uintptr_t p,size_t offset,const T& value){std::memcpy(reinterpret_cast<void*>(p+offset),&value,sizeof value);}
void type(Context& c,uintptr_t p,uintptr_t expected){need(p && c.read<uintptr_t>(p)==c.image+expected,"actor native class differs");}
std::vector<uintptr_t> array(Context& c,uintptr_t object,size_t maximum=100000) {
    auto data=c.read<uintptr_t>(object+8);auto count=c.read<int32_t>(object+16),capacity=c.read<int32_t>(object+20);
    need(count>=0 && size_t(count)<=maximum && capacity>=count && (!count || data),"actor native array bounds differ");
    std::vector<uintptr_t> rows;std::set<uintptr_t> unique;
    for(int32_t i=0;i<count;++i){auto p=c.read<uintptr_t>(data+size_t(i)*8);need(p && unique.insert(p).second,"actor native array has null/duplicate membership");rows.push_back(p);}
    need(c.read<uintptr_t>(object+8)==data && c.read<int32_t>(object+16)==count && c.read<int32_t>(object+20)==capacity,"actor native array changed during read");return rows;
}
Json handle(Context& c,uintptr_t object){return {{"epoch",c.epoch},{"uid",std::to_string(c.read<uint64_t>(object+0x18))}};}
uintptr_t map(Context& c){c.require_world();auto p=c.singleton(0x13df8c0);type(c,p,0x1742860);return p;}
std::vector<uintptr_t> regions(Context& c){auto world=map(c);auto values=array(c,world+0x230);for(auto p:values){type(c,p,0x1787930);need(c.read<uintptr_t>(p+0x70)==world+0x1c0,"actor root Region owner differs");}return values;}
uintptr_t region(Context& c,int32_t id){for(auto p:regions(c))if(c.read<int32_t>(p+0x834)==id)return p;throw std::runtime_error("actor root Region saved ID is absent");}
struct Actor {uintptr_t object;std::string kind;int32_t region;};
std::vector<Actor> census(Context&,const Json&);
struct Zone {uintptr_t object;int32_t region;};
std::vector<Zone> zone_census(Context& c,const Json& args,bool all_types=false) {
    auto saved=args.contains("region")?model::integer(args.at("region"),0):int32_t(-1);std::vector<Zone> out;
    for(auto r:regions(c)){auto id=c.read<int32_t>(r+0x834);if(saved>=0 && saved!=id)continue;auto manager=c.read<uintptr_t>(r+0x968);type(c,manager,0x16fe170);need(c.read<uintptr_t>(manager+0xf0)==r,"Zone manager Region owner differs");
        for(int32_t kind=0;kind<4;++kind)if(all_types || kind==1)for(auto p:array(c,manager+0x18+size_t(kind)*0x30,4096)){type(c,p,0x1745ce0);need(c.read<int32_t>(p+0x24c)==kind && c.read<uintptr_t>(p+0x178)==r && c.read<uintptr_t>(p+0x170)==map(c)+0x1c0,"Zone kind/Region/Shard ownership differs");out.push_back({p,id});}}
    if(saved>=0)region(c,saved);return out;
}
struct Point {int64_t x,z;};
struct VectorView {uintptr_t begin,end,capacity;};
size_t vector_count(const VectorView& view,size_t stride,size_t maximum) {need((!view.begin && !view.end && !view.capacity) || (view.begin && view.end>=view.begin && view.capacity>=view.end && (view.end-view.begin)%stride==0 && (view.capacity-view.begin)%stride==0),"Zone native vector extent differs");auto count=(view.end-view.begin)/stride;need(count<=maximum,"Zone native vector exceeds its bound");return count;}
Json polygon(Context& c,uintptr_t shape) {
    type(c,shape,0x16e0140);auto data=c.read<uintptr_t>(shape+0x18);need(data,"Zone native polygon storage is absent");auto paths=c.read<VectorView>(data);auto count=vector_count(paths,24,128);Json out=Json::array();size_t remaining=8192;
    for(size_t i=0;i<count;++i){auto points=c.read<VectorView>(paths.begin+i*24);auto size=vector_count(points,16,remaining);remaining-=size;Json path=Json::array();for(size_t j=0;j<size;++j){auto point=c.read<Point>(points.begin+j*16);path.push_back({{"x",point.x},{"z",point.z}});}out.push_back(path);}need(c.read<VectorView>(data).begin==paths.begin && c.read<VectorView>(data).end==paths.end,"Zone polygon changed during read");return out;
}
Json zone_json(Context& c,const Zone& zone) {
    auto p=zone.object;auto id=handle(c,p);need(c.resolve(id,0x1745ce0)==p,"Zone exact UID registry differs");Json members=Json::array();auto actors=census(c,{{"region",zone.region}});for(auto monster:array(c,p+0x380,4096)){auto it=std::find_if(actors.begin(),actors.end(),[&](const Actor& row){return row.kind=="monster" && row.object==monster;});need(it!=actors.end() && c.read<uintptr_t>(monster+0x8d0)==p,"Zone generated Monster manager membership differs");auto proxy=c.read<uintptr_t>(monster+0x8d8);need(proxy && c.read<uintptr_t>(p+0x28)==proxy && c.read<uintptr_t>(proxy)==p+0x20,"Zone generated Monster reciprocal weak link differs");members.push_back(handle(c,monster));}
    return {{"handle",id},{"region",zone.region},{"name",c.text(p+0x38,4096)},{"type",c.read<int32_t>(p+0x24c)},{"density",c.read<int32_t>(p+0x250)},{"level",c.read<int32_t>(p+0x360)},{"character_index",c.read<int32_t>(p+0x560)},{"constructed_day",c.read<int32_t>(p+0x56c)},{"shape",polygon(c,p+0x2f0)},{"monsters",members}};
}
std::vector<Actor> census(Context& c,const Json& args) {
    auto saved=args.contains("region")?model::integer(args.at("region"),0):int32_t(-1);std::vector<Actor> out;
    for(auto r:regions(c)) {
        auto id=c.read<int32_t>(r+0x834);if(saved>=0 && saved!=id)continue;
        auto npc=c.read<uintptr_t>(r+0x998);need(npc && c.read<uintptr_t>(npc+0xc8)==r,"NPC manager Region owner differs");
        for(size_t kind=0;kind<3;++kind)for(auto p:array(c,npc+0x38+kind*0x30)) {
            type(c,p,0x1742980);need(c.read<uintptr_t>(p+0x178)==r && c.read<int32_t>(p+0x80c)==int32_t(kind),"NPC manager kind/Region differs");out.push_back({p,"npc",id});
        }
        auto monster=c.read<uintptr_t>(r+0x988);type(c,monster,0x1714100);need(c.read<uintptr_t>(monster+0x80)==r,"Monster manager Region owner differs");
        for(auto p:array(c,monster+0x38)){type(c,p,0x16c0770);need(c.read<uintptr_t>(p+0x178)==r,"Monster Region differs");out.push_back({p,"monster",id});}
        auto boss=c.read<uintptr_t>(r+0x990);type(c,boss,0x16fa780);need(c.read<uintptr_t>(boss+0x68)==r,"Boss manager Region owner differs");
        for(auto p:array(c,boss+0x38)){type(c,p,0x1743040);need(c.read<uintptr_t>(p+0x178)==r,"Boss Region differs");out.push_back({p,"boss",id});}
    }
    if(saved>=0)region(c,saved);return out;
}
Actor selected(Context& c,const Json& h) {
    auto p=c.resolve(h);for(const auto& row:census(c,Json::object()))if(row.object==p)return row;
    throw std::runtime_error("actor handle is absent from the current root Region managers");
}
Json actor(Context& c,const Actor& row) {
    auto p=row.object;auto rotation=c.read<std::array<float,4>>(p+0x100);auto position=c.read<std::array<float,3>>(p+0x110);auto scale=c.read<std::array<float,3>>(p+0x11c);
    for(float v:rotation)need(std::isfinite(v),"actor rotation is non-finite");for(float v:position)need(std::isfinite(v),"actor position is non-finite");for(float v:scale)need(std::isfinite(v),"actor scale is non-finite");
    auto h=handle(c,p);need(c.resolve(h)==p,"actor UID registry differs from its manager");
    Json out={{"handle",h},{"kind",row.kind},{"region",row.region},{"name",c.text(p+0x38,256)},{"position",position},{"rotation",rotation},{"scale",scale},
              {"character_kind",c.read<int32_t>(p+0x4a8)},{"character_index",c.read<int32_t>(p+0x4a4)},{"level",c.read<int32_t>(p+0x6f8)}};
    if(row.kind=="npc") {
        auto mode=c.read<int32_t>(p+0x838);auto forced=c.read<uint8_t>(p+0x6c0);need(mode>=0 && mode<=2 && forced<=1 && bool(forced)==(mode!=2),"NPC online mode/tint differs");
        out["npc_type"]=c.read<int32_t>(p+0x80c);out["online"]=mode;out["force_tint"]=bool(forced);out["tint"]=c.read<std::array<float,4>>(p+0x6b0);out["command_level"]=c.read<int32_t>(p+0x890);
    }
    return out;
}
uintptr_t character(Context& c,const Json& h) {
    auto p=c.resolve(h,0x1705990);auto kind=model::integer(h.at("kind"),0,4),index=model::integer(h.at("index"),0);
    need(c.read<int32_t>(p+0x2c)==kind && c.read<int32_t>(p+0x30)==index && !c.read<uint8_t>(p+0x490),"phase owner definition kind/index/lifetime differs");
    auto manager=c.singleton(0x13df7a0);type(c,manager,0x1732430);auto types=array(c,manager+0x80+size_t(kind)*0x50);
    need(size_t(index)<types.size() && types[size_t(index)]==p,"phase owner is absent from the current definition manager");return p;
}
Json phases(Context& c,uintptr_t owner) {
    auto prototypes=array(c,owner+0x128,10000);auto members=array(c,owner+0x158,10000);Json rows=Json::array();
    for(size_t i=0;i<members.size();++i){auto p=members[i];type(c,p,0x16ddb00);need(c.read<int32_t>(p+0x5c)==int32_t(i),"phase native ordinal differs");Json abilities=Json::array();
        for(auto element:array(c,p+0x90,10000)){type(c,element,0x173a3a0);auto prototype=c.read<uintptr_t>(element+0x18);auto it=std::find(prototypes.begin(),prototypes.end(),prototype);need(it!=prototypes.end(),"phase ability is outside its character owner");abilities.push_back(size_t(it-prototypes.begin()));}
        auto color=c.read<std::array<float,4>>(p+0x80);for(float v:color)need(std::isfinite(v),"phase color is non-finite");
        rows.push_back({{"name",c.text(p+0x38,4096)},{"health",c.read<int32_t>(p+0x58)},{"vfx",c.text(p+0x60,4096)},{"color",color},{"abilities",abilities}});
    }return rows;
}
void event(Context& c,uintptr_t phase,bool post) {
    alignas(16) std::array<unsigned char,0x100> storage{};call<void>(c,post?0xd7f00:0xd7e40,storage.data(),reinterpret_cast<void*>(phase));
    call<void>(c,0xd9200,storage.data());put(uintptr_t(storage.data()),0xd0,c.image+0x174cca0);call<void>(c,0x89da30,storage.data()+0xd0);call<void>(c,0x11bd1e0,storage.data()+8);
}
Json quest(Context& c,uintptr_t npc,uintptr_t q,size_t slot) {
    type(c,q,0x174ac10);need(c.read<uintptr_t>(q+0xb8)==npc,"quest NPC owner differs");auto owner_proxy=c.read<uintptr_t>(q+0xc0),npc_proxy=c.read<uintptr_t>(npc+0x28);
    need(owner_proxy && owner_proxy==npc_proxy && c.read<uintptr_t>(owner_proxy)==npc+0x20,"quest NPC weak ownership differs");auto destination=c.read<uintptr_t>(q+0xf0),proxy=c.read<uintptr_t>(q+0xf8);
    need(destination && proxy && c.read<uintptr_t>(destination+0x28)==proxy && c.read<uintptr_t>(proxy)==destination+0x20,"quest destination weak binding differs");
    auto sequence=c.read<int32_t>(q+0x84);need(sequence>=0 && c.read<uint64_t>(destination+0x18),"quest sequence/destination identity is invalid");
    return {{"handle",{{"epoch",c.epoch},{"owner",handle(c,npc)},{"slot",slot},{"sequence",sequence}}},{"name",c.text(q+0x38,4096)},{"description",c.text(q+0x58,65536)},
        {"type",c.read<int32_t>(q+0x34)},{"action",c.read<int32_t>(q+0x78)},{"monsters_to_kill",c.read<int32_t>(q+0x7c)},{"monster_type",c.read<int32_t>(q+0x80)},
        {"xp_reward",c.read<int32_t>(q+0x88)},{"gold_reward",c.read<int32_t>(q+0x8c)},{"destination",handle(c,destination)},
        {"accepted_today",c.read<int32_t>(q+0x150)},{"accepted_total",c.read<int32_t>(q+0x154)},{"completed_today",c.read<int32_t>(q+0x1c0)},{"completed_total",c.read<int32_t>(q+0x1c4)}};
}
struct Quest {Actor owner;uintptr_t pointer;size_t slot;};
Quest selected_quest(Context& c,const Json& h) {
    need(h.at("epoch").get<uint64_t>()==c.epoch,"quest handle world epoch expired");auto owner=selected(c,h.at("owner"));need(owner.kind=="npc" && c.read<int32_t>(owner.object+0x80c)==0,"quest requires a current QuestGiver");
    auto slot=model::integer(h.at("slot"),0),sequence=model::integer(h.at("sequence"),0);auto members=array(c,owner.object+0x860,5);
    need(size_t(slot)<members.size(),"quest slot is absent");auto pointer=members[size_t(slot)];type(c,pointer,0x174ac10);need(c.read<int32_t>(pointer+0x84)==sequence,"quest slot sequence changed");quest(c,owner.object,pointer,size_t(slot));return {owner,pointer,size_t(slot)};
}
Json owner_quests(Context& c,uintptr_t npc) {Json out=Json::array();auto list=array(c,npc+0x860,5);for(size_t i=0;i<list.size();++i)out.push_back(quest(c,npc,list[i],i));return out;}
struct QuestCensus {uintptr_t manager;int32_t completed;std::vector<uintptr_t> members;Json rows;};
QuestCensus quest_census(Context& c) {
    QuestCensus out;out.manager=c.singleton(0x13df410);type(c,out.manager,0x17020b0);out.completed=c.read<int32_t>(out.manager+0x48);out.members=array(c,out.manager+0x28,4096);need(out.completed>=0 && size_t(out.completed)<=out.members.size(),"global quest completed count differs");out.rows=Json::array();
    for(auto q:out.members){type(c,q,0x174ac10);auto owner=c.read<uintptr_t>(q+0xb8);type(c,owner,0x1742980);need(c.resolve(handle(c,owner),0x1742980)==owner,"global quest owner UID differs");auto list=array(c,owner+0x860,5);auto it=std::find(list.begin(),list.end(),q);need(it!=list.end(),"global quest is absent from its owner's list");out.rows.push_back(quest(c,owner,q,size_t(it-list.begin())));}return out;
}
struct Undo {uintptr_t manager;std::vector<uintptr_t> undo,redo;};
Undo undo(Context& c) {need(!c.read<uintptr_t>(c.image+0x1a3aa80),"quest edit requires an idle native Undo graph");auto manager=c.singleton(0x13df380);type(c,manager,0x16fe070);type(c,manager+8,0x16e1320);type(c,manager+0x20,0x16e1320);return {manager,array(c,manager+8,20),array(c,manager+0x20,20)};}
Json ledger() {
    mt2mp::game::native::SimulationState value;std::string error;need(mt2mp::game::native::read_simulation_state(value,error),"actor ledger read failed: "+error);
    return {{"cash",value.cash},{"day",value.clock.flat_day},{"present",value.record_present},{"record_day",value.record_day},{"record_cash",value.record_cash},{"income",value.record_income},{"expense",value.record_expense},{"line_counts",value.line_counts},{"line_income",value.line_income}};
}
void quest_inverse(Context& c,const Undo& before,const Undo& after,uintptr_t owner,const Json& prior) {
    need(before.manager==after.manager && !after.undo.empty() && after.redo.empty(),"quest Undo manager/redo/commit differs");auto graph=after.undo.back();need(std::find(before.undo.begin(),before.undo.end(),graph)==before.undo.end(),"quest Undo graph was not newly committed");auto expected=before.undo;expected.push_back(graph);if(expected.size()>20)expected.erase(expected.begin());need(expected==after.undo,"quest Undo history differs");
    type(c,graph,0x16e32d0);type(c,graph+0x18,0x16e2dc0);type(c,graph+0x48,0x1748c50);need(array(c,graph).empty() && array(c,graph+0x48).empty() && c.read<uint8_t>(graph+0x10c)==1,"quest Undo graph is not drained/locked");auto operations=array(c,graph+0x18,1);need(operations.size()==1,"quest Undo must contain one native inverse");auto inverse=operations[0];type(c,inverse,0x1713330);
    need(c.read<uint64_t>(inverse+0xb8)==c.read<uint64_t>(owner+0x18) && c.read<uint8_t>(inverse+0xc0)==0,"quest Undo inverse owner/generation differs");
    auto data=c.read<uintptr_t>(inverse+0xd0);auto count=c.read<int32_t>(inverse+0xd8),capacity=c.read<int32_t>(inverse+0xdc);need(count>=0 && capacity>=count && count<=5 && (!count || data),"quest Undo inverse record bounds differ");size_t index=0;
    for(const auto& record:prior){if(record.at("action")==8)continue;need(index<size_t(count),"quest Undo inverse omitted a record");auto p=data+index*0x58;
        need(c.read<int32_t>(p)==record.at("type").get<int32_t>() && std::to_string(c.read<uint64_t>(p+8))==record.at("destination").at("uid").get<std::string>() &&
            c.read<int32_t>(p+0x10)==record.at("action").get<int32_t>() && c.read<int32_t>(p+0x14)==record.at("monsters_to_kill").get<int32_t>() && c.text(p+0x18,4096)==record.at("name").get<std::string>() && c.text(p+0x38,4096)=="[Quest description goes here]","quest Undo inverse saved record differs");++index;
    }need(index==size_t(count),"quest Undo inverse contains extra records");
}
struct Destination {uintptr_t object;int32_t action,kills,monster_type;};
Json visitor_link(Context& c,uintptr_t p,uintptr_t ap,bool is_quest,const QuestCensus& quests,const std::set<uintptr_t>& npcs) {
    type(c,p,ap);auto target=c.read<uintptr_t>(p+8),proxy=c.read<uintptr_t>(p+16);
    if(!proxy){need(!target,"quest visitor target has no weak proxy");return {{"state",0},{"target",0}};}
    auto weak=c.read<uintptr_t>(proxy);if(!weak)return {{"state",1},{"target",0}};
    need(target && weak==target+(is_quest?0x18:0x20) && c.read<uintptr_t>(target+(is_quest?0x20:0x28))==proxy,"quest visitor reciprocal weak target differs");type(c,target,is_quest?0x174ac10:0x1742980);
    need(is_quest?std::find(quests.members.begin(),quests.members.end(),target)!=quests.members.end():npcs.contains(target),"quest visitor live target is outside current native membership");return {{"state",2},{"target",target}};
}
Json visitor_references(Context& c,const QuestCensus& quests) {
    auto manager=c.singleton(0x13df690);type(c,manager,0x1725f40);auto subscribers=array(c,manager+0x38,1000000);std::set<uintptr_t> npcs;for(const auto& row:census(c,Json::object()))if(row.kind=="npc")npcs.insert(row.object);Json rows=Json::array();std::set<int32_t> ids;
    for(auto subscriber:subscribers){type(c,subscriber,0x16e75f0);auto id=c.read<int32_t>(subscriber+0x20);need(id>=0 && ids.insert(id).second,"quest visitor Subscriber ID is invalid/duplicate");auto toon=c.read<uintptr_t>(subscriber+0x80);Json row={{"subscriber",subscriber},{"id",id},{"toon",toon},{"progress",Json::array()}};
        if(toon){type(c,toon,0x1744e90);need(c.read<uintptr_t>(toon+0x940)==subscriber,"quest visitor Toon backlink differs");type(c,toon+0xaf0,0x16ee0e0);row["gold"]=c.read<int32_t>(toon+0x850);row["xp_bits"]=c.read<uint32_t>(toon+0x820);row["level"]=c.read<int32_t>(toon+0x6f8);row["quests_completed"]=c.read<int32_t>(toon+0x874);
            for(auto progress:array(c,toon+0xaf0,100000)){type(c,progress,0x172d130);type(c,progress+0x58,0x17087e0);auto defeated=c.read<int32_t>(progress+0xa8),completed=c.read<int32_t>(progress+0x50);auto visited=c.read<uint8_t>(progress+0xac),action=c.read<uint8_t>(progress+0xad),complete=c.read<uint8_t>(progress+0xae);need(defeated>=0 && completed>=-1 && visited<=1 && action<=1 && complete<=1,"quest visitor counters/flags differ");
                row["progress"].push_back({{"pointer",progress},{"npc",visitor_link(c,progress+0x38,0x16f9de0,false,quests,npcs)},{"quest",visitor_link(c,progress+0x90,0x16f9f20,true,quests,npcs)},{"completed",completed},{"monsters_defeated",defeated},{"visited",bool(visited)},{"action_performed",bool(action)},{"complete",bool(complete)}});}}
        rows.push_back(row);
    }need(array(c,manager+0x38,1000000)==subscribers,"quest visitor Subscriber membership changed during read");return rows;
}
void verify_references(const Json& before,const Json& after,uintptr_t removed=0) {
    auto expected=before;need(expected.size()==after.size(),"quest edit changed Subscriber cohort");
    if(removed)for(size_t i=0;i<expected.size();++i){auto& progress=expected[i]["progress"];need(progress.size()==after[i].at("progress").size(),"quest removal changed active progress membership");for(size_t j=0;j<progress.size();++j)if(progress[j]["quest"]["state"]==2 && progress[j]["quest"]["target"]==removed){const auto& now=after[i]["progress"][j]["quest"];need(now.at("state")!=2 && now.at("target")==0,"quest removal retained a live visitor quest reference");progress[j]["quest"]=now;progress[j]["complete"]=false;}}
    need(expected==after,"quest edit changed active visitor links/counters/rewards unexpectedly");
}
void quest_undo(Context& c,uintptr_t owner,const std::string& title,bool destroy_inverse=false,std::optional<int32_t> refund={}) {
    // This is the native selector's Undo construction, with an explicit owner so
    // an NPC with no existing quest can receive its first quest without UI.
    alignas(16) std::array<unsigned char,0x38> label{};bool ready=false;std::string failure;
    Text text{title.data(),title.size()};
    try {native([&]{auto graph=call<uintptr_t>(c,0xa02670,size_t(0x110));need(graph!=0,"quest Undo graph allocation failed");call<void>(c,0x5400f0,reinterpret_cast<void*>(graph));
        if(refund){auto cash=call<uintptr_t>(c,0xa02670,size_t(0xc8));need(cash!=0,"NPC cash Undo allocation failed");call<void>(c,0x53ccb0,reinterpret_cast<void*>(cash),*refund,reinterpret_cast<void*>(owner+0x110));call<void>(c,0x540760,reinterpret_cast<void*>(graph),reinterpret_cast<void*>(cash));}
        auto inverse=call<uintptr_t>(c,0xa02670,size_t(destroy_inverse?0xc0:0xe0));need(inverse!=0,"actor Undo inverse allocation failed");if(destroy_inverse)call<void>(c,0x53dc30,reinterpret_cast<void*>(inverse),reinterpret_cast<void*>(owner));else call<void>(c,0x562da0,reinterpret_cast<void*>(inverse),reinterpret_cast<void*>(owner),false);call<void>(c,0x540760,reinterpret_cast<void*>(graph),reinterpret_cast<void*>(inverse));
        call<void>(c,0x538ae0,label.data(),&text,reinterpret_cast<void*>(owner));ready=true;call<void>(c,0x53b260,reinterpret_cast<void*>(c.singleton(0x13df380)),label.data(),reinterpret_cast<void*>(graph));});
    }catch(const std::exception& e){failure=e.what();if(!ready)failure+="; native label construction was not acknowledged, uncertain allocations were not disposed";}
    if(ready)for(auto item:{std::pair{uintptr_t(0x11b2b00),size_t(0x20)},std::pair{uintptr_t(0x11bd1e0),size_t(0)}})try{native([&]{call<void>(c,item.first,label.data()+item.second);});}catch(const std::exception& e){if(!failure.empty())failure+="; ";failure+="quest Undo label cleanup: "+std::string(e.what());}
    need(failure.empty(),failure);
}
Json edit_quest(Context& c,const std::string& operation,const Json& args,bool& attempted) {
    bool creating=operation=="quests.create";Quest selected{};
    if(creating){auto owner=actors::selected(c,args.at("handle"));need(owner.kind=="npc" && c.read<int32_t>(owner.object+0x80c)==0,"quest creation requires a current QuestGiver");selected.owner=owner;}
    else selected=selected_quest(c,args.at("handle"));
    auto owner=selected.owner.object;auto prior=owner_quests(c,owner);auto members=array(c,owner+0x860,5);auto before=quest_census(c);auto references=visitor_references(c,before);auto history=undo(c);auto cash=ledger();auto expected_members=members;auto expected_rows=prior;
    for(size_t i=0;i<prior.size();++i)need(prior[i].at("handle").at("sequence")==i,"quest native sequence is not its current owner slot");
    std::string title;int32_t kills=0,xp=0,gold=0,target=0;bool defaults=false;
    if(creating){need(members.size()<5 && before.members.size()<4096,"quest native owner/global capacity is reached");auto r=region(c,selected.owner.region);auto buildings=c.read<uintptr_t>(r+0x950);type(c,buildings,0x17183c0);need(c.read<uintptr_t>(buildings+0x458)==r && !array(c,buildings+0x408).empty(),"quest creation requires an original Building fallback destination");}
    else if(operation=="quests.remove"){expected_members.erase(expected_members.begin()+selected.slot);expected_rows.erase(expected_rows.begin()+selected.slot);}
    else if(operation=="quests.swap"){target=model::integer(args.at("slot"),0,int32_t(members.size())-1);need(size_t(target)!=selected.slot,"quest swap slot is unchanged");std::swap(expected_members[selected.slot],expected_members[size_t(target)]);std::swap(expected_rows[selected.slot],expected_rows[size_t(target)]);}
    else if(operation=="quests.set_name"){title=model::text(args.at("name"),4096);need(!title.empty(),"quest title cannot be empty");expected_rows[selected.slot]["name"]=title;}
    else if(operation=="quests.set_kills"){kills=model::integer(args.at("kills"),0,1000000);expected_rows[selected.slot]["monsters_to_kill"]=kills;}
    else {defaults=args.value("defaults",false);need(defaults || (args.contains("xp") && args.contains("gold")),"quest custom rewards require xp and gold");need(!defaults || (!args.contains("xp") && !args.contains("gold")),"quest default rewards cannot include custom rewards");xp=defaults?-1:model::integer(args.at("xp"),0);gold=defaults?-1:model::integer(args.at("gold"),0);expected_rows[selected.slot]["xp_reward"]=xp;expected_rows[selected.slot]["gold_reward"]=gold;}
    if(!creating && expected_rows==prior){return {{"quests",prior},{"undo_confirmed",false}};}
    for(auto rva:{0xa02670,0x5400f0,0x562da0,0x540760,0x538ae0,0x53b260,0x11b2b00,0x11bd1e0})c.verify_original(rva);
    auto entry=creating?0x3c70d0:operation=="quests.remove"?0x3c73d0:operation=="quests.swap"?0x3c6640:operation=="quests.set_name"?0x11bfce0:operation=="quests.set_kills"?0x78ed30:defaults?0x4d6cc0:0x4d6cd0;c.verify_original(entry);if(operation=="quests.set_rewards" && !defaults)c.verify_original(0x4d6ce0);
    if(operation=="quests.set_kills")c.verify_hash(0x78ed30,2160,"ec2fa4a4576e5c3d2de04d8034cac5fddc8005b0636556f444ca3dbc1db31819");
    attempted=true;quest_undo(c,owner,"Edit NPC Quests");uintptr_t created=0;
    native([&]{if(creating)created=call<uintptr_t>(c,0x3c70d0,reinterpret_cast<void*>(owner));
        else if(operation=="quests.remove")call<void>(c,0x3c73d0,reinterpret_cast<void*>(owner),reinterpret_cast<void*>(selected.pointer));
        else if(operation=="quests.swap")call<void>(c,0x3c6640,reinterpret_cast<void*>(owner),reinterpret_cast<void*>(selected.pointer),target);
        else if(operation=="quests.set_name"){Text text{title.data(),title.size()};call<void>(c,0x11bfce0,reinterpret_cast<void*>(selected.pointer+0x38),&text);}
        else if(operation=="quests.set_kills")put(selected.pointer,0x7c,kills);
        else if(defaults)call<void>(c,0x4d6cc0,reinterpret_cast<void*>(selected.pointer));
        else {call<void>(c,0x4d6cd0,reinterpret_cast<void*>(selected.pointer),xp);call<void>(c,0x4d6ce0,reinterpret_cast<void*>(selected.pointer),gold);}});
    auto after=quest_census(c);auto actual_members=array(c,owner+0x860,5);auto actual=owner_quests(c,owner);auto global_members=before.members;auto global_rows=before.rows;int32_t completed=before.completed;
    if(creating){need(created && actual_members.size()==members.size()+1 && actual_members.back()==created && std::equal(members.begin(),members.end(),actual_members.begin()),"quest creation native append/return differs");need(actual.back().at("type")==1 && actual.back().at("handle").at("sequence")==members.size() && actual.back().at("accepted_today")==0 && actual.back().at("accepted_total")==0 && actual.back().at("completed_today")==0 && actual.back().at("completed_total")==0,"quest creation native record defaults differ");auto task=c.read<uintptr_t>(created+0x218);type(c,task,0x1714ac0);expected_members.push_back(created);expected_rows.push_back(actual.back());global_members.push_back(created);global_rows.push_back(actual.back());}
    if(operation=="quests.remove"){auto it=std::find(global_members.begin(),global_members.end(),selected.pointer);need(it!=global_members.end(),"removed quest was absent from global manager");auto index=size_t(it-global_members.begin());global_members.erase(it);global_rows.erase(global_rows.begin()+index);if(prior[selected.slot].at("completed_total").get<int32_t>()>0)--completed;}
    for(size_t i=0;i<expected_rows.size();++i){expected_rows[i]["handle"]["slot"]=i;expected_rows[i]["handle"]["sequence"]=i;for(size_t j=0;j<global_members.size();++j)if(global_members[j]==expected_members[i])global_rows[j]=expected_rows[i];}
    need(actual_members==expected_members && actual==expected_rows,"quest complete owner record/order readback differs");need(after.manager==before.manager && after.members==global_members && after.rows==global_rows && after.completed==completed,"quest complete global record/order/completion readback differs");verify_references(references,visitor_references(c,after),operation=="quests.remove"?selected.pointer:0);quest_inverse(c,history,undo(c),owner,prior);need(cash==ledger(),"quest edit changed native ledger");return {{"quests",actual},{"undo_confirmed",true}};
}
Destination destination(Context& c,const Json& h,const Json& before,const Json& args) {
    auto object=c.resolve(h);auto r=c.read<uintptr_t>(object+0x178);auto id=c.read<int32_t>(r+0x834);need(region(c,id)==r && c.read<uintptr_t>(object+0x170)==map(c)+0x1c0,"quest destination is outside its current root Region");auto ap=c.read<uintptr_t>(object);int32_t kills=0,monster_type=0;bool member=false;
    if(ap==c.image+0x1742980 || ap==c.image+0x16c0770){auto actor=selected(c,h);member=actor.object==object;if(actor.kind=="monster"){need(c.read<int32_t>(object+0x838)==1,"direct quest Monster destination requires native Elite rank");kills=1;}else need(actor.kind=="npc","quest destination actor family differs");}
    else if(ap==c.image+0x1745ce0){auto manager=c.read<uintptr_t>(r+0x968);type(c,manager,0x16fe170);need(c.read<uintptr_t>(manager+0xf0)==r && c.read<int32_t>(object+0x24c)==1,"quest destination is not a current Monster Zone");auto values=array(c,manager+0x48);member=std::find(values.begin(),values.end(),object)!=values.end();monster_type=c.read<int32_t>(object+0x560);kills=args.contains("kills")?model::integer(args.at("kills"),1,1000000):before.at("monsters_to_kill").get<int32_t>();if(!kills)kills=8;}
    else {auto manager=c.read<uintptr_t>(r+0x950);type(c,manager,0x17183c0);need(c.read<uintptr_t>(manager+0x458)==r && c.read<uintptr_t>(manager+0x450)==map(c)+0x1c0,"quest Building manager ownership differs");auto values=array(c,manager+0x408);member=std::find(values.begin(),values.end(),object)!=values.end();}
    need(member,"quest destination is absent from its original family pool");auto getter=c.read<uintptr_t>(ap+0x2d0);need(getter==c.image+(ap==c.image+0x1745ce0?0xe3f5c0:0xdf1670),"quest destination Monster type virtual differs");int32_t action=-1;native([&]{action=call<int32_t>(c,0x4d5ea0,reinterpret_cast<void*>(object));});need(action>=0 && action<=7,"quest original default action is invalid");return {object,action,kills,monster_type};
}
Json set_destination(Context& c,const Json& args,bool& attempted) {
    auto selected=selected_quest(c,args.at("handle"));auto current=quest(c,selected.owner.object,selected.pointer,selected.slot);auto desired=destination(c,args.at("destination"),current,args);auto before=quest_census(c);auto prior=owner_quests(c,selected.owner.object);auto undo_before=undo(c);auto cash=ledger();
    need(current.at("destination")!=args.at("destination") || current.at("action")!=desired.action || current.at("monsters_to_kill")!=desired.kills,"quest destination/action is unchanged");
    c.verify_hash(0x4d8720,1264,"812c6152c99cded4f7fb560f71b4c3dfabcb5283810d267b514a460cd4f8d94d");attempted=true;
    native([&]{call<void>(c,0x4d8720,reinterpret_cast<void*>(selected.pointer),reinterpret_cast<void*>(desired.object),desired.action,desired.kills);});
    auto after=quest_census(c);need(before.manager==after.manager && before.members==after.members && before.completed==after.completed,"quest destination edit changed global membership/completion");
    auto expected=before.rows;bool found=false;
    for(size_t i=0;i<before.members.size();++i)if(before.members[i]==selected.pointer){auto changed=before.rows[i];changed["name"]=after.rows[i].at("name");changed["destination"]=handle(c,desired.object);changed["action"]=desired.action;changed["monsters_to_kill"]=desired.kills;changed["monster_type"]=desired.monster_type;need(!changed.at("name").get<std::string>().empty(),"quest native regenerated title is empty");expected[i]=changed;found=true;}
    need(found && expected==after.rows,"quest destination complete authored/global readback differs");auto task=c.read<uintptr_t>(selected.pointer+0x218);type(c,task,0x1714ac0);need(c.read<uint8_t>(task+0xe)<=1,"quest path recalculation task state differs");quest_inverse(c,undo_before,undo(c),selected.owner.object,prior);need(cash==ledger(),"quest destination edit changed the native ledger");
    return {{"quest",quest(c,selected.owner.object,selected.pointer,selected.slot)},{"undo_confirmed",true},{"path_task_present",true}};
}
Json place_boss(Context& c,const Json& args,bool& attempted) {
    auto definition=character(c,args.at("character"));need(c.read<int32_t>(definition+0x2c)==3,"Boss placement requires a current kind 3 definition");auto index=c.read<int32_t>(definition+0x30);auto saved=model::integer(args.at("region"),0);auto r=region(c,saved);auto manager=c.read<uintptr_t>(r+0x990);type(c,manager,0x16fa780);auto pose=model::pose(args.at("pose"));auto world=map(c);
    uintptr_t at=0;native([&]{at=call<uintptr_t>(c,0x32a990,reinterpret_cast<void*>(world),pose.position.data(),static_cast<void*>(nullptr));});need(at==r,"Boss placement point is outside the requested root Region");
    auto before=census(c,Json::object());Json rows=Json::array();for(const auto& value:before)rows.push_back(actor(c,value));auto old_bosses=array(c,manager+0x38);auto history=undo(c);auto cash=ledger();auto expected_cash=model::charge(cash,1000,6);need(cash.at("cash").get<int32_t>()>=1000,"Boss placement requires 1000 cash");auto tag=c.read<int32_t>(manager+0x74);need(tag>=0 && tag<=99,"Boss manager next tag differs");
    alignas(16) std::array<unsigned char,0x70> transform{};for(auto rva:{0x9ff370,0x399ac0,0x239410})c.verify_original(rva);c.verify_hash(0x399ac0,704,"553194b40c54cfba34829fdb6c29abe4aacbc53b766f02820854b24ad70c7c3d");c.verify_hash(0x239410,656,"5a71da708e0497f16724dcf421fe5a445b1ad614e1a70d6b86f528f7dee4f5c1");
    bool purchased=false;uintptr_t object=0;attempted=true;
    native([&]{call<void>(c,0x9ff370,transform.data(),pose.rotation.data(),pose.position.data(),pose.scale.data());purchased=call<bool>(c,0x399ac0,pose.position.data());need(purchased,"Boss original Purchase declined");need(call<uintptr_t>(c,0x32a990,reinterpret_cast<void*>(world),pose.position.data(),static_cast<void*>(nullptr))==r,"Boss original Region changed after Purchase");object=call<uintptr_t>(c,0x239410,reinterpret_cast<void*>(manager),index,transform.data());});
    need(object,"Boss original AddBoss returned null after payment; do not replay");auto current=selected(c,handle(c,object));need(current.kind=="boss" && current.region==saved,"Boss original birth is outside requested manager");auto bosses=array(c,manager+0x38);auto expected_bosses=old_bosses;expected_bosses.push_back(object);need(bosses==expected_bosses && c.read<int32_t>(object+0x9a8)==tag && c.read<int32_t>(manager+0x74)==(tag==99?0:tag+1),"Boss native birth membership/tag readback differs");
    auto after=census(c,Json::object());need(after.size()==before.size()+1,"Boss creation changed actor cohort size unexpectedly");size_t survivors=0;for(const auto& value:after)if(value.object!=object){need(survivors<before.size() && value.object==before[survivors].object && actor(c,value)==rows[survivors],"Boss creation changed an existing actor record/order");++survivors;}need(survivors==before.size(),"Boss creation omitted a surviving actor");
    auto result=actor(c,current);need(result.at("position")==Json(pose.position) && result.at("rotation")==Json(pose.rotation) && result.at("scale")==Json(pose.scale) && result.at("character_kind")==3 && result.at("character_index")==index && result.at("level")==c.read<int32_t>(r+0x838)+10,"Boss native definition/pose/level readback differs");auto now=undo(c);need(history.manager==now.manager && history.undo==now.undo && history.redo==now.redo,"Boss native placement changed Undo unexpectedly");need(ledger()==expected_cash,"Boss native purchase accounting differs");return result;
}
Json place_monster(Context& c,const Json& args,bool& attempted) {
    auto definition=character(c,args.at("character"));auto index=c.read<int32_t>(definition+0x30);auto saved=model::integer(args.at("region"),0),rank=model::integer(args.at("rank"),0,2),level=model::integer(args.at("level"),0);auto kind=rank==2?4:1;need(c.read<int32_t>(definition+0x2c)==kind,"Monster definition kind differs from its native rank");auto r=region(c,saved);need(c.read<uint8_t>(r+0x850)==1,"Monster placement requires an active native Region");need(level>=c.read<int32_t>(r+0x838) && level<=c.read<int32_t>(r+0x83c),"Monster level is outside native Region bounds");auto manager=c.read<uintptr_t>(r+0x988);type(c,manager,0x1714100);auto pose_value=args.at("pose");need(pose_value.is_object() && pose_value.size()==2 && pose_value.contains("position") && pose_value.contains("rotation"),"Monster native pose requires position and rotation");pose_value["scale"]={1,1,1};auto pose=model::pose(pose_value);auto world=map(c);auto shard=world+0x1c0;uintptr_t at=0;
    native([&]{at=call<uintptr_t>(c,0x32a990,reinterpret_cast<void*>(world),pose.position.data(),static_cast<void*>(nullptr));});need(at==r,"Monster placement point is outside requested root Region");
    auto before=census(c,Json::object());Json rows=Json::array();for(const auto& row:before)rows.push_back(actor(c,row));auto members=array(c,manager+0x38);auto history=undo(c);auto cash=ledger();auto quests=quest_census(c);auto visitors=visitor_references(c,quests);auto release_manager=c.singleton(0x13df4f0);type(c,release_manager,0x1714ed0);auto release=c.read<uintptr_t>(release_manager+0x38);need(release!=0,"Monster placement current release is absent");auto edits=c.read<std::array<int32_t,8>>(release+0x120);need(edits[6]<INT32_MAX,"Monster release edit counter is at int32 bound");auto expected_edits=edits;++expected_edits[6];auto created=c.read<int32_t>(manager+0x90);need(created<INT32_MAX,"Monster manager created counter is at int32 bound");auto tag=c.read<int32_t>(c.image+0x1a30b70);need(tag>=0 && tag<INT32_MAX,"Monster global next tag is outside native bounds");
    for(auto rva:{0x9ff370,0x293520,0x50eb70,0x3c06e0,0x58a290,0xa02670,0x5400f0,0x53dc30,0x540760,0x538ae0,0x53b260,0x11b2b00,0x11bd1e0})c.verify_original(rva);
    c.verify_hash(0x293520,976,"8b61921c479b6e67309966e9fa225b1e4e0c2701b2bd0c68f763bdecf1c8c43f");c.verify_hash(0x5bf3c0,800,"2c35b11295c5583540d776c90680802c901c19d95e2ee652038469150603bea8");need(c.read<uintptr_t>(c.image+0x16c0770+0x470)==c.image+0x3c06e0,"Monster native LevelChanged virtual differs");auto sound=c.singleton(0x13df450);need(sound!=0,"Monster placement Sound singleton is absent");alignas(16) std::array<unsigned char,0x70> transform{};uintptr_t object=0;attempted=true;
    native([&]{call<void>(c,0x9ff370,transform.data(),pose.rotation.data(),pose.position.data(),pose.scale.data());object=call<uintptr_t>(c,0x293520,reinterpret_cast<void*>(manager),transform.data(),rank,static_cast<void*>(nullptr),index);need(object!=0,"Monster native SpawnNewMonsterHere returned null");call<void>(c,0x50eb70,reinterpret_cast<void*>(sound),int32_t(0x2f),reinterpret_cast<void*>(shard),pose.position.data());put(object,0x6f8,level);call<void>(c,0x3c06e0,reinterpret_cast<void*>(object));call<void>(c,0x58a290,static_cast<void*>(nullptr),int32_t(1),int32_t(6),reinterpret_cast<void*>(r));});quest_undo(c,object,"Place Monster",true);
    auto current=selected(c,handle(c,object));need(current.kind=="monster" && current.region==saved,"Monster original birth is outside its requested manager");auto expected_members=members;expected_members.push_back(object);need(array(c,manager+0x38)==expected_members && c.read<int32_t>(manager+0x90)==created+1 && c.read<int32_t>(object+0x820)==tag && c.read<int32_t>(c.image+0x1a30b70)==tag+1,"Monster native membership/created/tag counters differ");auto after=census(c,Json::object());need(after.size()==before.size()+1,"Monster placement changed actor cohort count unexpectedly");size_t survivors=0;for(const auto& row:after)if(row.object!=object){need(survivors<before.size() && row.object==before[survivors].object && actor(c,row)==rows[survivors],"Monster placement changed a surviving actor record/order");++survivors;}need(survivors==before.size(),"Monster placement omitted a survivor");auto result=actor(c,current);need(result.at("position")==Json(pose.position) && result.at("rotation")==Json(pose.rotation) && result.at("scale")==Json(pose.scale) && result.at("character_kind")==kind && result.at("character_index")==index && result.at("level")==level && c.read<int32_t>(object+0x838)==rank && !c.read<uintptr_t>(object+0x8d0),"Monster native pose/definition/rank/zone/level readback differs");
    auto now=undo(c);need(now.manager==history.manager && !now.undo.empty() && now.redo.empty(),"Monster native Undo manager/redo differs");auto graph=now.undo.back();auto expected_history=history.undo;expected_history.push_back(graph);if(expected_history.size()>20)expected_history.erase(expected_history.begin());need(expected_history==now.undo && std::find(history.undo.begin(),history.undo.end(),graph)==history.undo.end(),"Monster native Undo commit history differs");type(c,graph,0x16e32d0);need(array(c,graph).empty() && array(c,graph+0x48).empty() && c.read<uint8_t>(graph+0x10c)==1,"Monster Undo graph is not drained/locked");auto inverses=array(c,graph+0x18,1);need(inverses.size()==1,"Monster Undo inverse count differs");type(c,inverses[0],0x16de860);need(c.read<uint64_t>(inverses[0]+0xb8)==c.read<uint64_t>(object+0x18),"Monster native destroy inverse UID differs");
    auto after_quests=quest_census(c);need(after_quests.members==quests.members && after_quests.completed==quests.completed && after_quests.rows==quests.rows,"Monster placement changed authored quests");verify_references(visitors,visitor_references(c,after_quests));need(cash==ledger() && c.read<uintptr_t>(release_manager+0x38)==release && c.read<std::array<int32_t,8>>(release+0x120)==expected_edits,"Monster placement ledger/release edits differ");return result;
}
Json place_npc(Context& c,const Json& args,bool& attempted) {
    auto definition=character(c,args.at("character"));auto kind=c.read<int32_t>(definition+0x2c),index=c.read<int32_t>(definition+0x30);auto saved=model::integer(args.at("region"),0),npc_type=model::integer(args.at("type"),0,2);auto r=region(c,saved);need(c.read<uint8_t>(r+0x850)==1,"NPC placement requires an active native Region");auto manager=c.read<uintptr_t>(r+0x998);need(manager && c.read<uintptr_t>(manager+0xc8)==r,"NPC placement manager Region differs");auto pose=model::pose(args.at("pose"));auto world=map(c);auto shard=world+0x1c0;uintptr_t at=0;int32_t cost=0;
    native([&]{at=call<uintptr_t>(c,0x32a990,reinterpret_cast<void*>(world),pose.position.data(),static_cast<void*>(nullptr));cost=call<int32_t>(c,0x3c4380,npc_type);});need(at==r && cost>=0,"NPC placement point or original refund cost differs");
    auto before=census(c,Json::object());Json rows=Json::array();for(const auto& row:before)rows.push_back(actor(c,row));auto members=array(c,manager+0x38+size_t(npc_type)*0x30);auto history=undo(c);auto cash=ledger();auto quests=quest_census(c);auto visitors=visitor_references(c,quests);need(quests.members.size()+size_t(npc_type==0?2:0)<=4096,"NPC placement native quest capacity is reached");
    if(npc_type==0){auto buildings=c.read<uintptr_t>(r+0x950);type(c,buildings,0x17183c0);need(c.read<uintptr_t>(buildings+0x458)==r && !array(c,buildings+0x408).empty(),"QuestGiver placement requires an original Building fallback destination");}
    auto release_manager=c.singleton(0x13df4f0);type(c,release_manager,0x1714ed0);auto release=c.read<uintptr_t>(release_manager+0x38);need(release!=0,"NPC current release is absent");auto edits=c.read<std::array<int32_t,8>>(release+0x120);need(edits[6]<INT32_MAX,"NPC release edit counter is at int32 bound");auto expected_edits=edits;++expected_edits[6];auto tag=c.read<int32_t>(manager+0xdc);need(tag>=0 && tag<=99,"NPC manager next tag differs");auto sound=c.singleton(0x13df450);need(sound,"NPC placement Sound singleton is absent");
    for(auto rva:{0x9ff370,0x2967b0,0x50eb70,0x58a290,0xa02670,0x5400f0,0x53ccb0,0x53dc30,0x540760,0x538ae0,0x53b260,0x11b2b00,0x11bd1e0})c.verify_original(rva);c.verify_hash(0x2967b0,928,"7e123d635af94ad599fb2cd2922546c162fbbc50037c54065d996144f505821e");c.verify_hash(0x5c3ef0,800,"289fae8589c089f39b806c7c1b755da3ebe3f98664e90106d229b0fd65cbc0a7");alignas(16) std::array<unsigned char,0x70> transform{};std::array<int32_t,2> character_id{kind,index};uintptr_t object=0;attempted=true;
    native([&]{call<void>(c,0x9ff370,transform.data(),pose.rotation.data(),pose.position.data(),pose.scale.data());object=call<uintptr_t>(c,0x2967b0,reinterpret_cast<void*>(manager),npc_type,character_id.data(),transform.data(),false);need(object,"NPC original AddNPC returned null");call<void>(c,0x50eb70,reinterpret_cast<void*>(sound),int32_t(0x2e),reinterpret_cast<void*>(shard),pose.position.data());call<void>(c,0x58a290,static_cast<void*>(nullptr),int32_t(1),int32_t(6),reinterpret_cast<void*>(r));});quest_undo(c,object,"Place NPC",true,cost);
    auto current=selected(c,handle(c,object));need(current.kind=="npc" && current.region==saved,"NPC original birth is outside requested manager");auto expected_members=members;expected_members.push_back(object);need(array(c,manager+0x38+size_t(npc_type)*0x30)==expected_members && c.read<int32_t>(object+0x830)==tag && c.read<int32_t>(manager+0xdc)==(tag==99?0:tag+1),"NPC birth membership/tag readback differs");auto after=census(c,Json::object());need(after.size()==before.size()+1,"NPC placement changed actor cohort size unexpectedly");size_t survivors=0;for(const auto& row:after)if(row.object!=object){need(survivors<before.size() && row.object==before[survivors].object && actor(c,row)==rows[survivors],"NPC placement changed a surviving actor record/order");++survivors;}need(survivors==before.size(),"NPC placement omitted a survivor");auto result=actor(c,current);need(result.at("position")==Json(pose.position) && result.at("rotation")==Json(pose.rotation) && result.at("scale")==Json(pose.scale) && result.at("character_kind")==kind && result.at("character_index")==index && result.at("npc_type")==npc_type && result.at("level")==c.read<int32_t>(r+0x838)+10 && result.at("command_level")==c.read<int32_t>(r+0x838),"NPC native pose/type/level readback differs");
    auto new_quests=array(c,object+0x860,5);need(new_quests.size()==size_t(npc_type==0?2:0),"NPC native generated quest count differs");auto after_quests=quest_census(c);auto expected_quests=quests.members;expected_quests.insert(expected_quests.end(),new_quests.begin(),new_quests.end());need(after_quests.members==expected_quests && after_quests.completed==quests.completed,"NPC native global quest append/completion differs");for(size_t i=0;i<quests.rows.size();++i)need(after_quests.rows[i]==quests.rows[i],"NPC placement changed an existing authored quest");for(size_t i=0;i<new_quests.size();++i){auto q=quest(c,object,new_quests[i],i);need(q.at("type")==1 && q.at("handle").at("sequence")==i && q.at("accepted_total")==0 && q.at("completed_total")==0,"NPC generated quest defaults differ");type(c,c.read<uintptr_t>(new_quests[i]+0x218),0x1714ac0);}verify_references(visitors,visitor_references(c,after_quests));
    auto now=undo(c);need(now.manager==history.manager && !now.undo.empty() && now.redo.empty(),"NPC placement Undo manager/redo differs");auto graph=now.undo.back();auto expected_history=history.undo;expected_history.push_back(graph);if(expected_history.size()>20)expected_history.erase(expected_history.begin());need(expected_history==now.undo && std::find(history.undo.begin(),history.undo.end(),graph)==history.undo.end(),"NPC placement Undo history differs");type(c,graph,0x16e32d0);need(array(c,graph).empty() && array(c,graph+0x48).empty() && c.read<uint8_t>(graph+0x10c)==1,"NPC placement Undo graph is not drained/locked");auto inverses=array(c,graph+0x18,2);need(inverses.size()==2,"NPC placement Undo inverse count differs");type(c,inverses[0],0x1786fb0);type(c,inverses[1],0x16de860);need(c.read<int32_t>(inverses[0]+0xb8)==cost && c.read<std::array<float,3>>(inverses[0]+0xbc)==pose.position && c.read<uint64_t>(inverses[1]+0xb8)==c.read<uint64_t>(object+0x18),"NPC original cash/destroy Undo inverse differs");need(cash==ledger() && c.read<uintptr_t>(release_manager+0x38)==release && c.read<std::array<int32_t,8>>(release+0x120)==expected_edits,"NPC placement ledger/release edits differ");return {{"actor",result},{"quests",owner_quests(c,object)}};
}
uintptr_t move_native(Context& c,uintptr_t object,const model::Pose& pose) {
    alignas(16) std::array<unsigned char,0x70> transform{};alignas(16) std::array<unsigned char,0x38> label{};alignas(16) std::array<unsigned char,0x130> action{};
    bool labeled=false,begun=false,opened=false,constructed=false;uintptr_t graph=0;std::string failure;
    try{native([&]{call<void>(c,0x9ff370,transform.data(),pose.rotation.data(),pose.position.data(),pose.scale.data());call<void>(c,0xa16f10,label.data(),"Move");labeled=true;call<void>(c,0x265300,label.data());begun=true;graph=call<uintptr_t>(c,0x539990,label.data());opened=true;need(graph,"actor native Undo Open returned null");call<void>(c,0x55e8e0,action.data(),reinterpret_cast<void*>(object),transform.data());constructed=true;call<void>(c,0x55ed80,action.data(),reinterpret_cast<void*>(graph));});}catch(const std::exception& e){failure=e.what();}
    auto cleanup=[&](const char* name,auto&& fn){try{native(fn);}catch(const std::exception& e){if(!failure.empty())failure+="; ";failure+=std::string(name)+": "+e.what();}};
    if(constructed)cleanup("actor move destructor",[&]{call<void>(c,0xcf1600,action.data());});
    if(opened && graph)cleanup("actor native Undo Close",[&]{call<void>(c,0x5379c0);});
    if(begun)cleanup("actor native EndEdit",[&]{call<void>(c,0x265650);});
    if(labeled){cleanup("actor move label arguments",[&]{call<void>(c,0x11b2b00,label.data()+0x20);});cleanup("actor move label string",[&]{call<void>(c,0x11bd1e0,label.data());});}
    need(failure.empty(),failure);return graph;
}
Json move_actor(Context& c,const Json& args,bool& attempted) {
    auto chosen=selected(c,args.at("handle"));need(chosen.kind=="npc" || chosen.kind=="monster" || chosen.kind=="boss","actor move requires a current NPC, Monster or Boss");auto object=chosen.object;auto before=actor(c,chosen),expected=before;auto pose=model::pose(args.at("pose"));expected["position"]=pose.position;expected["rotation"]=pose.rotation;expected["scale"]=pose.scale;if(expected==before)return before;
    auto owner=region(c,chosen.region),world=map(c);auto automatic=c.read<uint8_t>(object+0x490);need(c.read<uintptr_t>(object+0x170)==world+0x1c0 && automatic<=1 && (chosen.kind=="boss" || automatic==0),"actor move root/fixed Region ownership differs");uintptr_t destination=0;native([&]{destination=call<uintptr_t>(c,0x32a990,reinterpret_cast<void*>(world),pose.position.data(),static_cast<void*>(nullptr));});need(destination && c.read<uintptr_t>(destination+0x70)==world+0x1c0,"actor move destination is outside current root world");if(chosen.kind=="boss")need(destination==owner,"Boss movement requires a destination within its current automatic Region owner");auto notify=chosen.kind=="npc"?0x3c2a40:(chosen.kind=="monster"?0x3b9eb0:0x39afe0);need(c.read<uintptr_t>(c.read<uintptr_t>(object)+0x2a0)==c.image+notify,"actor original NotifyMoved virtual differs");auto visual=c.read<uintptr_t>(object+0x6f0);need(visual && c.read<uint8_t>(object+0x368)==1,"actor move requires a registered on-map visual");auto spawn_offset=chosen.kind=="npc"?0x818:(chosen.kind=="monster"?0x82c:0x970);auto spawn_before=c.read<std::array<float,3>>(object+spawn_offset);auto boss_spawn_region=chosen.kind=="boss"?c.read<uintptr_t>(object+0x980):0;
    auto all=census(c,Json::object());Json rows=Json::array();for(const auto& row:all)rows.push_back(actor(c,row));auto quests=quest_census(c);auto visitors=visitor_references(c,quests);auto history=undo(c);auto cash=ledger();auto selection=c.singleton(0x13df630);type(c,selection,0x17205d0);need(!c.read<uint8_t>(selection+0xd1) && !c.read<uint8_t>(selection+0xd3),"actor move requires idle native selection");auto prior_selection=array(c,selection+0x20),prior_filtered=array(c,selection+0x38);auto focus=c.read<uintptr_t>(selection+0x80);
    for(auto rva:{0x9ff370,0xa16f10,0x265300,0x539990,0x55e8e0,0x55ed80,0xcf1600,0x5379c0,0x265650,0x11b2b00,0x11bd1e0,0x2d5910})c.verify_original(rva);c.verify_original(notify);c.verify_hash(0x55ed80,704,"555aef77c8b022b696a31e5c65a0c6c8a8f860449323943d4980e48187214aee");
    uintptr_t graph=0;std::string failure;attempted=true;try{graph=move_native(c,object,pose);}catch(const std::exception& e){failure=e.what();}
    struct NativeArray {uintptr_t ap;uintptr_t* data;int32_t count,capacity;};NativeArray restore{c.image+0x1748c50,prior_selection.data(),int32_t(prior_selection.size()),int32_t(prior_selection.size())};
    try{native([&]{call<void>(c,0x2d5910,reinterpret_cast<void*>(selection),&restore);});need(array(c,selection+0x20)==prior_selection && array(c,selection+0x38)==prior_filtered && c.read<uintptr_t>(selection+0x80)==focus,"actor move selection restoration readback differs");}catch(const std::exception& e){if(!failure.empty())failure+="; ";failure+="actor selection restoration: "+std::string(e.what());}need(failure.empty(),failure);
    auto current=selected(c,args.at("handle"));need(current.object==object && current.region==chosen.region && actor(c,current)==expected && c.read<uintptr_t>(object+0x178)==owner && c.read<uintptr_t>(object+0x6f0)==visual && c.read<uint8_t>(object+0x368)==1 && c.read<uint8_t>(object+0x490)==automatic && c.read<std::array<float,3>>(object+spawn_offset)==(chosen.kind=="boss"?spawn_before:pose.position) && (chosen.kind!="boss" || c.read<uintptr_t>(object+0x980)==boss_spawn_region),"actor move complete pose/spawn/visual/owner readback differs");auto after=census(c,Json::object());need(after.size()==all.size(),"actor move changed cohort count");for(size_t i=0;i<all.size();++i)need(after[i].object==all[i].object && actor(c,after[i])==(all[i].object==object?expected:rows[i]),"actor move changed another actor or membership order");
    auto now=undo(c);need(now.manager==history.manager && !now.undo.empty() && now.undo.back()==graph && now.redo.empty(),"actor move Undo commit/redo differs");auto expected_history=history.undo;expected_history.push_back(graph);if(expected_history.size()>20)expected_history.erase(expected_history.begin());need(now.undo==expected_history && std::find(history.undo.begin(),history.undo.end(),graph)==history.undo.end(),"actor move Undo history differs");type(c,graph,0x16e32d0);need(array(c,graph).empty() && array(c,graph+0x48).empty() && c.read<uint8_t>(graph+0x10c)==1,"actor move graph is not drained/locked");auto operations=array(c,graph+0x18,1);need(operations.size()==1,"actor move inverse count differs");auto inverse=operations[0];type(c,inverse,0x1786ff0);need(c.read<uint64_t>(inverse+0xb8)==c.read<uint64_t>(object+0x18) && Json(c.read<std::array<float,4>>(inverse+0xc0))==before.at("rotation") && Json(c.read<std::array<float,3>>(inverse+0xd0))==before.at("position") && Json(c.read<std::array<float,3>>(inverse+0xdc))==before.at("scale"),"actor move inverse UID/prior transform differs");auto after_quests=quest_census(c);need(after_quests.members==quests.members && after_quests.completed==quests.completed && after_quests.rows==quests.rows,"actor move changed authored quests");verify_references(visitors,visitor_references(c,after_quests));need(cash==ledger(),"actor move changed ledger");return actor(c,current);
}
void saved_quest_records(Context& c,uintptr_t data,int32_t count,int32_t capacity,const Json& before) {
    need(count>=0 && count<=5 && capacity>=count && (!count || data),"actor removal saved quest bounds differ");size_t index=0;for(const auto& row:before){if(row.at("action")==8)continue;need(index<size_t(count),"actor removal inverse omitted a quest");auto p=data+index*0x58;need(c.read<int32_t>(p)==row.at("type") && std::to_string(c.read<uint64_t>(p+8))==row.at("destination").at("uid").get<std::string>() && c.read<int32_t>(p+0x10)==row.at("action") && c.read<int32_t>(p+0x14)==row.at("monsters_to_kill") && c.text(p+0x18,4096)==row.at("name").get<std::string>() && c.text(p+0x38,65536)=="[Quest description goes here]","actor removal inverse authored quest record differs");++index;}need(index==size_t(count),"actor removal inverse contains an extra quest");
}
Json remove_actor(Context& c,const Json& args,bool& attempted) {
    auto chosen=selected(c,args.at("handle"));need(chosen.kind=="npc" || chosen.kind=="monster","actor removal requires a current NPC or Monster");auto object=chosen.object;auto old=actor(c,chosen);auto uid=old.at("handle").at("uid").get<std::string>();auto raw_uid=c.read<uint64_t>(object+0x18);auto r=region(c,chosen.region),world=map(c);need(c.read<uintptr_t>(object+0x170)==world+0x1c0,"actor removal Shard differs");need(c.read<uintptr_t>(c.read<uintptr_t>(object)+0x278)==c.image+0xe3f510,"actor native zero-cost contract differs");if(chosen.kind=="monster")need(!c.read<uintptr_t>(object+0x8d0),"direct actor removal requires a Monster outside a generating Zone");
    auto selection=c.singleton(0x13df630);type(c,selection,0x17205d0);need(!c.read<uint8_t>(selection+0xd1) && !c.read<uint8_t>(selection+0xd3),"actor removal requires idle native selection");auto mode=c.singleton(0x13df320);need(mode && c.read<int32_t>(mode+0x30c)==0,"actor removal requires no native edit in progress");auto history=undo(c);auto cash=ledger();auto all=census(c,Json::object());Json rows=Json::array();for(const auto& row:all)if(row.object!=object)rows.push_back(actor(c,row));auto before=quest_census(c);auto refs=visitor_references(c,before);auto expected_rows=model::surviving_quests(before.rows,uid);std::vector<uintptr_t> expected_members;std::set<uintptr_t> retired;std::map<std::string,Json> owner_records;std::set<std::string> incoming;int32_t completed=before.completed;
    for(size_t i=0;i<before.members.size();++i){const auto& row=before.rows[i];auto owner_uid=row.at("handle").at("owner").at("uid").get<std::string>();if(!owner_records.contains(owner_uid)){auto owner=c.read<uintptr_t>(before.members[i]+0xb8);owner_records[owner_uid]=owner_quests(c,owner);}if(model::retires_quest(row,uid)){retired.insert(before.members[i]);if(row.at("completed_total").get<int32_t>()>0)--completed;if(owner_uid!=uid)incoming.insert(owner_uid);}else expected_members.push_back(before.members[i]);}
    auto removal_manager=chosen.kind=="monster"?c.read<uintptr_t>(r+0x988):0;auto created_before=removal_manager?c.read<int32_t>(removal_manager+0x90):0;auto global_tag_before=c.read<int32_t>(c.image+0x1a30b70);if(removal_manager)need(created_before>=0 && created_before<INT32_MAX,"Monster removal lifecycle counter exceeds native bounds");auto rank=chosen.kind=="monster"?c.read<int32_t>(object+0x838):0;std::array<int32_t,2> character_id{c.read<int32_t>(object+0x4a8),c.read<int32_t>(object+0x4a4)};auto actor_type=chosen.kind=="npc"?c.read<int32_t>(object+0x80c):0;
    for(auto rva:{0x2d5520,0x47ce20,0x874df0})c.verify_original(rva);c.verify_hash(0x2d5520,432,"d4a6e096321192899165b722b0dab5c2039ed34f436aa4e5903785f148678a99");c.verify_hash(0x2d42c0,2768,"c526304f6ce4e9014154daf39a936bae914f448e7788f80d816b52615fb68e8f");attempted=true;native([&]{call<void>(c,0x2d5520,reinterpret_cast<void*>(selection),reinterpret_cast<void*>(object));});uintptr_t registered=1;native([&]{registered=call<uintptr_t>(c,0x47ce20,raw_uid);});need(!registered,"actor full UID remains registered after native removal");
    auto after=census(c,Json::object());need(after.size()+1==all.size(),"actor removal cohort count differs");for(size_t i=0;i<after.size();++i)need(after[i].object!=object && actor(c,after[i])==rows[i],"actor removal changed a surviving actor or membership order");bool present=true;native([&]{present=call<bool>(c,0x874df0,reinterpret_cast<void*>(r+0x7c8),reinterpret_cast<void*>(object));});need(!present,"actor removal retained Region CharacterRegistry membership");if(chosen.kind=="monster"){auto manager=c.read<uintptr_t>(r+0x988);for(auto active:array(c,manager+0x68))need(active!=object,"removed Monster remains active");auto spatial=c.read<uintptr_t>(manager+0x88);need(spatial,"Monster spatial index is absent after removal");native([&]{present=call<bool>(c,0x874df0,reinterpret_cast<void*>(spatial),reinterpret_cast<void*>(object));});need(!present,"removed Monster remains in spatial index");need(c.read<int32_t>(manager+0x90)==created_before+1 && c.read<int32_t>(c.image+0x1a30b70)==global_tag_before,"Monster removal lifetime/global tag counters differ");}
    for(auto offset:{size_t(0x20),size_t(0x38)})for(auto member:array(c,selection+offset))need(member!=object,"removed actor remains selected");need(c.read<uintptr_t>(selection+0x80)!=object,"removed actor remains focused");auto after_quests=quest_census(c);need(after_quests.manager==before.manager && after_quests.members==expected_members && after_quests.rows==expected_rows && after_quests.completed==completed,"actor removal quest cascade/global counters differ");
    auto after_refs=visitor_references(c,after_quests);auto expected_refs=refs;need(refs.size()==after_refs.size(),"actor removal changed Subscriber cohort");for(size_t i=0;i<expected_refs.size();++i){auto& progress=expected_refs[i]["progress"];need(progress.size()==after_refs[i]["progress"].size(),"actor removal changed visitor progress membership");for(size_t j=0;j<progress.size();++j){auto& row=progress[j];for(const char* key:{"npc","quest"})if(row[key]["state"]==2 && ((std::string(key)=="npc" && row[key]["target"]==object) || (std::string(key)=="quest" && retired.contains(row[key]["target"].get<uintptr_t>())))){const auto& link=after_refs[i]["progress"][j][key];need(link["state"]!=2 && link["target"]==0,"actor removal retained a live visitor reference");row[key]=link;if(std::string(key)=="quest")row["complete"]=false;}}}need(expected_refs==after_refs,"actor removal changed unrelated visitor links/counters/rewards");
    auto now=undo(c);need(now.manager==history.manager && !now.undo.empty() && now.redo.empty(),"actor removal native Undo/redo differs");auto graph=now.undo.back();auto expected_history=history.undo;expected_history.push_back(graph);if(expected_history.size()>20)expected_history.erase(expected_history.begin());need(now.undo==expected_history && std::find(history.undo.begin(),history.undo.end(),graph)==history.undo.end(),"actor removal native Undo history differs");type(c,graph,0x16e32d0);need(array(c,graph).empty() && array(c,graph+0x48).empty() && c.read<uint8_t>(graph+0x10c)==1,"actor removal native graph is not drained/locked");auto operations=array(c,graph+0x18,4099);size_t actor_inverse=0;std::set<std::string> saved;
    for(auto inverse:operations){auto ap=c.read<uintptr_t>(inverse);if(ap==c.image+0x1713330){auto owner_uid=std::to_string(c.read<uint64_t>(inverse+0xb8));need(incoming.contains(owner_uid) && saved.insert(owner_uid).second && c.read<uint8_t>(inverse+0xc0)==0,"actor removal incoming quest inverse owner/flag differs");saved_quest_records(c,c.read<uintptr_t>(inverse+0xd0),c.read<int32_t>(inverse+0xd8),c.read<int32_t>(inverse+0xdc),owner_records.at(owner_uid));continue;}
        need(ap==c.image+(chosen.kind=="npc"?0x16de8a0:0x1706550) && ++actor_inverse==1 && c.read<uint64_t>(inverse+0xb8)==raw_uid && c.text(inverse+0xc0,256)==old.at("name").get<std::string>(),"actor removal native actor inverse identity differs");auto pose_offset=chosen.kind=="npc"?0xf8:0x100;need(Json(c.read<std::array<float,4>>(inverse+pose_offset))==old.at("rotation") && Json(c.read<std::array<float,3>>(inverse+pose_offset+0x10))==old.at("position") && Json(c.read<std::array<float,3>>(inverse+pose_offset+0x1c))==old.at("scale"),"actor removal inverse prior pose differs");
        if(chosen.kind=="npc"){need(c.read<int32_t>(inverse+0xe0)==actor_type && c.read<uintptr_t>(inverse+0xe8)==world+0x1c0 && c.read<uintptr_t>(inverse+0xf0)==r && c.read<int32_t>(inverse+0x164)==old.at("level") && c.read<std::array<int32_t,2>>(inverse+0x168)==character_id,"NPC removal inverse original role/owners/level/definition differs");auto prior=owner_records.contains(uid)?owner_records.at(uid):Json::array();type(c,inverse+0x170,0x17469a0);saved_quest_records(c,c.read<uintptr_t>(inverse+0x178),c.read<int32_t>(inverse+0x180),c.read<int32_t>(inverse+0x184),prior);}
        else need(c.read<uintptr_t>(inverse+0xe0)==world+0x1c0 && c.read<uintptr_t>(inverse+0xe8)==r && c.read<int32_t>(inverse+0xf0)==rank && !c.read<uintptr_t>(inverse+0xf8) && c.read<int32_t>(inverse+0x16c)==old.at("level") && c.read<std::array<int32_t,2>>(inverse+0x170)==character_id,"Monster removal inverse original owners/rank/level/definition differs");}
    need(actor_inverse==1 && saved==incoming && operations.size()==incoming.size()+1,"actor removal native inverse coverage differs");need(cash==ledger(),"actor removal changed native zero-cost ledger");return {{"removed",old.at("handle")},{"quests_removed",retired.size()},{"undo_confirmed",true}};
}
#include "zone.inc"
#include "zone_edit.inc"
#include "zone_remove.inc"
#include "boss.inc"
#include "zone_paint.inc"
}
std::optional<Json> query(Context& c,const std::string& operation,const Json& args) {
    if(operation=="actors.regions"){Json out=Json::array();for(auto p:regions(c))out.push_back({{"id",c.read<int32_t>(p+0x834)},{"minimum_level",c.read<int32_t>(p+0x838)},{"maximum_level",c.read<int32_t>(p+0x83c)}});return out;}
    if(operation=="actors.list" || operation=="npcs.list" || operation=="monsters.list"){Json out=Json::array();for(const auto& row:census(c,args))if(operation=="actors.list" || row.kind==(operation=="npcs.list"?"npc":"monster"))out.push_back(actor(c,row));return out;}
    if(operation=="actors.get")return actor(c,selected(c,args.at("handle")));
    if(operation=="actors.boss_phases")return phases(c,character(c,args.at("handle")));
    if(operation=="monsters.zones"){Json out=Json::array();for(const auto& row:zone_census(c,args))out.push_back(zone_json(c,row));return out;}
    if(operation=="monsters.get_zone"){auto pointer=c.resolve(args.at("handle"),0x1745ce0);for(const auto& row:zone_census(c,Json::object()))if(row.object==pointer)return zone_json(c,row);throw std::runtime_error("Monster Zone handle is absent from current root manager membership");}
    if(operation=="quests.get"){auto value=selected_quest(c,args.at("handle"));return quest(c,value.owner.object,value.pointer,value.slot);}
    if(operation=="quests.list"){
        auto owner=selected(c,args.at("handle"));need(owner.kind=="npc" && c.read<int32_t>(owner.object+0x80c)==0,"quest list requires a native QuestGiver NPC");Json out=Json::array();auto rows=array(c,owner.object+0x860);
        for(size_t i=0;i<rows.size();++i)out.push_back(quest(c,owner.object,rows[i],i));return out;
    }
    return std::nullopt;
}
bool action(Context& c,const std::string& operation,const Json& args,bool& attempted,Json& result,std::string& cause) {
    static const std::set<std::string> operations{"monsters.paint_zone","monsters.erase_zones","monsters.remove_zone","actors.remove_boss","monsters.set_zone","monsters.reset_zone","monsters.respawn_zone","monsters.create_zone","actors.remove","actors.move","npcs.place","monsters.place","actors.place_boss","quests.create","quests.remove","quests.swap","quests.set_name","quests.set_kills","quests.set_rewards","quests.set_destination","npcs.rename","npcs.set_online","npcs.change_level","actors.create_boss_phase","actors.remove_boss_phase","actors.reorder_boss_phase","actors.set_boss_phase","actors.add_phase_ability","actors.remove_phase_ability"};
    if(!operations.contains(operation))return false;attempted=false;cause.clear();
    try {
        c.require_world();
        if(operation=="monsters.paint_zone" || operation=="monsters.erase_zones"){result=paint_zones(c,args,operation=="monsters.paint_zone",attempted);return true;}
        if(operation=="monsters.remove_zone"){result=remove_zone(c,args,attempted);return true;}
        if(operation=="actors.remove_boss"){result=remove_boss(c,args,attempted);return true;}
        if(operation=="monsters.set_zone" || operation=="monsters.reset_zone"){result=edit_zone(c,operation,args,attempted);return true;}
        if(operation=="monsters.respawn_zone"){result=respawn_zone(c,args,attempted);return true;}
        if(operation=="monsters.create_zone"){result=create_zone(c,args,attempted);return true;}
        if(operation=="actors.place_boss"){result=place_boss(c,args,attempted);return true;}
        if(operation=="monsters.place"){result=place_monster(c,args,attempted);return true;}
        if(operation=="npcs.place"){result=place_npc(c,args,attempted);return true;}
        if(operation=="actors.move"){result=move_actor(c,args,attempted);return true;}
        if(operation=="actors.remove"){result=remove_actor(c,args,attempted);return true;}
        if(operation=="quests.set_destination"){result=set_destination(c,args,attempted);return true;}
        if(operation.rfind("quests.",0)==0){result=edit_quest(c,operation,args,attempted);return true;}
        if(operation.rfind("npcs.",0)==0){
            auto current=selected(c,args.at("handle"));need(current.kind=="npc","NPC operation requires a current native NPC");auto before=actor(c,current),expected=before;
            if(operation=="npcs.rename"){
                auto name=model::text(args.at("name"),256);need(!name.empty(),"NPC name cannot be empty");if(before.at("name")==name){result=before;return true;}Text text{name.data(),name.size()};c.verify_original(0x4cf710);attempted=true;native([&]{call<void>(c,0x4cf710,reinterpret_cast<void*>(current.object),&text);});expected["name"]=name;
            }else if(operation=="npcs.set_online"){
                auto mode=model::integer(args.at("mode"),0,2);if(before.at("online")==mode){result=before;return true;}std::array<float,4> tint{};
                if(mode!=2){auto color=c.singleton(mode==1?0x164c5e0:0x164c5c0);tint=c.read<std::array<float,4>>(color);for(float v:tint)need(std::isfinite(v),"NPC native online color is non-finite");}
                bool accepted=false;c.verify_original(0x3c4800);attempted=true;native([&]{accepted=call<bool>(c,0x3c4800,reinterpret_cast<void*>(current.object),mode);});need(accepted,"NPC original online command was not accepted");expected["online"]=mode;expected["force_tint"]=mode!=2;if(mode!=2)expected["tint"]=tint;
            }else{
                auto step=model::integer(args.at("delta"),-1,1);need(step!=0,"NPC level delta must be -1 or 1");auto r=region(c,current.region);auto minimum=c.read<int32_t>(r+0x838),maximum=c.read<int32_t>(r+0x83c);need(minimum>=0 && maximum>=minimum,"NPC Region level bounds differ");
                auto target=std::clamp(int64_t(before.at("command_level").get<int32_t>())+step,int64_t(minimum),int64_t(maximum));need(target!=before.at("command_level").get<int32_t>(),"NPC level is already at the native Region bound");std::string command=step>0?"Raise Level":"Lower Level";Text text{command.data(),command.size()};bool accepted=false;c.verify_original(0x3c4c10);attempted=true;native([&]{accepted=call<bool>(c,0x3c4c10,reinterpret_cast<void*>(current.object),&text);});need(accepted,"NPC original level command was not accepted");expected["command_level"]=target;expected["level"]=target;
            }
            result=actor(c,current);need(result==expected,"NPC complete property readback differs");return true;
        }
        auto owner=character(c,args.at("handle"));auto before=phases(c,owner),expected=before;auto members=array(c,owner+0x158,10000);int32_t index=operation=="actors.create_boss_phase"?int32_t(members.size()):model::integer(args.at("phase"),0);
        if(operation!="actors.create_boss_phase")need(size_t(index)<members.size(),"boss phase slot is absent");
        uintptr_t phase=operation=="actors.create_boss_phase"?0:members[size_t(index)];
        if(operation=="actors.create_boss_phase") {
            need(members.size()<10000,"boss phase count exceeds native bound");auto name=args.contains("name")?model::text(args.at("name")):"{design_boss_defaultphasename} "+std::to_string(members.size()+1);Text title{name.data(),name.size()};
            for(auto rva:{0xa02670,0x4fe70,0x938e0,0x11bfce0})c.verify_original(rva);attempted=true;
            native([&]{phase=call<uintptr_t>(c,0xa02670,size_t(0xc8));need(phase!=0,"boss phase allocation failed");call<void>(c,0x4fe70,reinterpret_cast<void*>(phase));call<void>(c,0x938e0,reinterpret_cast<void*>(owner),reinterpret_cast<void*>(phase));call<void>(c,0x11bfce0,reinterpret_cast<void*>(phase+0x38),&title);});
            result=phases(c,owner);need(result.size()==before.size()+1 && result.back().at("name")==name,"boss phase creation readback differs");for(size_t i=0;i<before.size();++i)need(result[i]==before[i],"boss phase creation changed an existing phase");return true;
        }
        if(operation=="actors.remove_boss_phase") {
            expected.erase(expected.begin()+index);c.verify_original(0x938f0);attempted=true;native([&]{call<void>(c,0x938f0,reinterpret_cast<void*>(owner),reinterpret_cast<void*>(phase));});
        }else if(operation=="actors.reorder_boss_phase"){
            auto boundary=model::integer(args.at("boundary"),0,int32_t(members.size()));auto destination=model::reordered_index(size_t(index),size_t(boundary),members.size());auto row=expected[size_t(index)];expected.erase(expected.begin()+index);expected.insert(expected.begin()+destination,row);c.verify_original(0x93900);attempted=true;native([&]{call<void>(c,0x93900,reinterpret_cast<void*>(owner),reinterpret_cast<void*>(phase),boundary);});
        }else if(operation=="actors.set_boss_phase") {
            auto settings=args.at("settings");auto desired=model::phase_update(before[size_t(index)],settings);expected[size_t(index)]=desired;
            for(auto rva:{0xd7e40,0xd7f00,0xd9200,0x89da30,0x11bd1e0,0x11bfce0})c.verify_original(rva);attempted=true;
            native([&]{event(c,phase,false);for(auto it=settings.begin();it!=settings.end();++it){if(it.key()=="name" || it.key()=="vfx"){auto text=model::text(it.value());Text value{text.data(),text.size()};call<void>(c,0x11bfce0,reinterpret_cast<void*>(phase+(it.key()=="name"?0x38:0x60)),&value);}else if(it.key()=="health")put(phase,0x58,model::integer(it.value()));else if(it.key()=="color")put(phase,0x80,model::color(it.value()));}event(c,phase,true);});
        }else if(operation=="actors.add_phase_ability"){
            auto index_ability=model::integer(args.at("ability"),0);auto abilities=array(c,owner+0x128,10000);need(size_t(index_ability)<abilities.size(),"boss phase ability slot is absent");type(c,abilities[size_t(index_ability)],0x171c930);need(before[size_t(index)].at("abilities").size()<10000,"boss phase element count exceeds native bound");expected[size_t(index)]["abilities"].push_back(index_ability);
            for(auto rva:{0xa02670,0x54170,0x4f8e0})c.verify_original(rva);attempted=true;native([&]{auto element=call<uintptr_t>(c,0xa02670,size_t(0x20));need(element!=0,"boss phase element allocation failed");call<void>(c,0x54170,reinterpret_cast<void*>(element));put(element,0x18,abilities[size_t(index_ability)]);call<void>(c,0x4f8e0,reinterpret_cast<void*>(phase),reinterpret_cast<void*>(element));});
        }else{
            auto element=model::integer(args.at("element"),0);need(size_t(element)<expected[size_t(index)].at("abilities").size(),"boss phase element slot is absent");expected[size_t(index)]["abilities"].erase(expected[size_t(index)]["abilities"].begin()+element);
            for(auto rva:{0xd7e40,0xd7f00,0xd9200,0x89da30,0x11bd1e0,0x4fab0})c.verify_original(rva);attempted=true;native([&]{event(c,phase,false);call<void>(c,0x4fab0,reinterpret_cast<void*>(phase),element);event(c,phase,true);});
        }
        result=phases(c,owner);need(result==expected,"boss phase complete membership/property/order readback differs");return true;
    }catch(const std::exception& e){cause=e.what();return false;}
}
}
