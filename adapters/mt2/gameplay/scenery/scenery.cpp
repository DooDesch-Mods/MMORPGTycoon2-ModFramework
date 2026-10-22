// Copyright (c) 2026 DooDesch. Original standalone adapter under the MIT license.
// Native ABI facts: Multiplayer group_reader.h, group_runtime.inc,
// group_native.h, group_inverse.h and group_object_calls.h.
#include "scenery.h"
#include "../dungeons/undo_commit.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace mt2fw::mt2::gameplay::scenery {
namespace {
void need(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
void seh(uint32_t code,const char* operation){need(!code,std::string("Scenery native ")+operation+" failed SEH="+std::to_string(code)+"; do not replay");}
Json handle(Context& c,uintptr_t p){return {{"epoch",c.epoch},{"uid",std::to_string(c.read<uint64_t>(p+0x18))}};}
std::vector<uintptr_t> array(Context& c,uintptr_t header,size_t bound=100000){
    const auto data=c.read<uintptr_t>(header+8);const auto count=c.read<int32_t>(header+16),capacity=c.read<int32_t>(header+20);
    need(count>=0 && size_t(count)<=bound && capacity>=count && (!count || (data && size_t(count)<=(UINTPTR_MAX-data)/8)),"Scenery owned array bounds differ");
    std::vector<uintptr_t> rows;std::set<uintptr_t> unique;for(int32_t n=0;n<count;++n){auto p=c.read<uintptr_t>(data+size_t(n)*8);need(p && unique.insert(p).second,"Scenery array has absent/duplicate member");rows.push_back(p);}
    need(c.read<uintptr_t>(header+8)==data && c.read<int32_t>(header+16)==count && c.read<int32_t>(header+20)==capacity,"Scenery owned array changed during read");return rows;
}
struct World {uintptr_t map=0,root=0,groups=0,selection=0,shards=0,undo=0;std::vector<uintptr_t> regions;};
World world(Context& c,bool active){
    c.require_world();World w;w.map=c.singleton(0x13df8c0);need(w.map && c.read<uintptr_t>(w.map)==c.image+0x1742860,"Scenery WorldMap class differs");w.root=w.map+0x1c0;
    w.regions=array(c,w.map+0x350);for(auto region:w.regions)need(c.read<uintptr_t>(region)==c.image+0x1787930 && c.read<uintptr_t>(region+0x70)==w.root,"Scenery current root Region owner differs");
    w.groups=c.singleton(0x13df3e0);w.selection=c.singleton(0x13df630);w.shards=c.singleton(0x13df440);w.undo=c.singleton(0x13df380);
    need(w.groups && c.read<uintptr_t>(w.groups)==c.image+0x1700890 && c.read<uintptr_t>(w.groups+0x18)==c.image+0x16f0060,"Scenery GroupManager class differs");
    if(active){need(w.shards && c.read<uintptr_t>(w.shards)==c.image+0x1702b00 && c.read<uintptr_t>(w.shards+0x60)==w.root,"Scenery action requires the root world to be the actual native active scene");
        need(w.selection && c.read<uintptr_t>(w.selection)==c.image+0x17205d0 && !c.read<uint8_t>(w.selection+0xd1) && !c.read<uint8_t>(w.selection+0xd3),"Scenery action requires idle native SelectionManager");
        need(w.undo && c.read<uintptr_t>(w.undo)==c.image+0x16fe070 && c.read<uintptr_t>(w.undo+8)==c.image+0x16e1320 && !c.read<uintptr_t>(c.image+0x1a3aa80),"Scenery action requires idle native Undo");}
    return w;
}
std::array<float,10> pose(Context& c,uintptr_t p,size_t offset=0x100){auto value=c.read<std::array<float,10>>(p+offset);for(float f:value)need(std::isfinite(f),"Scenery authored pose is non-finite");return value;}
Json prop_json(Context& c,const World& w,uintptr_t p,uintptr_t parent){
    need(c.resolve(handle(c,p),0x16c1390)==p && c.read<uintptr_t>(p+0x170)==w.root && c.read<uintptr_t>(p+0x180)==parent,"Scenery full UID/class/root/parent reciprocity differs");
    const auto region=c.read<uintptr_t>(p+0x178);need(std::find(w.regions.begin(),w.regions.end(),region)!=w.regions.end(),"Scenery Region is absent from the current root");
    const auto manager=c.read<uintptr_t>(region+0x960);const auto category=c.read<int32_t>(p+0x268);need(manager && c.read<uintptr_t>(manager)==c.image+0x17152d0 && c.read<uintptr_t>(manager+0x900)==region && c.read<uintptr_t>(manager+0x908)==w.root && category>=0 && category<47,"Scenery manager/category reciprocal owner differs");
    const auto roster=array(c,manager+0x18+size_t(category)*0x30);need(std::find(roster.begin(),roster.end(),p)!=roster.end(),"Scenery is absent from its native Region/category roster");
    return {{"handle",handle(c,p)},{"pose",pose(c,p)},{"region",c.read<int32_t>(region+0x834)}};
}
struct Group {uintptr_t pointer=0,region=0,root=0;uint64_t uid=0;Json value;std::vector<uintptr_t> members;};
std::vector<Group> groups(Context& c,const World& w){
    std::vector<Group> rows;std::set<uintptr_t> members;
    for(auto p:array(c,w.groups+0x18)){
        need(c.resolve(handle(c,p),0x1749de0)==p,"Scenery Group original UID registry differs");if(c.read<uintptr_t>(p+0x170)!=w.root)continue;
        Group row;row.pointer=p;row.root=w.root;row.uid=c.read<uint64_t>(p+0x18);row.region=c.read<uintptr_t>(p+0x178);need(std::find(w.regions.begin(),w.regions.end(),row.region)!=w.regions.end() && !c.read<uintptr_t>(p+0x180),"Scenery Group Region/parent differs");
        need(c.read<uintptr_t>(p+0x210)==c.image+0x16ea660,"Scenery Group member array class differs");Json children=Json::array();
        for(auto record:array(c,p+0x210)){need(c.read<uintptr_t>(record)==c.image+0x16fbc40,"Scenery Group member record class differs");const auto child=c.read<uintptr_t>(record+0x18);need(members.insert(child).second,"Scenery belongs to multiple Groups");
            auto value=prop_json(c,w,child,p);value["relative_pose"]=pose(c,record,0x64);children.push_back(std::move(value));row.members.push_back(child);}
        row.value={{"handle",handle(c,p)},{"pose",pose(c,p)},{"region",c.read<int32_t>(row.region+0x834)},{"members",std::move(children)}};rows.push_back(std::move(row));
    }return rows;
}
std::vector<uintptr_t> group_pointers(const std::vector<Group>& rows){std::vector<uintptr_t> out;for(const auto& row:rows)out.push_back(row.pointer);return out;}
struct NativeArray {uintptr_t ap;const uintptr_t* data;int32_t count,capacity;};
uint32_t select_native(uintptr_t image,uintptr_t receiver,const NativeArray* rows){__try{reinterpret_cast<void(*)(uintptr_t,const NativeArray*)>(image+0x2d5910)(receiver,rows);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t filter_native(uintptr_t image,uintptr_t receiver,const std::array<uint8_t,9>* filter){__try{reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0x2d58f0)(receiver,filter->data());return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t command_native(uintptr_t image,uintptr_t receiver,bool ungroup){__try{reinterpret_cast<void(*)(uintptr_t)>(image+(ungroup?0x2d5d10:0x2d7380))(receiver);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t create_native(uintptr_t image,uintptr_t manager,uintptr_t root,uintptr_t region,uintptr_t& group){
    __try{group=reinterpret_cast<uintptr_t(*)(uintptr_t)>(image+0x278bb0)(manager);if(group){reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x4cf6e0)(group,root);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x4d0140)(group,region);}return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t empty_group_native(uintptr_t image,uintptr_t group){__try{reinterpret_cast<void(*)(uintptr_t)>(image+0x4cf170)(group);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uintptr_t committed(Context& c,const World& w,const std::vector<uintptr_t>& before,size_t operations){
    const auto after=array(c,w.undo+8,20);need(!after.empty(),"Scenery original Undo commit is absent");const auto graph=after.back();
    need(dungeons::detail::committed_history(before,after,array(c,w.undo+0x20,20).empty(),c.read<uint8_t>(graph+0x10c),c.read<int32_t>(graph+0x58),c.read<uintptr_t>(c.image+0x1a3aa80)),"Scenery original Undo/Redo history or locked settlement readback differs");const auto nodes=c.read<int32_t>(graph+0x10),capacity=c.read<int32_t>(graph+0x14);
    const auto count=c.read<int32_t>(graph+0x28);need(c.read<uintptr_t>(graph)==c.image+0x16e32d0 && c.read<uintptr_t>(graph+0x18)==c.image+0x16e2dc0 && count>0 && (operations==SIZE_MAX || count==int32_t(operations)) && nodes>=0 && nodes<=100000 && capacity>=nodes && (!nodes || c.read<uintptr_t>(graph+8)) && c.read<uintptr_t>(graph+0x48)==c.image+0x1748c50,"Scenery original graph operations/content differs");return graph;
}
void inverse(Context& c,uintptr_t graph,bool ungroup,const std::vector<Group>& targets){
    const auto operations=array(c,graph+0x18);need(operations.size()==targets.size(),"Scenery grouping inverse operation count differs");
    for(size_t i=0;i<targets.size();++i){const auto operation=operations[i];const auto& target=targets[i];need(c.read<uintptr_t>(operation)==c.image+(ungroup?0x16ff380:0x1723370) && c.read<uint64_t>(operation+0xb8)==target.uid,"Scenery grouping inverse class/full Group UID differs");
        if(ungroup)need(c.read<uintptr_t>(operation+0xc0)==target.root && c.read<uintptr_t>(operation+0xc8)==target.region && Json(pose(c,operation,0xf0))==target.value.at("pose"),"Scenery Ungroup inverse root/Region/pose differs");
        const auto offset=ungroup?0x160:0xc0;const auto header=operation+offset;need(c.read<uintptr_t>(header)==c.image+0x1749370,"Scenery grouping inverse member array class differs");const auto data=c.read<uintptr_t>(header+8);const auto count=c.read<int32_t>(header+16),capacity=c.read<int32_t>(header+20);need(count>=0 && size_t(count)==target.members.size() && capacity>=count && (!count || data),"Scenery grouping inverse member bounds differ");
        for(size_t n=0;n<target.members.size();++n){const auto& h=target.value.at("members").at(n).at("handle");const auto stored=c.read<uint32_t>(data+n*4);need(std::to_string(stored)==h.at("uid").get<std::string>() && c.resolve(h,0x16c1390)==target.members[n],"Scenery grouping inverse member full UID/order differs");}
    }
}
Json grouping(Context& c,const World& w,const Json& args,bool ungroup,bool& attempted){
    const auto& input=args.at("handles");need(input.is_array() && input.size()>=(ungroup?1u:2u) && input.size()<=4096,"Scenery grouping handles count is outside the native operation bounds");
    const auto before=groups(c,w);const auto undo_before=array(c,w.undo+8,20);const auto original_groups=group_pointers(before);std::vector<uintptr_t> targets,added,removed;std::vector<Group> removed_rows;std::set<uintptr_t> unique;uintptr_t destination=0,region=0;
    Json props=Json::array();for(const auto& h:input){const auto p=c.resolve(h);need(unique.insert(p).second,"Scenery grouping handles repeat");const auto found=std::find_if(before.begin(),before.end(),[&](const auto& row){return row.pointer==p;});
        if(found!=before.end()){if(!ungroup && !destination){destination=p;region=found->region;}else{removed.push_back(p);removed_rows.push_back(*found);}for(size_t n=0;n<found->members.size();++n){const auto child=found->members[n];need(c.read<uint64_t>(child+0x18)<=UINT32_MAX,"Scenery original inverse cannot represent this member UID");props.push_back(found->value.at("members").at(n));if(ungroup || p!=destination)added.push_back(child);}}
        else{need(!ungroup,"Scenery Ungroup requires native Group handles");auto value=prop_json(c,w,p,0);need(c.read<uint64_t>(p+0x18)<=UINT32_MAX,"Scenery original inverse cannot represent this member UID");if(!region)region=c.read<uintptr_t>(p+0x178);added.push_back(p);props.push_back(std::move(value));}targets.push_back(p);}
    need(!added.empty(),"Scenery grouping has no affected members");
    const auto cursor=c.singleton(0x13df9d0);need(cursor && c.read<uintptr_t>(cursor)==c.image+0x1786dd0,"Scenery grouping Cursor class differs");const auto behaviour=c.read<uintptr_t>(cursor+0x190);need(behaviour && c.read<uint8_t>(behaviour+0x120)==1,"Scenery grouping current native tool does not permit selection");
    const auto filter=c.read<std::array<uint8_t,9>>(w.selection+0xc8);for(auto b:filter)need(b<=1,"Scenery grouping native selection filter is invalid");for(uintptr_t rva:{0x2d5910,0x2d58f0,0x2d7380,0x2d5d10,0x278bb0,0x4cf6e0,0x4d0140,0x4cf170})c.verify_original(rva);
    std::string failure;Json result;bool filter_changed=false,provisional=false,entered=false;attempted=true;
    try{
        if(!ungroup && !destination){seh(create_native(c.image,w.groups,w.root,region,destination),"create explicitly rooted Group");need(destination && c.resolve(handle(c,destination),0x1749de0)==destination && c.read<uintptr_t>(destination+0x170)==w.root && c.read<uintptr_t>(destination+0x178)==region && array(c,destination+0x210).empty(),"Scenery new empty Group owner/full UID differs");provisional=true;targets.insert(targets.begin(),destination);}
        std::array<uint8_t,9> all;all.fill(1);filter_changed=true;seh(filter_native(c.image,w.selection,&all),"all-category Selection filter");const NativeArray selection{c.image+0x1748c50,targets.data(),int32_t(targets.size()),int32_t(targets.size())};seh(select_native(c.image,w.selection,&selection),"bind grouping Selection");need(array(c,w.selection+0x20)==targets && array(c,w.selection+0x38)==targets,"Scenery original grouping Selection differs");
        entered=true;seh(command_native(c.image,w.selection,ungroup),"whole GroupSelection/UngroupSelection");need(c.read<uintptr_t>(w.shards+0x60)==w.root,"Scenery active world changed during grouping");const auto after=groups(c,world(c,true));auto expected=original_groups;for(auto p:removed)std::erase(expected,p);if(!ungroup && std::find(expected.begin(),expected.end(),destination)==expected.end())expected.push_back(destination);need(group_pointers(after)==expected,"Scenery grouping surviving Group roster differs");
        for(const auto& old:before)if(old.pointer!=destination && std::find(removed.begin(),removed.end(),old.pointer)==removed.end()){const auto current=std::find_if(after.begin(),after.end(),[&](const auto& row){return row.pointer==old.pointer;});need(current!=after.end() && current->value==old.value,"Scenery grouping changed another Group");}
        for(const auto& old:props){const auto p=c.resolve(old.at("handle"),0x16c1390);need(Json(pose(c,p))==old.at("pose") && c.read<int32_t>(c.read<uintptr_t>(p+0x178)+0x834)==old.at("region"),"Scenery grouping changed a member pose/Region");need(c.read<uintptr_t>(p+0x180)==(ungroup?0:destination),"Scenery grouping member reciprocal parent readback differs");}
        if(ungroup){const auto graph=committed(c,w,undo_before,removed.size());inverse(c,graph,true,removed_rows);result={{"ungrouped",removed.size()},{"members",props}};}
        else{const auto current=std::find_if(after.begin(),after.end(),[&](const auto& row){return row.pointer==destination;});need(current!=after.end(),"Scenery original grouped result is absent");auto members=added;const auto prior=std::find_if(before.begin(),before.end(),[&](const auto& row){return row.pointer==destination;});if(prior!=before.end())members.insert(members.begin(),prior->members.begin(),prior->members.end());need(current->members==members,"Scenery original Group member order differs");const auto graph=committed(c,w,undo_before,1);auto inverse_target=*current;inverse_target.members=added;inverse_target.value["members"]=Json::array();for(auto p:added){const auto index=size_t(std::find(current->members.begin(),current->members.end(),p)-current->members.begin());inverse_target.value["members"].push_back(current->value.at("members").at(index));}inverse(c,graph,false,{inverse_target});result=current->value;}
    }catch(const std::exception& e){failure=e.what();}
    if(!failure.empty() && provisional && !entered){try{need(world(c,true).root==w.root && c.resolve(handle(c,destination),0x1749de0)==destination && array(c,destination+0x210).empty(),"Scenery failed preparation Group is no longer the original empty owner");seh(empty_group_native(c.image,destination),"empty preparation Group cleanup");need(group_pointers(groups(c,w))==original_groups,"Scenery empty preparation cleanup Group roster differs");}catch(const std::exception& e){failure+="; cleanup: "+std::string(e.what());}}
    if(filter_changed){try{seh(filter_native(c.image,w.selection,&filter),"restore grouping Selection filter");need(c.read<std::array<uint8_t,9>>(w.selection+0xc8)==filter,"Scenery grouping filter restoration readback differs");}catch(const std::exception& e){failure+=(failure.empty()?"":"; ")+std::string("cleanup: ")+e.what();}}
    need(failure.empty(),failure);return result;
}
std::map<uintptr_t,Json> scenery_census(Context& c,const World& w){
    std::map<uintptr_t,Json> result;for(auto region:w.regions){const auto manager=c.read<uintptr_t>(region+0x960);need(manager && c.read<uintptr_t>(manager)==c.image+0x17152d0 && c.read<uintptr_t>(manager+0x900)==region && c.read<uintptr_t>(manager+0x908)==w.root,"Scenery census manager reciprocity differs");
        for(size_t category=0;category<47;++category)for(auto p:array(c,manager+0x18+category*0x30)){need(c.read<int32_t>(p+0x268)==int32_t(category),"Scenery census category differs");auto value=prop_json(c,w,p,c.read<uintptr_t>(p+0x180));need(result.emplace(p,std::move(value)).second && result.size()<=1000000,"Scenery census member repeats or exceeds bound");}}
    return result;
}
template<size_t N>std::array<float,N> numbers(const Json& input,const char* field){
    need(input.is_array() && input.size()==N,std::string("Scenery ")+field+" requires exactly "+std::to_string(N)+" numbers");std::array<float,N> out{};for(size_t i=0;i<N;++i){need(input[i].is_number(),std::string("Scenery ")+field+" contains a non-number");out[i]=input[i].get<float>();need(std::isfinite(out[i]),std::string("Scenery ")+field+" contains a non-finite number");}return out;
}
std::array<float,10> requested_pose(const Json& args,const std::array<float,10>& before){
    auto out=before;const auto position=numbers<3>(args.at("position"),"position");std::copy(position.begin(),position.end(),out.begin()+4);
    if(args.contains("rotation")){const auto rotation=numbers<4>(args.at("rotation"),"rotation");std::copy(rotation.begin(),rotation.end(),out.begin());}
    if(args.contains("scale")){const auto scale=numbers<3>(args.at("scale"),"scale");std::copy(scale.begin(),scale.end(),out.begin()+7);}
    float norm=0;for(size_t i=0;i<4;++i)norm+=out[i]*out[i];need(std::abs(norm-1)<0.002f,"Scenery rotation quaternion must be normalized");for(size_t i=7;i<10;++i)need(out[i]>0,"Scenery scale must be positive");return out;
}
struct MoveReceipt {uintptr_t graph=0;bool label=false,begun=false,opened=false,constructed=false,moved=false;uint32_t move_cleanup=0,close_cleanup=0,end_cleanup=0,args_cleanup=0,label_cleanup=0;};
uint32_t move_native(uintptr_t image,uintptr_t target,const std::array<float,10>* pose,MoveReceipt& r){
    alignas(16) unsigned char transform[0x6c]{},label[0x38]{},operation[0x130]{};uint32_t code=0;
    __try{std::memcpy(transform,pose->data(),sizeof(*pose));transform[0x28]=1;
        reinterpret_cast<void(*)(void*,const char*)>(image+0xa16f10)(label,"Move");r.label=true;reinterpret_cast<void(*)(const void*)>(image+0x265300)(label);r.begun=true;
        r.graph=reinterpret_cast<uintptr_t(*)(const void*)>(image+0x539990)(label);r.opened=true;if(!r.graph)code=ERROR_INVALID_HANDLE;
        else{reinterpret_cast<void(*)(void*,uintptr_t,const void*)>(image+0x55e8e0)(operation,target,transform);r.constructed=true;reinterpret_cast<void(*)(void*,uintptr_t)>(image+0x55ed80)(operation,r.graph);r.moved=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(r.constructed){__try{reinterpret_cast<void(*)(void*)>(image+0xcf1600)(operation);}__except(EXCEPTION_EXECUTE_HANDLER){r.move_cleanup=GetExceptionCode();}}
    if(r.opened && r.graph){__try{reinterpret_cast<void(*)()>(image+0x5379c0)();}__except(EXCEPTION_EXECUTE_HANDLER){r.close_cleanup=GetExceptionCode();}}
    if(r.begun){__try{reinterpret_cast<void(*)()>(image+0x265650)();}__except(EXCEPTION_EXECUTE_HANDLER){r.end_cleanup=GetExceptionCode();}}
    if(r.label){__try{reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}}return code;
}
uint32_t destination_native(uintptr_t image,uintptr_t root,const std::array<float,10>* pose,uintptr_t& region){
    __try{region=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*)>(image+0x508a40)(root,pose->data()+4);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t destroy_native(uintptr_t image,uintptr_t manager,uintptr_t target){__try{reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x2d5520)(manager,target);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
Json control_group(Context& c,const World& w,const Json& args,bool destroying,bool& attempted){
    const auto before=groups(c,w);const auto target=c.resolve(args.at("group"),0x1749de0);const auto selected=std::find_if(before.begin(),before.end(),[&](const auto& row){return row.pointer==target;});need(selected!=before.end(),"Scenery Group is absent from the current root world");
    const auto props_before=scenery_census(c,w);const auto undo_before=array(c,w.undo+8,20);const auto prior_pose=selected->value.at("pose").get<std::array<float,10>>();const auto wanted=destroying?prior_pose:requested_pose(args,prior_pose);
    for(auto [slot,rva]:std::initializer_list<std::pair<uintptr_t,uintptr_t>>{{0xd0,0xe47100},{0x100,0x4ceaa0},{0x120,0xe3f480},{0x278,0x4ceb00},{0x280,0xe3f3b0},{0x2a0,0x4ccaf0}})need(c.read<uintptr_t>(c.image+0x1749de0+slot)==c.image+rva,"Scenery Group original movement/member virtual method differs");
    uintptr_t graph=0;
    if(destroying){c.verify_original(0x2d5520);c.verify_original(0x2d42c0);attempted=true;seh(destroy_native(c.image,w.selection,target),"whole Group DestroyProp");graph=committed(c,w,undo_before,SIZE_MAX);}
    else{
        for(uintptr_t rva:{0x508a40,0xe47100,0x4ceaa0,0x4ccaf0,0xa16f10,0x265300,0x539990,0x55e8e0,0x55ed80,0xcf1600,0x5379c0,0x265650,0x11b2b00,0x11bd1e0})c.verify_original(rva);
        uintptr_t region=0;seh(destination_native(c.image,w.root,&wanted,region),"Group destination Region");need(std::find(w.regions.begin(),w.regions.end(),region)!=w.regions.end(),"Scenery Group destination has no current root Region");
        MoveReceipt r;attempted=true;const auto code=move_native(c.image,target,&wanted,r);need(!code && r.moved && !r.move_cleanup && !r.close_cleanup && !r.end_cleanup && !r.args_cleanup && !r.label_cleanup,"Scenery Group Move failed SEH="+std::to_string(code)+" move_cleanup="+std::to_string(r.move_cleanup)+" close_cleanup="+std::to_string(r.close_cleanup)+" end_cleanup="+std::to_string(r.end_cleanup)+" args_cleanup="+std::to_string(r.args_cleanup)+" label_cleanup="+std::to_string(r.label_cleanup)+"; do not replay");
        graph=committed(c,w,undo_before,1);need(graph==r.graph,"Scenery Group Move committed another graph");const auto inverse=array(c,graph+0x18);const auto operation=inverse.at(0);need(c.read<uintptr_t>(operation)==c.image+0x1786ff0 && c.read<uint64_t>(operation+0xb8)==selected->uid && pose(c,operation,0xc0)==prior_pose,"Scenery Group Move inverse class/full UID/prior pose differs");
    }
    const auto current=world(c,true);const auto after=groups(c,current);const auto props_after=scenery_census(c,current);auto expected_groups=group_pointers(before);if(destroying)std::erase(expected_groups,target);need(group_pointers(after)==expected_groups,"Scenery Group control changed surviving Group roster");
    for(const auto& old:before)if(old.pointer!=target){const auto found=std::find_if(after.begin(),after.end(),[&](const auto& row){return row.pointer==old.pointer;});need(found!=after.end() && found->value==old.value,"Scenery Group control changed another Group");}
    for(const auto& [p,value]:props_before)if(std::find(selected->members.begin(),selected->members.end(),p)==selected->members.end()){const auto found=props_after.find(p);need(found!=props_after.end() && found->second==value,"Scenery Group control changed another Scenery member");}
    if(destroying){need(props_after.size()+selected->members.size()==props_before.size(),"Scenery Group Destroy retained or created Scenery");for(auto p:selected->members)need(!props_after.contains(p),"Scenery Group Destroy retained an owned member");return {{"destroyed",true},{"members",selected->members.size()}};}
    need(props_after.size()==props_before.size(),"Scenery Group Move changed Scenery roster");const auto moved=std::find_if(after.begin(),after.end(),[&](const auto& row){return row.pointer==target;});need(moved!=after.end() && moved->members==selected->members && moved->value.at("pose")==Json(wanted),"Scenery Group Move authored pose/member identity differs");
    for(size_t i=0;i<moved->members.size();++i)need(moved->value.at("members").at(i).at("relative_pose")==selected->value.at("members").at(i).at("relative_pose"),"Scenery Group Move changed a member relative pose");return moved->value;
}
#include "duplicate.inc"
#include "batch_move.inc"
#include "stroke.inc"
}
std::optional<Json> query(Context& c,const std::string& operation,const Json&){
    if(operation!="scenery.groups" && operation!="scenery.list")return std::nullopt;const auto w=world(c,false);Json result=Json::array();if(operation=="scenery.list"){for(const auto& [p,row]:scenery_census(c,w)){(void)p;result.push_back(row);}}else for(const auto& row:groups(c,w))result.push_back(row.value);return result;
}
bool action(Context& c,const std::string& operation,const Json& args,bool& attempted,Json& result,std::string& cause){
    if(operation!="scenery.group" && operation!="scenery.ungroup" && operation!="scenery.move_group" && operation!="scenery.destroy_group" && operation!="scenery.duplicate" && operation!="scenery.transform" && operation!="scenery.stroke")throw std::invalid_argument("Unknown Scenery action: "+operation);
    try{const auto w=world(c,true);if(operation=="scenery.stroke")result=stroke(c,w,args,attempted);else if(operation=="scenery.transform")result=transform_batch(c,w,args,attempted);else if(operation=="scenery.duplicate")result=duplicate(c,w,args,attempted);else if(operation=="scenery.move_group" || operation=="scenery.destroy_group")result=control_group(c,w,args,operation=="scenery.destroy_group",attempted);else result=grouping(c,w,args,operation=="scenery.ungroup",attempted);cause.clear();return true;}catch(const std::exception& e){result=Json();cause=e.what();return false;}
}
}
