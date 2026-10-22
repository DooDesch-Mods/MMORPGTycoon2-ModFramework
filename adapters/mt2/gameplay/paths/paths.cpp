// Copyright (c) 2026 DooDesch. Original standalone adapter under the MIT license.
// Native ABI facts: Multiplayer mixed_path_reader.h, path_execution.inc,
// world_path_replay.inc, world_path_remove.inc and world_path_wall_native.inc.
#include "paths.h"
#include "../dungeons/validation.h"
#include "../dungeons/undo_commit.h"
#include "history_transition.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace mt2fw::mt2::gameplay::paths {
namespace {
using Point=std::array<float,3>;
using Spline=std::array<float,12>;
using dungeons::detail::integer;
using dungeons::detail::numbers;
void need(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
void seh(uint32_t code,const char* operation){need(!code,std::string("Path native ")+operation+" failed SEH="+std::to_string(code)+"; do not replay");}
struct Text {const char* data;uint64_t size;uint64_t storage[2]{};};
Json handle(Context& c,uintptr_t p){return {{"epoch",c.epoch},{"uid",std::to_string(c.read<uint64_t>(p+0x18))}};}
template<class T>std::vector<T> span(Context& c,uintptr_t data_field,uintptr_t count_field,uintptr_t capacity_field,size_t limit){
    const auto data=c.read<uintptr_t>(data_field);const auto count=c.read<int32_t>(count_field),capacity=c.read<int32_t>(capacity_field);
    need(count>=0 && size_t(count)<=limit && capacity>=count && (!count || (data && size_t(count)<=(UINTPTR_MAX-data)/sizeof(T))),"Path native array bounds differ");
    std::vector<T> out(static_cast<size_t>(count));if(count)c.read_bytes(data,out.data(),out.size()*sizeof(T));need(c.read<uintptr_t>(data_field)==data && c.read<int32_t>(count_field)==count && c.read<int32_t>(capacity_field)==capacity,"Path native array changed during read");return out;
}
std::vector<uintptr_t> array(Context& c,uintptr_t header,size_t limit=1000000){return span<uintptr_t>(c,header+8,header+16,header+20,limit);}
void finite(const Point& p){for(float v:p)need(std::isfinite(v),"Path coordinate is non-finite");}
struct Region {uintptr_t pointer=0,manager=0;int32_t id=-1;bool owned=false;};
struct Node {uintptr_t pointer=0,region=0;uint64_t uid=0;int32_t type=-1;Point position{};std::vector<uintptr_t> edges;};
struct Path {uintptr_t pointer=0,region=0,second=0,start=0,end=0;uint64_t uid=0;int32_t type=-1;std::string name;std::vector<Point> points,walks;std::vector<float> distances,walk_distances;};
struct Remote {uintptr_t region=0,path=0;int32_t type=-1;};
struct Census {uintptr_t map=0,root=0;std::vector<Region> regions;std::vector<Path> paths;std::vector<Node> nodes;std::vector<Remote> remote;};
Census census(Context& c){
    c.require_world();Census out;out.map=c.singleton(0x13df8c0);need(out.map && c.read<uintptr_t>(out.map)==c.image+0x1742860,"Path WorldMap class differs");out.root=out.map+0x1c0;
    need(c.read<uintptr_t>(out.root)==c.image+0x174ad20 && c.read<int32_t>(out.root+0x48)==0 && c.read<uintptr_t>(out.map+0x230)==c.image+0x16f04e0,"Path saved root/Region table class differs");
    std::set<uintptr_t> region_ids;std::set<int32_t> ids;for(auto p:array(c,out.map+0x230,4096)){Region row;row.pointer=p;row.id=c.read<int32_t>(p+0x834);row.manager=c.read<uintptr_t>(p+0x9a0);const auto owned=c.read<uint8_t>(p+0x850);
        need(p && region_ids.insert(p).second && ids.insert(row.id).second && row.id>=0 && owned<=1 && c.read<uintptr_t>(p)==c.image+0x1787930 && c.read<uintptr_t>(p+0x70)==out.root && row.manager && c.read<uintptr_t>(row.manager+0x6a0)==p,"Path Region/manager reciprocal owner differs");row.owned=owned!=0;out.regions.push_back(row);}
    need(!out.regions.empty(),"Path saved World Region table is empty");std::set<uintptr_t> all;std::set<uint64_t> uids;
    for(const auto& r:out.regions){
        for(int32_t type=0;type<11;++type){
            for(auto p:span<uintptr_t>(c,r.manager+0x28+size_t(type)*0x30,r.manager+0x30+size_t(type)*0x30,r.manager+0x34+size_t(type)*0x30,1000000)){
                Path row;row.pointer=p;row.region=r.pointer;row.type=type;row.uid=c.read<uint64_t>(p+0x18);row.second=c.read<uintptr_t>(p+0x2f8);row.start=c.read<uintptr_t>(p+0x268);row.end=c.read<uintptr_t>(p+0x270);
                need(all.insert(p).second && uids.insert(row.uid).second && c.resolve(handle(c,p),0x1743f10)==p && c.read<uintptr_t>(p+0x170)==out.root && c.read<uintptr_t>(p+0x178)==r.pointer && c.read<int32_t>(p+0x21c)==type && !c.read<uint8_t>(p+0x46e),"Path native object full UID/type/root/Region/preview differs");
                row.name=c.text(p+0x190);row.points=span<Point>(c,p+0x298,p+0x2a0,p+0x2a4,65536);row.distances=span<float>(c,p+0x2b0,p+0x2b8,p+0x2bc,65536);
                if(type==6){row.walks=span<Point>(c,p+0x2c8,p+0x2d0,p+0x2d4,65536);row.walk_distances=span<float>(c,p+0x2e0,p+0x2e8,p+0x2ec,65536);need(row.walks.size()>=2,"Path Bridge native walkpoints are absent");}
                need(row.points.size()>=2 && (!row.second || (row.second!=r.pointer && region_ids.contains(row.second))),"Path native waypoints/second Region differ");for(const auto& points:{row.points,row.walks})for(auto point:points)finite(point);for(const auto& values:{row.distances,row.walk_distances})for(float value:values)need(std::isfinite(value),"Path native distance is non-finite");out.paths.push_back(std::move(row));need(out.paths.size()+out.nodes.size()<=1000000,"Path complete native graph exceeds member bound");
            }
            auto remote=span<uintptr_t>(c,r.manager+0x370+size_t(type)*0x48,r.manager+0x378+size_t(type)*0x48,r.manager+0x37c+size_t(type)*0x48,1000000);std::set<uintptr_t> unique;for(auto p:remote){need(p && unique.insert(p).second,"Path remote bucket member is absent or repeated");out.remote.push_back({r.pointer,p,type});need(out.remote.size()<=1000000,"Path complete remote membership exceeds its bound");}
        }
        for(int32_t type=0;type<6;++type)for(auto p:span<uintptr_t>(c,r.manager+0x238+size_t(type)*0x30,r.manager+0x240+size_t(type)*0x30,r.manager+0x244+size_t(type)*0x30,1000000)){
            Node row;row.pointer=p;row.region=r.pointer;row.type=type;row.uid=c.read<uint64_t>(p+0x18);row.position=c.read<Point>(p+0x110);row.edges=span<uintptr_t>(c,p+0x2c8,p+0x2d0,p+0x2d4,65536);finite(row.position);
            need(all.insert(p).second && uids.insert(row.uid).second && c.resolve(handle(c,p),0x1700a10)==p && c.read<uintptr_t>(p+0x170)==out.root && c.read<uintptr_t>(p+0x178)==r.pointer && c.read<int32_t>(p+0x298)==type,"Path Intersection full UID/type/root/Region differs");out.nodes.push_back(std::move(row));need(out.paths.size()+out.nodes.size()<=1000000,"Path complete native graph exceeds member bound");
        }
    }
    std::map<uintptr_t,const Path*> paths;std::map<uintptr_t,const Node*> nodes;std::map<uintptr_t,std::set<uintptr_t>> expected;for(const auto& p:out.paths)paths.emplace(p.pointer,&p);for(const auto& n:out.nodes)nodes.emplace(n.pointer,&n);
    for(const auto& p:out.paths)for(auto n:{p.start,p.end})if(n){need(nodes.contains(n) && (nodes.at(n)->region==p.region || nodes.at(n)->region==p.second),"Path endpoint is absent or belongs to another Region");expected[n].insert(p.pointer);}
    for(const auto& n:out.nodes){std::set<uintptr_t> edges;for(auto p:n.edges)need(paths.contains(p) && edges.insert(p).second,"Path Intersection has a foreign/duplicate edge");need(edges==expected[n.pointer],"Path Intersection adjacency is not reciprocal");}
    std::set<std::pair<uintptr_t,uintptr_t>> remote;for(auto entry:out.remote){need(remote.insert({entry.region,entry.path}).second && paths.contains(entry.path) && paths.at(entry.path)->second==entry.region && paths.at(entry.path)->type==entry.type,"Path remote membership is not reciprocal");}for(const auto& p:out.paths)need(!p.second || remote.contains({p.second,p.pointer}),"Path second Region lacks remote registration");return out;
}
int32_t region_id(const Census& all,uintptr_t p){for(const auto& r:all.regions)if(r.pointer==p)return r.id;throw std::runtime_error("Path Region is absent from current saved World");}
const Region& region(Context&,const Census& all,const Json& value,bool owned){const auto id=integer(value,0,INT32_MAX,"Path Region ID");for(const auto& row:all.regions)if(row.id==id){need(!owned || row.owned,"Path action requires an owned native Region");return row;}throw std::runtime_error("Path requested Region ID is absent");}
Json node_json(Context& c,const Census& all,const Node& n){Json edges=Json::array();for(auto p:n.edges)edges.push_back(handle(c,p));return {{"handle",handle(c,n.pointer)},{"region",region_id(all,n.region)},{"type",n.type},{"position",n.position},{"edges",edges}};}
Json path_json(Context& c,const Census& all,const Path& p){return {{"handle",handle(c,p.pointer)},{"region",region_id(all,p.region)},{"second_region",p.second?Json(region_id(all,p.second)):Json()},{"type",p.type},{"name",p.name},{"start",p.start?handle(c,p.start):Json()},{"end",p.end?handle(c,p.end):Json()},{"points",p.points},{"distances",p.distances},{"walkpoints",p.walks},{"walk_distances",p.walk_distances}};}
Json graph_json(Context& c,const Census& all){Json paths=Json::array(),nodes=Json::array();for(const auto& p:all.paths)paths.push_back(path_json(c,all,p));for(const auto& n:all.nodes)nodes.push_back(node_json(c,all,n));return {{"paths",paths},{"nodes",nodes}};}
uintptr_t idle(Context& c,const Census& all){const auto manager=c.singleton(0x13df440),undo=c.singleton(0x13df380);need(manager && c.read<uintptr_t>(manager)==c.image+0x1702b00 && c.read<uintptr_t>(manager+0x60)==all.root,"Path action requires the root world to be the actual native active scene");need(undo && c.read<uintptr_t>(undo)==c.image+0x16fe070 && c.read<uintptr_t>(undo+8)==c.image+0x16e1320 && !c.read<uintptr_t>(c.image+0x1a3aa80),"Path action requires idle native Undo");return undo;}
uintptr_t committed(Context& c,uintptr_t undo,const std::vector<uintptr_t>& before){const auto after=array(c,undo+8,20);need(!after.empty(),"Path original Undo graph is absent");const auto graph=after.back();need(dungeons::detail::committed_history(before,after,array(c,undo+0x20,20).empty(),c.read<uint8_t>(graph+0x10c),c.read<int32_t>(graph+0x58),c.read<uintptr_t>(c.image+0x1a3aa80)) && c.read<uintptr_t>(graph)==c.image+0x16e32d0 && c.read<uintptr_t>(graph+0x18)==c.image+0x16e2dc0,"Path original locked Undo graph/history differs");return graph;}
uintptr_t endpoint(Context& c,const Census& all,const Json& args,const char* key,const Region& r,Point& position,int32_t type=-1){
    if(!args.contains(key))return 0;const auto p=c.resolve(args.at(key),0x1700a10);for(const auto& n:all.nodes)if(n.pointer==p){need(n.region==r.pointer && (type<0 || n.type==type),"Path endpoint native Region/type differs");position=n.position;return p;}throw std::runtime_error("Path endpoint Intersection is absent from the current World");
}
void unchanged_paths(const Census& before,const Census& after,const std::set<uintptr_t>& excluded={}){
    for(const auto& p:before.paths)if(!excluded.contains(p.pointer)){const auto found=std::find_if(after.paths.begin(),after.paths.end(),[&](const auto& row){return row.pointer==p.pointer;});need(found!=after.paths.end() && found->uid==p.uid && found->region==p.region && found->second==p.second && found->type==p.type && found->name==p.name && found->start==p.start && found->end==p.end && found->points==p.points && found->distances==p.distances && found->walks==p.walks && found->walk_distances==p.walk_distances,"Path transaction changed an unrelated native Path");}
}
#include "road.inc"
#include "road_route.inc"
#include "remove.inc"
#include "link.inc"
#include "wall.inc"
#include "wall_curve.inc"
#include "history.inc"
}
std::optional<Json> query(Context& c,const std::string& operation,const Json&){if(operation!="paths.graph")return std::nullopt;return graph_json(c,census(c));}
bool action(Context& c,const std::string& operation,const Json& args,bool& attempted,Json& result,std::string& cause){
    if(operation!="paths.place" && operation!="paths.place_route" && operation!="paths.remove" && operation!="paths.link" && operation!="walls.place" && operation!="walls.place_curve" && operation!="paths.undo" && operation!="paths.redo")throw std::invalid_argument("Unknown Path action: "+operation);
    try{const auto before=census(c);const auto undo=idle(c,before);if(operation=="paths.undo" || operation=="paths.redo")result=apply_history(c,before,undo,args,operation=="paths.redo",attempted);else{const auto snapshot=canonical(c,before);const auto stack=array(c,undo+8,20);if(operation=="paths.remove")result=remove_path(c,before,undo,args,attempted);else if(operation=="paths.link")result=link_path(c,before,undo,args,attempted);else if(operation=="walls.place_curve")result=place_wall_curve(c,before,undo,args,attempted);else if(operation=="walls.place")result=place_wall(c,before,undo,args,attempted);else if(operation=="paths.place_route")result=place_road_route(c,before,undo,args,attempted);else result=place_road(c,before,undo,args,attempted);const auto after=census(c);result["receipt"]=remember(c,before.root,undo,committed(c,undo,stack),snapshot,canonical(c,after));}cause.clear();return true;}catch(const std::exception& e){result=Json();cause=e.what();return false;}
}
}
