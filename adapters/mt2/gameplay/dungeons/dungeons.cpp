// Copyright (c) 2026 DooDesch. Original standalone adapter under the MIT license.
// Native ABI facts: Multiplayer src/game/dungeon_* readers, calls and code pins.
#include "dungeons.h"
#include "outcome.h"
#include "validation.h"
#include "fill_records.h"
#include "undo_commit.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <intrin.h>
#include <set>
#include <stdexcept>
#include <vector>

namespace mt2fw::mt2::gameplay::dungeons {
namespace {
using detail::integer;
using detail::integers;
using detail::numbers;
using detail::color_input;
void need(bool condition, const std::string& cause) { if (!condition) throw std::runtime_error(cause); }
void seh(uint32_t code, const char* operation) {
    need(code == 0, std::string("Dungeon native ") + operation + " failed SEH=" + std::to_string(code) + "; do not replay");
}
struct Pin {uintptr_t rva; size_t size; const char* sha;};
void pin(Context& c, Pin p) {
    c.verify_hash(p.rva,p.size,p.sha);
}
struct Text {const char* data; uint64_t size; uint64_t capacity=0, padding=0;};
uintptr_t object(Context& c,const Json& handle,uintptr_t ap) {
    return c.resolve(handle,ap);
}
Json handle(Context& c,uintptr_t p) {return {{"epoch",c.epoch},{"uid",std::to_string(c.read<uint64_t>(p+0x18))}};}
struct Owner {uintptr_t exterior=0,dungeon=0,shard=0,region=0;};
Owner owner(Context& c,const Json& args) {
    c.require_world(); Owner out;out.exterior=object(c,args.at("handle"),0x16df760);
    const auto map=c.singleton(0x13eb8c0);need(map && c.read<uintptr_t>(map)==c.image+0x1748c00,"Dungeon current WorldMap class differs");
    need(c.read<uintptr_t>(out.exterior+0x170)==map+0x1c0,"Dungeon exterior does not belong to the current root world");
    out.dungeon=c.read<uintptr_t>(out.exterior+0xa38);
    need(out.dungeon && c.read<uintptr_t>(out.dungeon)==c.image+0x16c6900 && c.read<uintptr_t>(out.dungeon+0x6f8)==out.exterior,"Dungeon reciprocal exterior owner differs");
    out.shard=c.read<uintptr_t>(out.dungeon+0x48);out.region=c.read<uintptr_t>(out.dungeon+0x50);
    need(out.shard && out.region && c.read<uintptr_t>(out.shard)==c.image+0x17510c0 && c.read<int32_t>(out.shard+0x48)==1 &&
         c.read<uintptr_t>(out.shard+0x18)==out.dungeon && c.read<uintptr_t>(out.region)==c.image+0x178dcd0 && c.read<uintptr_t>(out.region+0x70)==out.shard,
         "Dungeon template Shard/Region ownership differs");return out;
}
std::vector<uintptr_t> array(Context& c,uintptr_t p,size_t bound=100000) {
    const auto data=c.read<uintptr_t>(p+8);const auto count=c.read<int32_t>(p+0x10),capacity=c.read<int32_t>(p+0x14);
    need(count>=0 && size_t(count)<=bound && capacity>=count && (!count || (data && size_t(count)<= (UINTPTR_MAX-data)/8)),"Dungeon owned array bounds differ");
    std::vector<uintptr_t> rows;std::set<uintptr_t> seen;
    for(int32_t i=0;i<count;++i){const auto row=c.read<uintptr_t>(data+size_t(i)*8);need(row && seen.insert(row).second,"Dungeon owned array has absent/duplicate member");rows.push_back(row);}
    need(c.read<uintptr_t>(p+8)==data && c.read<int32_t>(p+0x10)==count && c.read<int32_t>(p+0x14)==capacity,"Dungeon owned array changed during read");return rows;
}
struct Instance {uintptr_t pointer=0,shard=0,region=0;int32_t index=-1;bool started=false;};
std::vector<Instance> instances(Context& c,const Owner& o) {
    std::vector<Instance> out;std::set<int32_t> ids;
    for(auto p:array(c,o.dungeon+0x4b8,10000)) {
        Instance row;row.pointer=p;row.index=c.read<int32_t>(p+0x18);const auto started=c.read<uint8_t>(p+0x20);
        row.shard=c.read<uintptr_t>(p+0x38);row.region=c.read<uintptr_t>(p+0x40);
        need(c.read<uintptr_t>(p)==c.image+0x171f7f0 && row.index>=0 && ids.insert(row.index).second && started<=1 && c.read<uintptr_t>(p+0x30)==o.dungeon &&
             row.shard && row.region && c.read<uintptr_t>(row.shard+0x20)==p && c.read<uintptr_t>(row.region+0x70)==row.shard,
             "Dungeon Instance index/owner/Shard/Region differs");row.started=started!=0;out.push_back(row);
    }return out;
}
Json instance_json(const Instance& row) {return {{"index",row.index},{"started",row.started}};}
struct Cell {uintptr_t pointer;bool open;std::string theme;std::array<uint32_t,4> colors;};
struct Layout {std::vector<Cell> cells;std::vector<uint32_t> palette;};
Layout layout(Context& c,const Owner& o) {
    Layout out;need(c.read<uintptr_t>(o.dungeon+0x108)==c.image+0x170ca30,"Dungeon Layout class differs");
    const auto slots=array(c,o.dungeon+0x120);need(size_t(c.read<int32_t>(o.dungeon+0x80))==slots.size(),"Dungeon grid/layout cell counts differ");
    for(auto p:slots){const auto open=c.read<uint8_t>(p+0x60);need(c.read<uintptr_t>(p)==c.image+0x1729d30 && c.read<uintptr_t>(p+0x38)==c.image+0x173dc50 && open<=1,"Dungeon layout Cell/Data class differs");
        out.cells.push_back({p,open!=0,c.text(p+0x18),c.read<std::array<uint32_t,4>>(p+0x50)});}
    const auto data=c.read<uintptr_t>(o.dungeon+0x1c0);const auto count=c.read<int32_t>(o.dungeon+0x1c8),capacity=c.read<int32_t>(o.dungeon+0x1cc);
    need(data && count>=4 && count<=256 && capacity>=count,"Dungeon palette bounds differ");for(int32_t i=0;i<count;++i)out.palette.push_back(c.read<uint32_t>(data+size_t(i)*4));return out;
}
Json layout_json(const Layout& value) {
    Json cells=Json::array();for(size_t i=0;i<value.cells.size();++i){const auto& row=value.cells[i];cells.push_back({{"index",i},{"open",row.open},{"theme",row.theme},{"colors",row.colors}});}return {{"cells",cells},{"palette",value.palette}};
}
uintptr_t scene_region(Context& c,const Owner& o,const Json& args,uintptr_t& shard) {
    shard=o.shard;if(!args.contains("instance"))return o.region;
    const auto index=integer(args.at("instance"),0,INT32_MAX,"Instance index");for(const auto& row:instances(c,o))if(row.index==index){shard=row.shard;return row.region;}
    throw std::runtime_error("Dungeon Instance saved index is absent");
}
std::vector<uintptr_t> gizmos(Context& c,const Owner& o,const Json& args) {
    uintptr_t shard=0;const auto region=scene_region(c,o,args,shard);const auto manager=c.read<uintptr_t>(region+0x9a8);
    need(manager && c.read<uintptr_t>(manager)==c.image+0x1706600 && c.read<uintptr_t>(manager+0x68)==shard && c.read<uintptr_t>(manager+0x70)==region,"Dungeon Gizmo manager scene ownership differs");
    auto rows=array(c,manager+0x18);for(auto p:rows){need(c.read<uintptr_t>(p)==c.image+0x174fe50 && c.read<uintptr_t>(p+0x170)==shard && c.read<uintptr_t>(p+0x178)==region,"Dungeon Gizmo scene membership differs");
        need(object(c,handle(c,p),0x174fe50)==p,"Dungeon Gizmo original UID registry differs");}return rows;
}
std::vector<uintptr_t> monsters(Context& c,const Owner& o,const Json& args) {
    uintptr_t shard=0;const auto region=scene_region(c,o,args,shard),manager=c.read<uintptr_t>(region+0x988);
    need(manager && c.read<uintptr_t>(manager)==c.image+0x171a4a0 && c.read<uintptr_t>(manager+0x80)==region,"Dungeon Monster manager reciprocal Region differs");
    const auto rows=array(c,manager+0x38);for(auto p:rows)need(c.read<uintptr_t>(p)==c.image+0x16c6b10 && c.read<uintptr_t>(p+0x170)==shard && c.read<uintptr_t>(p+0x178)==region && object(c,handle(c,p),0x16c6b10)==p,"Dungeon Monster class/full UID/scene membership differs");return rows;
}
Json monster_json(Context& c,uintptr_t p) {
    return {{"handle",handle(c,p)},{"rank",c.read<int32_t>(p+0x838)},{"character_type",c.read<int32_t>(p+0x4a4)},{"character_kind",c.read<int32_t>(p+0x4a8)},
        {"level",c.read<int32_t>(p+0x6f8)},{"tag",c.read<int32_t>(p+0x820)},{"status",c.read<int32_t>(p+0x824)},
        {"position",c.read<std::array<float,3>>(p+0x110)},{"rotation",c.read<std::array<float,4>>(p+0x100)},{"scale",c.read<std::array<float,3>>(p+0x11c)}};
}
Json gizmo_json(Context& c,uintptr_t p) {
    return {{"handle",handle(c,p)},{"name",c.text(p+0x38)},{"type",c.text(p+0x210)},{"variant",c.text(p+0x230)},
        {"position",c.read<std::array<float,3>>(p+0x110)},{"rotation",c.read<std::array<float,4>>(p+0x100)},{"scale",c.read<std::array<float,3>>(p+0x11c)},
        {"triggered",c.read<uint8_t>(p+0x250)!=0},{"state",c.read<int32_t>(p+0x254)},{"immediate",c.read<int32_t>(p+0x328)},
        {"on_map",c.read<uintptr_t>(p+0x300)!=0},{"gold",c.read<int32_t>(p+0x260)},
        {"loot",c.read<int32_t>(p+0x264)},{"vendor_trash",c.read<int32_t>(p+0x268)},{"keys",c.read<int32_t>(p+0x26c)},{"boss_keys",c.read<int32_t>(p+0x270)},
        {"lock_id",c.read<int32_t>(p+0x330)},{"edge_a",c.read<int32_t>(p+0x334)},{"edge_b",c.read<int32_t>(p+0x338)}};
}
uint32_t window_native(uintptr_t image,const char* name,size_t size,uintptr_t& p) {
    const Text key{name,size};__try {p=reinterpret_cast<uintptr_t(*)(const Text&)>(image+0x823080)(key);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uintptr_t window(Context& c,const char* name,uintptr_t ap) {
    c.verify_original(0x823080,5);
    uintptr_t p=0;seh(window_native(c.image,name,std::strlen(name),p),"Window lookup");need(p && c.read<uintptr_t>(p)==c.image+ap,"Dungeon native named Window class differs");return p;
}
struct CommandReceipt {bool constructed=false,returned=false,accepted=false;uint32_t cleanup=0;};
uint32_t command_native(uintptr_t image,uintptr_t method,uintptr_t receiver,const char* command,CommandReceipt& r) {
    alignas(8) unsigned char record[0x78]{};uint32_t code=0;
    __try {reinterpret_cast<void(*)(void*,const char*)>(image+0x970590)(record,command);r.constructed=true;
        r.accepted=reinterpret_cast<bool(*)(uintptr_t,void*)>(image+method)(receiver,record);r.returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(r.constructed){__try{reinterpret_cast<void(*)(void*)>(image+0x9708c0)(record);}__except(EXCEPTION_EXECUTE_HANDLER){r.cleanup=GetExceptionCode();}}return code;
}
void command(Context& c,uintptr_t method,uintptr_t receiver,const std::string& label,bool& attempted) {
    pin(c,{0x970590,407,"8889b8a130368f7924544d45b67e57d18248bd5a1489a5a700246f2af8609842"});
    pin(c,{0x9708c0,148,"bb4624bbc533d99539535f19391f65e947168a081318a35d4e7eae53082253f9"});
    CommandReceipt r;attempted=true;seh(command_native(c.image,method,receiver,label.c_str(),r),"UI command");seh(r.cleanup,"UI record cleanup");need(r.returned && r.accepted,"Dungeon native UI command did not accept; inspect state before retry");
}
uint32_t config_prepare(uintptr_t image,uintptr_t window,uintptr_t dungeon,int32_t count) {
    __try {reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x6b5ee0)(window,dungeon);*reinterpret_cast<int32_t*>(window+0x508)=count;
        reinterpret_cast<void(*)(uintptr_t)>(image+0x6b4fb0)(window);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t sound_prepare(uintptr_t image,uintptr_t window,uintptr_t dungeon) {
    __try {*reinterpret_cast<uintptr_t*>(window+0x4e8)=dungeon;reinterpret_cast<void(*)(uintptr_t)>(image+0x6dc0f0)(window);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t lifecycle(uintptr_t image,int kind,uintptr_t dungeon,uintptr_t target,uintptr_t& made) {
    __try {
        if(kind==0)made=reinterpret_cast<uintptr_t(*)(uintptr_t)>(image+0xa8370)(dungeon);
        else if(kind==1)reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0xa8700)(dungeon,target);
        else reinterpret_cast<void(*)(uintptr_t)>(image+(kind==2?0xb7840:0xb73e0))(target);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
constexpr Pin instance_pins[]{
    {0xa8370,792,"9f34d7520f3c8a876913ecd07e7dd590ebee992ebbf908b23bbc04081d0c7d22"},
    {0xa8700,1320,"51870dc178c8e46f5b28f552b3ab96b6e6b2af4b64ac5c09b238adf3bc1c27bc"},
    {0xb7840,69,"213645604eed0f7b6676b627d397ca8098a41bd80401b307e9cafd79db8a8276"},
    {0xb73e0,1091,"a8017d81e266a8558b65b1f29ea8a09b0b128cc248cdbdfeeb8c280343576eba"}};
constexpr Pin cell_pins[]{
    {0xa4ef0,117,"34910bae76c457d2c2558bef85d3d8370a59988add43b2bc9f026b182a8fafbb"},
    {0xa5960,244,"c32846c46037a04f01f4bbded9dfeaf11235961de23a25373f77e884cc7a5167"},
    {0xa8ed0,328,"feca29b54756d4785646cca92927490d190683990bbc8977c3adc1f6961e1a95"},
    {0xa9020,373,"d7a4a78c1166a7cd271211658fac7eb1e68ebca6a60d48f4aafa72eef342e626"},
    {0xa91e0,644,"b6710da229548ca3aef132f31fda3af2daa8595a6979237d4f7e6cab3abfa913"},
    {0xa9880,118,"14d855d49f3cd46d37a8d62e4c840b7373b6213cc3d4e57a52b81f3b1bf8516d"},
    {0xcb950,131,"8c1581784748542fb28e3f5c9d6b94353746f6927e7b8b9aaf10f88747f4cbcb"},
    {0x268750,4409,"0a8dce7a41ea7b52a5cfe35484aaa3005e28af3441eabc72a4b2be4f3fa474dd"},
    {0x53dd50,68,"af5e03ae629b473a1bfb27bbba384246c70a1cc237d08248b4191d74f53bff02"},
    {0x541790,649,"c90ed96c3a4cf01412149e64cb20b7c92e53d867ec3bf95ec96477e0e681fab5"},
    {0x54ac40,797,"a405fdfd299416bc2f116e5726e0d307f6953116526aec985e0e5b8b76bba465"},
    {0x5a39e0,350,"bfd9b6d1e5672a65e03f57d2b933b8130126ba854b5494903f529d7d598fcfbd"},
    {0x5a3b40,1711,"7e91c006e3e03054b9e9d66604c027eeb6bf47cfa45ecb65ec6198ea844fd7a1"},
    {0xa21c10,180,"ba10d3a02d35a530d90c715559a1d9105e0957c9d2bfa08e0dd6896b4f5fa7a1"},
    {0x11be4a0,169,"6f1a11a441f1444800a4d051034d4780bc1877837236124d6e8cf3fc954b91d8"},
    {0x11c8b80,25,"8721e2502357123267a8e9510ee755adac5f43e9fdefc725c39cee84c09e2a3d"}};
uint32_t cell_allowed(uintptr_t image,uintptr_t dungeon,int32_t index,bool opening,bool& allowed) {
    __try {
        if(opening)allowed=reinterpret_cast<bool(*)(uintptr_t,int32_t)>(image+0xa4ef0)(dungeon,index);
        else allowed=!reinterpret_cast<bool(*)(uintptr_t,uintptr_t,int32_t,bool)>(image+0x5a3b40)(0,dungeon,index,false);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t theme_native(uintptr_t image,uintptr_t manager,const Text* theme,uintptr_t& result) {
    __try {result=reinterpret_cast<uintptr_t(*)(uintptr_t,const Text&)>(image+0xcb950)(manager,*theme);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct CellReceipt {uintptr_t graph=0,marker=0;bool edit_open=false,active=false,changed=false,committed=false;uint32_t active_cleanup=0,end_cleanup=0,marker_cleanup=0,args_cleanup=0,text_cleanup=0;};
// The exact _StartEdit implementation accepts private storage with an explicit
// Dungeon field. It never requires the player's selected cursor or camera.
uint32_t cell_native(uintptr_t image,uintptr_t dungeon,uintptr_t undo,int32_t index,int mode,const Text* theme,CellReceipt& r) {
    alignas(16) unsigned char storage[0x350]{};alignas(16) unsigned char label[0x38]{};
    const Text edit_label{"Dungeon tiles",13};bool constructed=false;uint32_t code=0;
    *reinterpret_cast<uintptr_t*>(storage+0x2c0)=dungeon;
    __try {
        reinterpret_cast<void(*)(void*,const Text&)>(image+0x5a39e0)(storage,edit_label);
        r.graph=*reinterpret_cast<uintptr_t*>(storage+0x338);r.marker=*reinterpret_cast<uintptr_t*>(storage+0x348);
        r.edit_open=r.marker && *reinterpret_cast<const uint8_t*>(r.marker)!=0;
        if(!r.graph || !r.marker || !r.edit_open)code=ERROR_INVALID_STATE;
        else {
            r.active=true;reinterpret_cast<void(*)(uintptr_t)>(image+0x53dd50)(r.graph);
            if(mode==0)r.changed=reinterpret_cast<bool(*)(uintptr_t,int32_t)>(image+0xa9020)(dungeon,index);
            else if(mode==1)r.changed=reinterpret_cast<bool(*)(uintptr_t,int32_t,const Text&)>(image+0xa91e0)(dungeon,index,*theme);
            else if(mode==2){reinterpret_cast<void(*)(uintptr_t,int32_t,const Text&)>(image+0xa8ed0)(dungeon,index,*theme);r.changed=true;}
            else {reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0xa5960)(dungeon,index);r.changed=true;}
            if(r.changed){reinterpret_cast<void(*)(uintptr_t)>(image+0x54ac40)(r.graph);reinterpret_cast<void(*)(uintptr_t,bool)>(image+0xa9880)(dungeon,true);}
            else code=ERROR_INVALID_STATE;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();r.graph=*reinterpret_cast<uintptr_t*>(storage+0x338);r.marker=*reinterpret_cast<uintptr_t*>(storage+0x348);}
    if(r.active){__try{reinterpret_cast<void(*)(uintptr_t)>(image+0x53dd50)(0);r.active=false;}__except(EXCEPTION_EXECUTE_HANDLER){r.active_cleanup=GetExceptionCode();}}
    if(r.edit_open){__try{reinterpret_cast<void(*)()>(image+0x268750)();}__except(EXCEPTION_EXECUTE_HANDLER){r.end_cleanup=GetExceptionCode();}}
    if(r.marker){__try{reinterpret_cast<void(*)(uintptr_t,size_t)>(image+0x1270520)(r.marker,1);r.marker=0;}__except(EXCEPTION_EXECUTE_HANDLER){r.marker_cleanup=GetExceptionCode();}}
    if(!code && !r.active_cleanup && !r.end_cleanup && !r.marker_cleanup){
        __try {reinterpret_cast<void(*)(void*,const char*)>(image+0xa21c10)(label,"Edit dungeon tiles");constructed=true;
            reinterpret_cast<void(*)(uintptr_t,const void*,uintptr_t)>(image+0x541790)(undo,label,r.graph);r.committed=true;
        }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    }
    if(constructed){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11be4a0)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11c8b80)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.text_cleanup=GetExceptionCode();}
    }return code;
}
struct LightInput {std::array<int32_t,4> spinners;std::array<int32_t,3> sliders;std::array<std::array<float,4>,4> colors;};
struct LightWidgets {std::array<uintptr_t,4> spinners,colors;std::array<uintptr_t,3> sliders;};
LightWidgets light_widgets(Context& c,uintptr_t w) {
    LightWidgets out;constexpr size_t so[]{0x508,0x510,0x500,0x518},co[]{0x4f8,0x4f0,0x530,0x548},lo[]{0x528,0x538,0x540};std::set<uintptr_t> seen;
    const auto check=[&](uintptr_t p,uintptr_t ap){need(p && seen.insert(p).second && c.read<uintptr_t>(p)==c.image+ap,"Dungeon lighting widget class/uniqueness differs");};
    for(size_t i=0;i<4;++i){out.spinners[i]=c.read<uintptr_t>(w+so[i]);out.colors[i]=c.read<uintptr_t>(w+co[i]);check(out.spinners[i],0x16c7ac0);check(out.colors[i],0x1728840);
        need(!c.read<uintptr_t>(out.spinners[i]+0x428) && !c.read<uintptr_t>(out.spinners[i]+0x430) && !c.read<uintptr_t>(out.colors[i]+0x370),"Dungeon lighting input has native gameplay bindings");}
    for(size_t i=0;i<3;++i){out.sliders[i]=c.read<uintptr_t>(w+lo[i]);check(out.sliders[i],0x178ddd0);need(!c.read<uintptr_t>(out.sliders[i]+0x420) && !c.read<uintptr_t>(out.sliders[i]+0x428),"Dungeon lighting slider has native gameplay bindings");}return out;
}
uint32_t light_numbers(uintptr_t image,const LightWidgets* widgets,const LightInput* input,std::array<float,23>* output) {
    float spinner[4]{},slider[3]{};alignas(16) unsigned char angle[16]{};float vector[3]{},forward[2]{0,-1};uint64_t rotated=0;
    __try {
        for(size_t i=0;i<4;++i)spinner[i]=reinterpret_cast<float(*)(uintptr_t,int32_t)>(image+0x7c7b20)(widgets->spinners[i],input->spinners[i]);
        for(size_t i=0;i<3;++i)slider[i]=reinterpret_cast<float(*)(uintptr_t,int32_t)>(image+0x7c1840)(widgets->sliders[i],input->sliders[i]);
        reinterpret_cast<void(*)(void*,float)>(image+0x9f90e0)(angle,spinner[1]*-0.01745329424738884f);
        rotated=reinterpret_cast<uint64_t(*)(const void*,const void*)>(image+0x9f93f0)(angle,forward);
        std::memcpy(vector,&rotated,4);vector[1]=1;std::memcpy(vector+2,reinterpret_cast<const unsigned char*>(&rotated)+4,4);
        reinterpret_cast<void(*)(void*)>(image+0xa0a940)(vector);
        for(size_t i=0;i<4;++i){(*output)[i]=input->colors[0][i]*spinner[0];(*output)[7+i]=input->colors[1][i]*spinner[2];(*output)[11+i]=input->colors[2][i];(*output)[16+i]=input->colors[3][i];}
        for(size_t i=0;i<3;++i)(*output)[4+i]=vector[i];
        (*output)[15]=slider[0]*0.0010000000474974513f;(*output)[20]=slider[1]*0.009999999776482582f;(*output)[21]=slider[2];(*output)[22]=spinner[3];return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t light_prepare(uintptr_t image,uintptr_t w,uintptr_t dungeon,const LightWidgets* widgets,const LightInput* input) {
    __try {
        *reinterpret_cast<uintptr_t*>(w+0x4e8)=dungeon;
        for(size_t i=0;i<4;++i){reinterpret_cast<void(*)(uintptr_t,const void*,bool)>(image+0x8423c0)(widgets->colors[i],input->colors[i].data(),false);
            reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0x7cadc0)(widgets->spinners[i],input->spinners[i]);}
        for(size_t i=0;i<3;++i)reinterpret_cast<void(*)(uintptr_t,int32_t,bool)>(image+0x7c4070)(widgets->sliders[i],input->sliders[i],false);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
constexpr Pin light_pins[]{
    {0x6db4d0,713,"7b7dbd2248456a8644c7e339f2a01f9d1ab42d45ab4a97fb05a8f1b1c6bbad39"},
    {0x6db7a0,455,"e82710192d4174ab69982602acadbcb93b6b268777bbcd7364ce654d992e301b"},
    {0x7c1840,17,"6a1c053fd5d66b35bd798028ad140017ae60f17f0d43fcdfe7fec43c7e4f323b"},
    {0x7c4070,612,"a0f655df3e39717f677f404222e5156ae99232b85de5df76806c37184a2b1268"},
    {0x7c7b20,17,"5c0b1940d701d2edec21e468c9fdb3bceae6a9f8dee6410d0139dca26d8da7c6"},
    {0x7cadc0,356,"2112e2fba9f2a2843d2940b2ff742eccd15c5914c8668ca1eb301cc5cede8db3"},
    {0x8423c0,553,"437190f57ad3314b99ad55a4edf936b665035afba5216fef5d129690c6f48c38"},
    {0x9f90e0,121,"a22ce5fe4492d3d842bd1e2d080743e5b0964042fd07c44e01be995828ffe98d"},
    {0x9f93f0,117,"5766b29a4c6590ec7f9198cb5a587ec364da3430fb2103057bc95a333fbc14be"},
    {0xa0a940,366,"b9f308e0f0ba3137c45398a3ec246c9652f54809722ffb4946ea1d798517b181"}};
uint32_t pack_color(uintptr_t image,uint32_t* output,const std::array<float,4>* color) {
    __try {reinterpret_cast<void(*)(void*,const void*)>(image+0x97bcb0)(output,color);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t palette_native(uintptr_t image,uintptr_t dungeon,uintptr_t data,bool defaults,const std::array<std::array<float,4>,4>* colors,CommandReceipt& r) {
    alignas(8) unsigned char record[0x78]{};uint32_t code=0;uintptr_t slots[4]{};
    __try {
        for(int32_t i=0;i<4;++i){slots[i]=reinterpret_cast<uintptr_t(*)(uintptr_t,int32_t)>(image+0xe4ba70)(dungeon+0x1b8,i);if(slots[i]!=data+size_t(i)*4)return ERROR_INVALID_ADDRESS;}
        reinterpret_cast<void(*)(void*,const char*)>(image+0x970590)(record,defaults?"useDefault true":"useDefault false");r.constructed=true;
        r.accepted=reinterpret_cast<bool(*)(void*,uintptr_t,uintptr_t)>(image+0x8aad90)(record,dungeon+0x1d0,0);
        if(r.accepted){for(size_t i=0;i<4;++i)reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0x97bcb0)(slots[i],&(*colors)[i]);r.returned=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(r.constructed){__try{reinterpret_cast<void(*)(void*)>(image+0x9708c0)(record);}__except(EXCEPTION_EXECUTE_HANDLER){r.cleanup=GetExceptionCode();}}return code;
}
uint32_t runtime_scalar(uintptr_t address,bool byte,int32_t expected,int32_t desired,bool& exchanged) {
    __try {
        if(byte)exchanged=uint8_t(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address),char(desired),char(expected)))==uint8_t(expected);
        else exchanged=_InterlockedCompareExchange(reinterpret_cast<volatile long*>(address),long(desired),long(expected))==expected;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t on_map_native(uintptr_t image,uintptr_t p,bool value) {
    __try{reinterpret_cast<void(*)(uintptr_t,bool)>(image+0x148fc0)(p,value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t rename_native(uintptr_t image,uintptr_t p,const Text* name) {
    __try{reinterpret_cast<void(*)(uintptr_t,const Text*)>(image+0x4d5490)(p,name);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Pose {std::array<float,4> rotation{0,0,0,1};std::array<float,3> position{},scale{1,1,1};};
struct Placement {uintptr_t object=0,graph=0;bool purchased=false,committed=false,edit_open=false;uint32_t begin_cleanup=0,end_cleanup=0,args_cleanup=0,label_cleanup=0;};
uint32_t placement_probe(uintptr_t image,uintptr_t dungeon,uintptr_t business,uintptr_t shard,const Text* type,const Pose* pose,int32_t& cost,bool& offline,bool& affordable,uintptr_t& region) {
    __try {
        offline=reinterpret_cast<bool(*)(uintptr_t)>(image+0xa36a0)(dungeon);
        cost=reinterpret_cast<int32_t(*)(const Text&)>(image+0x147b20)(*type);
        affordable=cost>=0 && reinterpret_cast<bool(*)(uintptr_t,int32_t)>(image+0x55200)(business,cost);
        region=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*,uintptr_t)>(image+0x50eb00)(shard,pose->position.data(),0);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t region_native(uintptr_t image,uintptr_t shard,const Pose* pose,uintptr_t& region) {
    __try{region=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*,uintptr_t)>(image+0x50eb00)(shard,pose->position.data(),0);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t place_gizmo_native(uintptr_t image,uintptr_t dungeon,uintptr_t manager,uintptr_t undo,uintptr_t sound,const Text* type,const Text* variant,const Pose* pose,int tool,int32_t cost,Placement& r) {
    alignas(16) unsigned char transform[0x70]{},edit_name[0x20]{},label[0x38]{};
    bool edit_name_ready=false,label_ready=false;uintptr_t inverse=0;int32_t edge[2]{-1,-1};uint32_t code=0;
    __try {
        reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(image+0xa09f30)(transform,pose->rotation.data(),pose->position.data(),pose->scale.data());
        r.purchased=reinterpret_cast<bool(*)(const Text&,const void*)>(image+0x148a50)(*type,pose->position.data());
        if(!r.purchased)code=ERROR_ACCESS_DENIED;
        else {
            if(tool==2)reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0x5149b0)(sound,0x20);
            if(tool==0 && reinterpret_cast<bool(*)()>(image+0x267d30)()){
                reinterpret_cast<void(*)(void*,const char*)>(image+0x5c6c50)(edit_name,"Dungeon input");edit_name_ready=true;
                reinterpret_cast<void(*)(const void*)>(image+0x268400)(edit_name);r.edit_open=true;
            }
            r.object=reinterpret_cast<uintptr_t(*)(uintptr_t,const Text&,const Text&,const void*)>(image+0x154a10)(manager,*type,*variant,transform);
            if(!r.object)code=ERROR_INVALID_HANDLE;
            else {
                if(tool==0){reinterpret_cast<void(*)(void*,uintptr_t,const void*)>(image+0xa5c10)(edge,dungeon,pose->position.data());reinterpret_cast<void(*)(uintptr_t,int32_t,int32_t)>(image+0x146980)(r.object,edge[0],edge[1]);}
                r.graph=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0x110);
                if(!r.graph)code=ERROR_NOT_ENOUGH_MEMORY;
                else {
                    reinterpret_cast<void(*)(uintptr_t)>(image+0x5466b0)(r.graph);
                    if(tool==0 || tool==2){
                        inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0xc8);
                        if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
                        else {reinterpret_cast<void(*)(uintptr_t,int32_t,const void*)>(image+0x543230)(inverse,cost,pose->position.data());reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x546d20)(r.graph,inverse);}
                    }
                    if(!code){
                        inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0xc0);
                        if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
                        else {reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x5441f0)(inverse,r.object);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x546d20)(r.graph,inverse);}
                    }
                    if(!code && tool==1 && cost!=0){
                        inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0xc8);
                        if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
                        else {reinterpret_cast<void(*)(uintptr_t,int32_t,const void*)>(image+0x543230)(inverse,cost,pose->position.data());reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x546d20)(r.graph,inverse);}
                    }
                    if(!code){reinterpret_cast<void(*)(void*,const char*)>(image+0xa21c10)(label,"Place Dungeon Gizmo");label_ready=true;
                        reinterpret_cast<void(*)(uintptr_t,const void*,uintptr_t)>(image+0x541790)(undo,label,r.graph);r.committed=true;}
                }
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(label_ready){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11be4a0)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11c8b80)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}
    }
    if(edit_name_ready){__try{reinterpret_cast<void(*)(void*)>(image+0x11c8b80)(edit_name);}__except(EXCEPTION_EXECUTE_HANDLER){r.begin_cleanup=GetExceptionCode();}}
    if(r.edit_open){__try{reinterpret_cast<void(*)()>(image+0x268750)();}__except(EXCEPTION_EXECUTE_HANDLER){r.end_cleanup=GetExceptionCode();}}
    return code;
}
struct NativeArray {uintptr_t ap;const uintptr_t* data;int32_t count,capacity;};
uint32_t selection_native(uintptr_t image,uintptr_t selection,const NativeArray* rows,bool destroy) {
    __try {reinterpret_cast<void(*)(uintptr_t,const NativeArray*)>(image+(destroy?0x2d7b10:0x2d9000))(selection,rows);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
std::vector<uintptr_t> selection(Context& c,uintptr_t manager) {
    need(manager && c.read<uintptr_t>(manager)==c.image+0x1726970 && !c.read<uint8_t>(manager+0xd1) && !c.read<uint8_t>(manager+0xd3),"Dungeon control requires idle native SelectionManager");return array(c,manager+0x20);
}
std::vector<uintptr_t> undo_stack(Context& c,uintptr_t manager) {
    need(manager && c.read<uintptr_t>(manager)==c.image+0x1704410 && c.read<uintptr_t>(manager+8)==c.image+0x16e76c0,"Dungeon Undo manager/array class differs");return array(c,manager+8,20);
}
void committed_graph(Context& c,uintptr_t undo,const std::vector<uintptr_t>& before,uintptr_t expected=0) {
    const auto after=undo_stack(c,undo);need(!after.empty(),"Dungeon control original Undo commit is absent");const auto graph=after.back();
    need(detail::committed_history(before,after,array(c,undo+0x20,20).empty(),c.read<uint8_t>(graph+0x10c),c.read<int32_t>(graph+0x58),c.read<uintptr_t>(c.image+0x1a41a80)),"Dungeon control original Undo/Redo history or locked settlement readback differs");
    const auto nodes=c.read<int32_t>(graph+0x10),capacity=c.read<int32_t>(graph+0x14);
    need((!expected || graph==expected) && c.read<uintptr_t>(graph)==c.image+0x16e9670 && c.read<uintptr_t>(graph+0x18)==c.image+0x16e9160 && nodes>=0 && nodes<=100000 && capacity>=nodes && (!nodes || c.read<uintptr_t>(graph+8)) && c.read<int32_t>(graph+0x28)>0 && c.read<uintptr_t>(graph+0x48)==c.image+0x174eff0,"Dungeon control graph ownership/content differs");
}
struct MoveReceipt {uintptr_t graph=0;bool label=false,begun=false,opened=false,constructed=false,moved=false;uint32_t move_cleanup=0,close_cleanup=0,end_cleanup=0,args_cleanup=0,label_cleanup=0;};
uint32_t move_native(uintptr_t image,uintptr_t target,const Pose* pose,MoveReceipt& r) {
    alignas(16) unsigned char transform[0x70]{},label[0x38]{},action[0x130]{};uint32_t code=0;
    __try {
        reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(image+0xa09f30)(transform,pose->rotation.data(),pose->position.data(),pose->scale.data());
        reinterpret_cast<void(*)(void*,const char*)>(image+0xa21c10)(label,"Move");r.label=true;
        reinterpret_cast<void(*)(const void*)>(image+0x268400)(label);r.begun=true;
        r.graph=reinterpret_cast<uintptr_t(*)(const void*)>(image+0x53fea0)(label);r.opened=true;
        if(!r.graph)code=ERROR_INVALID_HANDLE;
        else {reinterpret_cast<void(*)(void*,uintptr_t,const void*)>(image+0x564f70)(action,target,transform);r.constructed=true;
            reinterpret_cast<void(*)(void*,uintptr_t)>(image+0x565410)(action,r.graph);r.moved=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(r.constructed){__try{reinterpret_cast<void(*)(void*)>(image+0xcfcd40)(action);}__except(EXCEPTION_EXECUTE_HANDLER){r.move_cleanup=GetExceptionCode();}}
    if(r.opened && r.graph){__try{reinterpret_cast<void(*)()>(image+0x53ded0)();}__except(EXCEPTION_EXECUTE_HANDLER){r.close_cleanup=GetExceptionCode();}}
    if(r.begun){__try{reinterpret_cast<void(*)()>(image+0x268750)();}__except(EXCEPTION_EXECUTE_HANDLER){r.end_cleanup=GetExceptionCode();}}
    if(r.label){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11be4a0)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11c8b80)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}
    }return code;
}
uint32_t duplicate_price(uintptr_t p,int32_t& amount,bool& allowed) {
    __try{const auto table=*reinterpret_cast<const uintptr_t*>(p);allowed=reinterpret_cast<bool(*)(uintptr_t)>(*reinterpret_cast<const uintptr_t*>(table+0xd8))(p);amount=reinterpret_cast<int32_t(*)(uintptr_t)>(*reinterpret_cast<const uintptr_t*>(table+0x278))(p);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t container_feature(uintptr_t image,uintptr_t target,bool& container) {
    __try{container=reinterpret_cast<bool(*)(uintptr_t,int32_t)>(image+0x146590)(target,0);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t duplicate_native(uintptr_t image,uintptr_t business,uintptr_t undo,uintptr_t source,const Pose* pose,int32_t price,Placement& r) {
    alignas(16) unsigned char transform[0x70]{},name[0x20]{},label[0x38]{};uintptr_t inverse=0;bool name_ready=false,label_ready=false;uint32_t code=0;
    __try {
        r.purchased=price<=0 || reinterpret_cast<bool(*)(uintptr_t,int32_t,int32_t)>(image+0x55280)(business,price,6);
        if(!r.purchased)code=ERROR_ACCESS_DENIED;
        else {
            if(reinterpret_cast<bool(*)()>(image+0x267d30)()){
                reinterpret_cast<void(*)(void*,const char*)>(image+0x5c6c50)(name,"Duplicating collection");name_ready=true;
                reinterpret_cast<void(*)(const void*)>(image+0x268400)(name);r.edit_open=true;
            }
            r.graph=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0x110);
            if(!r.graph)code=ERROR_NOT_ENOUGH_MEMORY;
            else {
                reinterpret_cast<void(*)(uintptr_t)>(image+0x5466b0)(r.graph);inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0xc8);
                if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
                else {
                    reinterpret_cast<void(*)(uintptr_t,int32_t,const void*)>(image+0x543230)(inverse,price,pose->position.data());reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x546d20)(r.graph,inverse);
                    reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(image+0xa09f30)(transform,pose->rotation.data(),pose->position.data(),pose->scale.data());
                    r.object=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*)>(image+0x1456a0)(source,transform);
                    if(!r.object)code=ERROR_INVALID_HANDLE;
                    else {inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0xc0);if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
                        else {reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x5441f0)(inverse,r.object);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x546d20)(r.graph,inverse);
                            reinterpret_cast<void(*)(void*,const char*)>(image+0xa21c10)(label,"Duplicate Dungeon Gizmo");label_ready=true;
                            reinterpret_cast<void(*)(uintptr_t,const void*,uintptr_t)>(image+0x541790)(undo,label,r.graph);r.committed=true;}}
                }
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(label_ready){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11be4a0)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11c8b80)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}
    }
    if(name_ready){__try{reinterpret_cast<void(*)(void*)>(image+0x11c8b80)(name);}__except(EXCEPTION_EXECUTE_HANDLER){r.begin_cleanup=GetExceptionCode();}}
    if(r.edit_open){__try{reinterpret_cast<void(*)()>(image+0x268750)();}__except(EXCEPTION_EXECUTE_HANDLER){r.end_cleanup=GetExceptionCode();}}return code;
}
uint32_t spawn_monster_native(uintptr_t image,uintptr_t manager,uintptr_t shard,uintptr_t undo,uintptr_t sound,const Pose* pose,int32_t rank,int32_t character,int32_t level,Placement& r) {
    alignas(16) unsigned char transform[0x70]{},label[0x38]{};bool label_ready=false;uintptr_t inverse=0;uint32_t code=0;
    __try {
        reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(image+0xa09f30)(transform,pose->rotation.data(),pose->position.data(),pose->scale.data());
        r.object=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*,int32_t,uintptr_t,int32_t)>(image+0x296940)(manager,transform,rank,0,character);
        if(!r.object)code=ERROR_INVALID_HANDLE;
        else {
            r.graph=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0x110);
            if(!r.graph)code=ERROR_NOT_ENOUGH_MEMORY;
            else {
                reinterpret_cast<void(*)(uintptr_t)>(image+0x5466b0)(r.graph);inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0xc0);
                if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
                else {
                    reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x5441f0)(inverse,r.object);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x546d20)(r.graph,inverse);
                    reinterpret_cast<void(*)(void*,const char*)>(image+0xa21c10)(label,"Place Dungeon Monster");label_ready=true;
                    reinterpret_cast<void(*)(uintptr_t,const void*,uintptr_t)>(image+0x541790)(undo,label,r.graph);r.committed=true;
                    if(rank==2){const auto definition=reinterpret_cast<uintptr_t(*)(uintptr_t)>(image+0x3a18f0)(r.object);
                        if(definition && reinterpret_cast<int32_t(*)(uintptr_t)>(image+0x945b0)(definition)>1)reinterpret_cast<void(*)(int32_t)>(image+0x45de30)(15);}
                    reinterpret_cast<void(*)(uintptr_t,int32_t,uintptr_t,const void*)>(image+0x514d10)(sound,0x2f,shard,pose->position.data());
                    const auto prior=*reinterpret_cast<const int32_t*>(r.object+0x6f8);
                    if(_InterlockedCompareExchange(reinterpret_cast<volatile long*>(r.object+0x6f8),long(level),long(prior))!=prior)code=ERROR_INVALID_STATE;
                    else reinterpret_cast<void(*)(uintptr_t)>(image+0x3c51f0)(r.object);
                }
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(label_ready){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11be4a0)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11c8b80)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}
    }return code;
}
struct Point {int64_t x,z;};
struct VectorView {uintptr_t begin,end,capacity;};
struct EraseReceipt {uintptr_t graph=0;float area=0;bool shape=false,committed=false;uint32_t stage=0,shape_cleanup=0,args_cleanup=0,label_cleanup=0;};
uint32_t erase_native(uintptr_t image,uintptr_t dungeon,uintptr_t undo,const VectorView* paths,EraseReceipt& r) {
    alignas(16) unsigned char shape[0x70]{},label[0x38]{};uintptr_t inverse=0;bool label_ready=false;uint32_t code=0;
    __try {
        r.stage=1;reinterpret_cast<void(*)(void*)>(image+0xf2880)(shape);r.shape=true;
        r.stage=2;reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0xf8f40)(*reinterpret_cast<const uintptr_t*>(shape+0x18),paths);
        r.stage=3;reinterpret_cast<void(*)(void*)>(image+0xf4bd0)(shape);
        r.graph=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0x110);
        if(!r.graph)code=ERROR_NOT_ENOUGH_MEMORY;
        else {
            r.stage=4;reinterpret_cast<void(*)(uintptr_t)>(image+0x5466b0)(r.graph);inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0xd8);
            if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
            else {
                r.stage=5;reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x544ca0)(inverse,dungeon);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x546d20)(r.graph,inverse);
                r.stage=6;r.area=reinterpret_cast<float(*)(uintptr_t,const void*,uintptr_t)>(image+0x158810)(dungeon+0x500,shape,0);
                if(!std::isfinite(r.area) || r.area<=0)code=ERROR_INVALID_STATE;
                else {
                    r.stage=7;reinterpret_cast<void(*)(void*,const char*)>(image+0xa21c10)(label,"Edit dungeon paint");label_ready=true;
                    reinterpret_cast<void(*)(uintptr_t,const void*,uintptr_t)>(image+0x541790)(undo,label,r.graph);r.committed=true;r.stage=8;
                }
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(label_ready){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11be4a0)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11c8b80)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}
    }
    if(r.shape){__try{reinterpret_cast<void(*)(void*)>(image+0xf2610)(shape);}__except(EXCEPTION_EXECUTE_HANDLER){r.shape_cleanup=GetExceptionCode();}}return code;
}
#include "fill.inc"
struct PaintReceipt {uintptr_t graph=0,stroke=0,pending=0;float painted=0,erased=0;uint32_t stage=0,shape_cleanup=0,accumulated_cleanup=0,args_cleanup=0,label_cleanup=0;bool shape=false,accumulated=false,committed=false;};
uint32_t paint_native(uintptr_t image,uintptr_t dungeon,uintptr_t undo,uintptr_t fill,const VectorView* paths,PaintReceipt& r) {
    alignas(16) unsigned char shape[0x70]{},accumulated[0x70]{},label[0x38]{};uintptr_t inverse=0;bool label_ready=false;uint32_t code=0;
    __try {
        r.stage=1;reinterpret_cast<void(*)(void*)>(image+0xf2880)(shape);r.shape=true;
        reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0xf8f40)(*reinterpret_cast<const uintptr_t*>(shape+0x18),paths);reinterpret_cast<void(*)(void*)>(image+0xf4bd0)(shape);
        r.stage=2;reinterpret_cast<void(*)(void*)>(image+0xf2880)(accumulated);r.accumulated=true;
        reinterpret_cast<void(*)(void*,const void*)>(image+0xf9ca0)(accumulated,shape);
        r.graph=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0x110);
        if(!r.graph)code=ERROR_NOT_ENOUGH_MEMORY;
        else {
            r.stage=3;reinterpret_cast<void(*)(uintptr_t)>(image+0x5466b0)(r.graph);inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0xd8);
            if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
            else {
                reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x544ca0)(inverse,dungeon);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x546d20)(r.graph,inverse);
                r.stage=4;r.stroke=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*,uintptr_t)>(image+0x158770)(dungeon+0x500,shape,fill);
                if(r.stroke)r.painted=reinterpret_cast<float(*)(uintptr_t,const void*)>(image+0x15e730)(r.stroke,shape);
                else {
                    const auto area=reinterpret_cast<float(*)(const void*)>(image+0xf22a0)(accumulated);
                    if(area>50.0f){
                        r.stage=5;r.pending=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa0d230)(0x140);
                        if(!r.pending)code=ERROR_NOT_ENOUGH_MEMORY;
                        else {reinterpret_cast<void(*)(uintptr_t)>(image+0x15e560)(r.pending);
                            reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0x15e260)(r.pending,shape);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x15e370)(r.pending,fill);
                            reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x1574b0)(dungeon+0x500,r.pending);r.stroke=r.pending;r.pending=0;
                            reinterpret_cast<void(*)(uintptr_t)>(image+0x15e48c)(r.stroke);r.painted=area;}
                    }
                }
                if(!code && r.stroke){r.stage=6;reinterpret_cast<void(*)(uintptr_t)>(image+0x15e2e0)(r.stroke);
                    r.erased=reinterpret_cast<float(*)(uintptr_t,uintptr_t,uintptr_t)>(image+0x158810)(dungeon+0x500,r.stroke+0x18,r.stroke);
                    reinterpret_cast<void(*)(uintptr_t)>(image+0x15e4fc)(r.stroke);r.stroke=0;}
                if(!code && std::isfinite(r.painted) && std::isfinite(r.erased) && (r.painted>0 || r.erased>0)){
                    r.stage=7;reinterpret_cast<void(*)(void*,const char*)>(image+0xa21c10)(label,"Edit dungeon paint");label_ready=true;
                    reinterpret_cast<void(*)(uintptr_t,const void*,uintptr_t)>(image+0x541790)(undo,label,r.graph);r.committed=true;r.stage=8;
                }else if(!code)code=ERROR_INVALID_STATE;
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(label_ready){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11be4a0)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11c8b80)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}
    }
    if(r.shape){__try{reinterpret_cast<void(*)(void*)>(image+0xf2610)(shape);}__except(EXCEPTION_EXECUTE_HANDLER){r.shape_cleanup=GetExceptionCode();}}
    if(r.accumulated){__try{reinterpret_cast<void(*)(void*)>(image+0xf2610)(accumulated);}__except(EXCEPTION_EXECUTE_HANDLER){r.accumulated_cleanup=GetExceptionCode();}}return code;
}
#include "building.inc"
#include "gizmo_state.inc"
}

std::optional<Json> query(Context& c,const std::string& operation,const Json& args) {
    if(operation=="dungeons.list"){
        c.require_world();const auto map=c.singleton(0x13eb8c0);need(map && c.read<uintptr_t>(map)==c.image+0x1748c00,"Dungeon current WorldMap class differs");Json rows=Json::array();
        for(auto region:array(c,map+0x230)){
            const auto manager=c.read<uintptr_t>(region+0x950);need(c.read<uintptr_t>(region)==c.image+0x178dcd0 && c.read<uintptr_t>(region+0x70)==map+0x1c0 && manager && c.read<uintptr_t>(manager)==c.image+0x171e760 && c.read<uintptr_t>(manager+0x450)==map+0x1c0 && c.read<uintptr_t>(manager+0x458)==region,"Dungeon root region/Building manager owner differs");
            for(auto exterior:array(c,manager+0x408))if(c.read<uintptr_t>(exterior)==c.image+0x16df760){const auto h=handle(c,exterior);const auto o=owner(c,Json{{"handle",h}});rows.push_back({{"handle",h},{"dungeon_uid",std::to_string(c.read<uint64_t>(o.dungeon+0x18))}});}
        }return rows;
    }
    if(operation!="dungeons.get" && operation!="dungeons.layout" && operation!="dungeons.instances" && operation!="dungeons.gizmos" && operation!="dungeons.buildings" && operation!="dungeons.monsters")return std::nullopt;
    const auto o=owner(c,args);
    if(operation=="dungeons.layout")return layout_json(layout(c,o));
    if(operation=="dungeons.instances"){Json rows=Json::array();for(const auto& row:instances(c,o))rows.push_back(instance_json(row));return rows;}
    if(operation=="dungeons.gizmos"){Json rows=Json::array();for(auto p:gizmos(c,o,args))rows.push_back(gizmo_json(c,p));return rows;}
    if(operation=="dungeons.monsters"){Json rows=Json::array();for(auto p:monsters(c,o,args))rows.push_back(monster_json(c,p));return rows;}
    if(operation=="dungeons.buildings"){
        uintptr_t shard=0;const auto region=scene_region(c,o,args,shard),manager=c.read<uintptr_t>(region+0x950);
        need(manager && c.read<uintptr_t>(manager)==c.image+0x171e760 && c.read<uintptr_t>(manager+0x450)==shard && c.read<uintptr_t>(manager+0x458)==region,"Dungeon Building manager owner differs");
        Json rows=Json::array();for(auto p:array(c,manager+0x408)){
            need(c.read<uintptr_t>(p+0x170)==shard && c.read<uintptr_t>(p+0x178)==region,"Dungeon Building scene differs");
            rows.push_back({{"handle",handle(c,p)},{"type",c.text(p+0x768)},{"variant",c.text(p+0x788)},{"position",c.read<std::array<float,3>>(p+0x110)}});
        }return rows;
    }
    const auto light=c.read<std::array<float,23>>(o.dungeon+0x1f0);for(auto x:light)need(std::isfinite(x),"Dungeon authored lighting is non-finite");
    const auto desired=c.read<int32_t>(o.dungeon+0x40c),atmosphere=c.read<int32_t>(o.dungeon+0x830);
    need(desired>=0 && desired<=10 && atmosphere>=0 && atmosphere<6,"Dungeon config/atmosphere bounds differ");
    return Json{{"handle",handle(c,o.exterior)},{"dungeon_uid",std::to_string(c.read<uint64_t>(o.dungeon+0x18))},{"desired_instances",desired},
        {"atmosphere",atmosphere},{"light",light},{"palette_defaults",c.read<uint8_t>(o.dungeon+0x1d0)!=0},{"instance_count",instances(c,o).size()}};
}

bool action(Context& c,const std::string& operation,const Json& args,bool& attempted,Json& result,std::string& cause) {
    const bool instance=operation=="dungeons.make_instance" || operation=="dungeons.destroy_instance" || operation=="dungeons.reset_instance" || operation=="dungeons.force_reset_instance";
    const bool cell=operation=="dungeons.open_cell" || operation=="dungeons.close_cell" || operation=="dungeons.set_cell_theme" || operation=="dungeons.apply_cell_palette";
    if(!instance && !cell && operation!="dungeons.configure_instances" && operation!="dungeons.set_atmosphere" && operation!="dungeons.set_lighting" && operation!="dungeons.set_palette" && operation!="dungeons.set_gizmo_runtime" && operation!="dungeons.rename_gizmo" && operation!="dungeons.place_gizmo" && operation!="dungeons.move_gizmo" && operation!="dungeons.destroy_gizmo" && operation!="dungeons.duplicate_gizmo" && operation!="dungeons.set_gizmo_contents" && operation!="dungeons.spawn_monster" && operation!="dungeons.erase_paint" && operation!="dungeons.paint" && operation!="dungeons.place_building" && operation!="dungeons.move_building" && operation!="dungeons.destroy_building" && operation!="dungeons.duplicate_building" && operation!="dungeons.set_gizmo_state")throw std::invalid_argument("Unknown Dungeon action: "+operation);
    return detail::outcome(result,cause,[&]() -> bool {
        const auto o=owner(c,args);
        if(operation=="dungeons.place_building"){result=place_building(c,o,args,attempted);return true;}
        if(operation=="dungeons.set_gizmo_state"){result=set_gizmo_state(c,o,args,attempted);return true;}
        if(operation=="dungeons.move_building" || operation=="dungeons.destroy_building" || operation=="dungeons.duplicate_building"){result=control_building(c,o,operation,args,attempted);return true;}
        if(operation=="dungeons.erase_paint" || operation=="dungeons.paint") {
            const auto& paths=args.at("paths");need(paths.is_array() && !paths.empty() && paths.size()<=2048,"Dungeon erase requires 1..2048 polygon paths");std::vector<std::vector<Point>> points;size_t count=0;
            for(const auto& path:paths){need(path.is_array() && path.size()>=3 && path.size()<=100000-count,"Dungeon paint path requires at least 3 points within the 100000 point bound");auto& next=points.emplace_back();
                for(const auto& point:path){need(point.is_array() && point.size()==2,"Dungeon paint point requires exactly x,z integer coordinates");Point p{};for(size_t i=0;i<2;++i){need(point[i].is_number_integer() && (!point[i].is_number_unsigned() || point[i].get<uint64_t>()<=uint64_t(INT64_MAX>>2)),"Dungeon paint coordinate exceeds native clipping range");const auto value=point[i].get<int64_t>();need(value>=-(INT64_MAX>>2) && value<=(INT64_MAX>>2),"Dungeon paint coordinate exceeds native clipping range");if(i)p.z=value;else p.x=value;}next.push_back(p);}count+=path.size();}
            std::vector<VectorView> views;for(const auto& path:points){const auto p=uintptr_t(path.data()),end=p+path.size()*sizeof(Point);views.push_back({p,end,end});}const auto first=uintptr_t(views.data()),end=first+views.size()*sizeof(VectorView);const VectorView borrowed{first,end,end};
            for(uintptr_t rva:{0xf2880,0xf8f40,0xf4bd0,0xa0d230,0x5466b0,0x544ca0,0x546d20,0x158810,0xa21c10,0x541790,0x11be4a0,0x11c8b80,0xf2610})c.verify_original(rva);
            need(c.read<uintptr_t>(o.dungeon+0x500)==c.image+0x178cff0,"Dungeon embedded Canvas class differs");need(!c.read<uintptr_t>(c.image+0x1a41a80),"Dungeon paint requires idle native Undo graph");const auto undo=c.singleton(0x13eb380);const auto before=undo_stack(c,undo);
            if(operation=="dungeons.paint"){
                fill_records(args.at("fill_records"));verify_fill_code(c);for(uintptr_t rva:{0xf9ca0,0x158770,0x15e730,0xf22a0,0x15e560,0x15e260,0x15e370,0x1574b0,0x15e48c,0x15e2e0,0x15e4fc})c.verify_original(rva);
                attempted=true;auto fill=create_fill(c,args.at("fill_records"));PaintReceipt receipt;const auto code=paint_native(c.image,o.dungeon,undo,fill.pointer,&borrowed,receipt);const auto cleanup=fill_dispose_native(c.image,fill.pointer);
                need(!code && !cleanup && receipt.committed && !receipt.pending && !receipt.stroke && !receipt.shape_cleanup && !receipt.accumulated_cleanup && !receipt.args_cleanup && !receipt.label_cleanup,"Dungeon paint failed SEH="+std::to_string(code)+" stage="+std::to_string(receipt.stage)+" painted="+std::to_string(receipt.painted)+" erased="+std::to_string(receipt.erased)+" required_changed_area>0 fill_cleanup="+std::to_string(cleanup)+" shape_cleanup="+std::to_string(receipt.shape_cleanup)+" accumulated_cleanup="+std::to_string(receipt.accumulated_cleanup)+" args_cleanup="+std::to_string(receipt.args_cleanup)+" label_cleanup="+std::to_string(receipt.label_cleanup)+" pending_stroke="+std::to_string(bool(receipt.pending || receipt.stroke))+"; do not replay");
                need(owner(c,args).dungeon==o.dungeon,"Dungeon Canvas owner changed during paint");committed_graph(c,undo,before,receipt.graph);result={{"painted_area",receipt.painted},{"erased_area",receipt.erased}};return true;
            }
            EraseReceipt receipt;attempted=true;const auto code=erase_native(c.image,o.dungeon,undo,&borrowed,receipt);
            need(!code && receipt.committed && !receipt.shape_cleanup && !receipt.args_cleanup && !receipt.label_cleanup,"Dungeon paint erasure failed SEH="+std::to_string(code)+" stage="+std::to_string(receipt.stage)+" erased="+std::to_string(receipt.area)+" required_area>0 shape_cleanup="+std::to_string(receipt.shape_cleanup)+" args_cleanup="+std::to_string(receipt.args_cleanup)+" label_cleanup="+std::to_string(receipt.label_cleanup)+"; do not replay");
            need(owner(c,args).dungeon==o.dungeon,"Dungeon Canvas owner changed during erasure");committed_graph(c,undo,before,receipt.graph);result={{"erased_area",receipt.area}};
        } else if(operation=="dungeons.spawn_monster") {
            const auto character=object(c,args.at("character"),0x170bd30);need(c.read<int32_t>(character+0x2c)==1 && !c.read<uint8_t>(character+0x490),"Dungeon Monster requires a live Monster Character definition");const auto type=c.read<int32_t>(character+0x30);
            const auto definitions=c.singleton(0x13eb7a0);need(definitions && c.read<uintptr_t>(definitions)==c.image+0x17387d0,"Dungeon Monster Character manager class differs");const auto types=array(c,definitions+0xd0);need(type>=0 && size_t(type)<types.size() && types[size_t(type)]==character,"Dungeon Monster Character full UID/global type slot differs");
            const int32_t rank=integer(args.at("rank"),0,2,"Monster rank"),level=integer(args.at("level"),1,INT32_MAX,"Monster level");Pose pose;pose.position=numbers<3>(args.at("position"),"Monster position");if(args.contains("rotation"))pose.rotation=numbers<4>(args.at("rotation"),"Monster rotation");float norm=0;for(float x:pose.rotation)norm+=x*x;need(std::abs(norm-1)<0.002f,"Dungeon Monster rotation must be a normalized quaternion");
            for(uintptr_t rva:{0x296940,0xa09f30,0xa0d230,0x5466b0,0x5441f0,0x546d20,0xa21c10,0x541790,0x3a18f0,0x945b0,0x45de30,0x514d10,0x3c51f0,0x11be4a0,0x11c8b80,0x50eb00})c.verify_original(rva);
            need(c.read<uintptr_t>(c.image+0x16c6b10+0x470)==c.image+0x3c51f0,"Dungeon Monster LevelChanged virtual differs");need(!c.read<uintptr_t>(c.image+0x1a41a80),"Dungeon Monster placement requires idle native Undo");uintptr_t shard=0;const auto region=scene_region(c,o,args,shard),manager=c.read<uintptr_t>(region+0x988),undo=c.singleton(0x13eb380),sound=c.singleton(0x13eb450);need(sound && undo,"Dungeon Monster placement requires Sound/Undo singleton");
            uintptr_t destination=0;seh(region_native(c.image,shard,&pose,destination),"Monster destination RegionAt");need(destination==region,"Dungeon Monster position leaves the selected scene Region");
            const auto before=monsters(c,o,args),undo_before=undo_stack(c,undo);Placement receipt;attempted=true;const auto code=spawn_monster_native(c.image,manager,shard,undo,sound,&pose,rank,type,level,receipt);
            need(!code && receipt.committed && !receipt.args_cleanup && !receipt.label_cleanup,"Dungeon Monster placement failed SEH="+std::to_string(code)+" args_cleanup="+std::to_string(receipt.args_cleanup)+" label_cleanup="+std::to_string(receipt.label_cleanup)+"; do not replay");
            committed_graph(c,undo,undo_before,receipt.graph);const auto after=monsters(c,owner(c,args),args);need(after.size()==before.size()+1 && after.back()==receipt.object && std::equal(before.begin(),before.end(),after.begin()),"Dungeon Monster placement surviving roster differs");result=monster_json(c,receipt.object);
            need(result.at("rank")==rank && result.at("character_type")==type && result.at("character_kind")== (rank==2?4:1) && result.at("level")==level && result.at("tag")==0 && result.at("status")==3 && result.at("position")==Json(pose.position) && result.at("rotation")==Json(pose.rotation) && result.at("scale")==Json(pose.scale),"Dungeon Monster placement authored readback differs");
        } else if(operation=="dungeons.set_gizmo_contents") {
            const auto target=object(c,args.at("gizmo"),0x174fe50);const auto members=gizmos(c,o,args);need(std::find(members.begin(),members.end(),target)!=members.end(),"Dungeon Gizmo does not belong to the selected scene");
            pin(c,{0x146590,38,"cd1def127ff461cfccd56ad5c12b07e201cd625757245b02f98c41e472a0bb1e"});bool container=false;seh(container_feature(c.image,target,container),"Gizmo container feature");need(container,"Dungeon Gizmo content edits require the native container feature");
            const auto& contents=args.at("contents");need(contents.is_object() && !contents.empty(),"Dungeon contents must contain at least one field");
            for(const auto& [name,value]:contents.items()){need(name=="gold" || name=="loot" || name=="vendor_trash" || name=="keys" || name=="boss_keys","Dungeon content field is unsupported: "+name);integer(value,0,5,"container content");}
            auto expected=gizmo_json(c,target);
            for(const auto& [name,offset]:std::array<std::pair<const char*,size_t>,5>{{{"gold",0x260},{"loot",0x264},{"vendor_trash",0x268},{"keys",0x26c},{"boss_keys",0x270}}}){
                if(!contents.contains(name) || contents.at(name)==expected.at(name))continue;need(gizmo_json(c,object(c,args.at("gizmo"),0x174fe50))==expected,"Dungeon container changed before content CAS");
                bool exchanged=false;attempted=true;seh(runtime_scalar(target+offset,false,expected.at(name).get<int32_t>(),integer(contents.at(name),0,5,"container content"),exchanged),"Gizmo content CAS");need(exchanged,"Dungeon content CAS prior value differs; do not replay");expected[name]=contents.at(name);
            }
            result=gizmo_json(c,object(c,args.at("gizmo"),0x174fe50));need(result==expected,"Dungeon container content readback differs");
        } else if(operation=="dungeons.move_gizmo" || operation=="dungeons.destroy_gizmo" || operation=="dungeons.duplicate_gizmo") {
            const auto target=object(c,args.at("gizmo"),0x174fe50);const auto members=gizmos(c,o,args);need(std::find(members.begin(),members.end(),target)!=members.end(),"Dungeon Gizmo does not belong to the selected scene");
            need(!args.contains("instance") && instances(c,o).empty(),"Dungeon Gizmo control requires authored template with zero Instances");
            c.verify_original(0xa36a0);c.verify_original(0x147b20);c.verify_original(0x55200);c.verify_original(0x50eb00);
            const auto before=gizmo_json(c,target);Pose pose;pose.position=before.at("position").get<std::array<float,3>>();pose.rotation=before.at("rotation").get<std::array<float,4>>();pose.scale=before.at("scale").get<std::array<float,3>>();
            if(operation=="dungeons.move_gizmo" || operation=="dungeons.duplicate_gizmo"){
                pose.position=numbers<3>(args.at("position"),"position");if(args.contains("rotation"))pose.rotation=numbers<4>(args.at("rotation"),"rotation");if(args.contains("scale"))pose.scale=numbers<3>(args.at("scale"),"scale");
                for(float x:pose.position)need(std::isfinite(x),"Dungeon Gizmo move position is non-finite");for(float x:pose.scale)need(std::isfinite(x) && x>0,"Dungeon Gizmo move scale must be positive and finite");float norm=0;for(float x:pose.rotation){need(std::isfinite(x),"Dungeon Gizmo move quaternion is non-finite");norm+=x*x;}need(std::abs(norm-1)<0.002f,"Dungeon Gizmo move quaternion is not normalized");
                need(c.read<uintptr_t>(c.image+0x174fe50+0xd0)==c.image+0xe52bb0 && c.read<uintptr_t>(c.image+0x174fe50+0x160)==c.image+0x1478a0,"Dungeon Gizmo movement virtual methods differ");
            }
            const auto type=before.at("type").get<std::string>();const Text key{type.data(),uint64_t(type.size())};int32_t ignored_cost=0;bool offline=false,ignored_affordable=false;uintptr_t destination=0;
            seh(placement_probe(c.image,o.dungeon,c.singleton(0x13eb220),o.shard,&key,&pose,ignored_cost,offline,ignored_affordable,destination),"Gizmo control preflight");need(offline && destination==o.region,"Dungeon Gizmo control requires offline native owner and same Region");
            need(!c.read<uintptr_t>(c.image+0x1a41a80),"Dungeon Gizmo control requires idle Undo graph");const auto undo=c.singleton(0x13eb380),selection_manager=c.singleton(0x13eb630);const auto undo_before=undo_stack(c,undo);const auto prior=selection(c,selection_manager);std::vector<Json> prior_handles;for(auto p:prior)prior_handles.push_back(handle(c,p));
            c.verify_original(0x2d9000);uintptr_t expected_graph=0,created=0;
            if(operation=="dungeons.destroy_gizmo"){
                pin(c,{0x2d7b10,2416,"8eb1851d6ce5e612ad954322fe36d26ee8ebc5fcaa4545a74b30d065e5137e34"});const NativeArray row{c.image+0x174eff0,&target,1,1};attempted=true;seh(selection_native(c.image,selection_manager,&row,true),"DestroyProps");
            }else if(operation=="dungeons.duplicate_gizmo"){
                need(c.read<uintptr_t>(c.image+0x174fe50+0xd8)==c.image+0xe52ae0 && c.read<uintptr_t>(c.image+0x174fe50+0xe8)==c.image+0x1456a0 && c.read<uintptr_t>(c.image+0x174fe50+0x278)==c.image+0xe52b30,"Dungeon Gizmo duplication virtual methods differ");
                for(uintptr_t rva:{0xe52ae0,0x1456a0,0xe52b30,0x55280,0x267d30,0x5c6c50,0x268400,0xa0d230,0x5466b0,0x543230,0x546d20,0xa09f30,0x5441f0,0xa21c10,0x541790,0x11be4a0,0x11c8b80,0x268750})c.verify_original(rva);
                int32_t price=-1;bool allowed=false;seh(duplicate_price(target,price,allowed),"Gizmo duplicate capability/price");need(allowed && price>=0,"Dungeon Gizmo native CanDuplicate/price rejected target");
                bool affordable=false;int32_t actual_type_cost=0;uintptr_t same_region=0;bool same_offline=false;seh(placement_probe(c.image,o.dungeon,c.singleton(0x13eb220),o.shard,&key,&pose,actual_type_cost,same_offline,affordable,same_region),"duplicate affordability");need(affordable && actual_type_cost==price,"Dungeon Gizmo duplicate native price/affordability differs");
                Placement receipt;attempted=true;const auto code=duplicate_native(c.image,c.singleton(0x13eb220),undo,target,&pose,price,receipt);need(!code && receipt.committed && !receipt.begin_cleanup && !receipt.end_cleanup && !receipt.args_cleanup && !receipt.label_cleanup,"Dungeon Gizmo Duplicate failed SEH="+std::to_string(code)+" begin_cleanup="+std::to_string(receipt.begin_cleanup)+" end_cleanup="+std::to_string(receipt.end_cleanup)+" args_cleanup="+std::to_string(receipt.args_cleanup)+" label_cleanup="+std::to_string(receipt.label_cleanup)+"; do not replay");expected_graph=receipt.graph;created=receipt.object;
            }else{
                for(uintptr_t rva:{0xa09f30,0xa21c10,0x268400,0x53fea0,0x564f70,0x565410,0xcfcd40,0x53ded0,0x268750,0x11be4a0,0x11c8b80,0xe52bb0,0x1478a0})c.verify_original(rva);
                MoveReceipt receipt;attempted=true;const auto code=move_native(c.image,target,&pose,receipt);need(!code && receipt.moved && !receipt.move_cleanup && !receipt.close_cleanup && !receipt.end_cleanup && !receipt.args_cleanup && !receipt.label_cleanup,"Dungeon Gizmo Move failed SEH="+std::to_string(code)+" move_cleanup="+std::to_string(receipt.move_cleanup)+" close_cleanup="+std::to_string(receipt.close_cleanup)+" end_cleanup="+std::to_string(receipt.end_cleanup)+" args_cleanup="+std::to_string(receipt.args_cleanup)+" label_cleanup="+std::to_string(receipt.label_cleanup)+"; do not replay");expected_graph=receipt.graph;
            }
            std::vector<uintptr_t> restored;for(size_t i=0;i<prior.size();++i){if(operation=="dungeons.destroy_gizmo" && prior[i]==target)continue;need(c.resolve(prior_handles[i])==prior[i],"Dungeon control changed another selected object's UID lifetime");restored.push_back(prior[i]);}
            const NativeArray restore{c.image+0x174eff0,restored.data(),int32_t(restored.size()),int32_t(restored.size())};seh(selection_native(c.image,selection_manager,&restore,false),"restore Selection");need(selection(c,selection_manager)==restored,"Dungeon control Selection readback differs");committed_graph(c,undo,undo_before,expected_graph);
            const auto after=gizmos(c,owner(c,args),args);if(operation=="dungeons.destroy_gizmo"){auto expected=members;std::erase(expected,target);need(after==expected,"Dungeon DestroyProps surviving scene roster differs");result={{"destroyed",true}};}
            else if(operation=="dungeons.duplicate_gizmo"){need(created && after.size()==members.size()+1 && after.back()==created && std::equal(members.begin(),members.end(),after.begin()),"Dungeon Duplicate changed surviving scene roster");result=gizmo_json(c,created);need(result.at("type")==before.at("type") && result.at("variant")==before.at("variant") && result.at("position")==Json(pose.position) && result.at("rotation")==Json(pose.rotation) && result.at("scale")==Json(pose.scale),"Dungeon Gizmo Duplicate authored readback differs");}
            else{need(after==members,"Dungeon Move changed scene roster");auto expected=before;expected["position"]=pose.position;expected["rotation"]=pose.rotation;expected["scale"]=pose.scale;result=gizmo_json(c,object(c,args.at("gizmo"),0x174fe50));need(result==expected,"Dungeon Gizmo Move authored readback differs");}
        } else if(operation=="dungeons.place_gizmo") {
            need(!args.contains("instance"),"Dungeon Gizmo placement targets the authored template");
            const auto type=args.at("type").get<std::string>(),variant=args.value("variant",std::string{}),tool_name=args.at("tool").get<std::string>();
            need(!type.empty() && type.size()<=256 && variant.size()<=256 && type.find('\0')==std::string::npos && variant.find('\0')==std::string::npos,"Dungeon Gizmo resource key is invalid");
            const int tool=tool_name=="door"?0:tool_name=="key"?1:tool_name=="treasure"?2:-1;need(tool>=0,"Dungeon Gizmo tool must be door, key or treasure");
            Pose pose;pose.position=numbers<3>(args.at("position"),"position");if(args.contains("rotation"))pose.rotation=numbers<4>(args.at("rotation"),"rotation");if(args.contains("scale"))pose.scale=numbers<3>(args.at("scale"),"scale");
            for(float x:pose.position)need(std::isfinite(x),"Dungeon placement position is non-finite");for(float x:pose.scale)need(std::isfinite(x) && x>0,"Dungeon placement scale must be finite and positive");
            float norm=0;for(float x:pose.rotation){need(std::isfinite(x),"Dungeon placement rotation is non-finite");norm+=x*x;}need(std::abs(norm-1)<0.002f,"Dungeon placement rotation must be a normalized quaternion");
            for(uintptr_t rva:{0xa36a0,0x147b20,0x55200,0x50eb00,0xa09f30,0x148a50,0x5149b0,0x267d30,0x5c6c50,0x268400,0x154a10,0xa5c10,0x146980,0xa0d230,0x5466b0,0x543230,0x546d20,0x5441f0,0xa21c10,0x541790,0x11be4a0,0x11c8b80,0x268750})c.verify_original(rva);
            need(c.read<uintptr_t>(c.image+0x1a41a80)==0,"Dungeon Gizmo placement requires idle native Undo");const auto before=gizmos(c,o,args);const auto manager=c.read<uintptr_t>(o.region+0x9a8),business=c.singleton(0x13eb220),undo=c.singleton(0x13eb380),sound=c.singleton(0x13eb450);
            need(business && undo && (tool!=2 || sound),"Dungeon placement Business/Undo/Sound singleton is absent");const Text key{type.data(),uint64_t(type.size())},variant_key{variant.data(),uint64_t(variant.size())};
            int32_t cost=-1;bool offline=false,affordable=false;uintptr_t destination=0;seh(placement_probe(c.image,o.dungeon,business,o.shard,&key,&pose,cost,offline,affordable,destination),"placement preflight");
            need(offline && instances(c,o).empty(),"Dungeon Gizmo placement requires original offline Dungeon with zero Instances");need(cost>=0 && affordable,"Dungeon native Gizmo resource is unaffordable or has invalid price");need(destination==o.region,"Dungeon placement position leaves its template Region");
            const auto undo_before=undo_stack(c,undo);Placement receipt;attempted=true;const auto code=place_gizmo_native(c.image,o.dungeon,manager,undo,sound,&key,&variant_key,&pose,tool,cost,receipt);
            need(!code && !receipt.begin_cleanup && !receipt.end_cleanup && !receipt.args_cleanup && !receipt.label_cleanup && receipt.committed,"Dungeon Gizmo placement transaction failed SEH="+std::to_string(code)+" begin_cleanup="+std::to_string(receipt.begin_cleanup)+" end_cleanup="+std::to_string(receipt.end_cleanup)+" args_cleanup="+std::to_string(receipt.args_cleanup)+" label_cleanup="+std::to_string(receipt.label_cleanup)+"; do not replay");
            committed_graph(c,undo,undo_before,receipt.graph);const auto after=gizmos(c,owner(c,args),args);need(after.size()==before.size()+1 && after.back()==receipt.object && std::equal(before.begin(),before.end(),after.begin()),"Dungeon Gizmo placement roster readback differs");
            result=gizmo_json(c,receipt.object);need(result.at("type")==type && result.at("variant")==variant && result.at("position")==Json(pose.position) && result.at("rotation")==Json(pose.rotation) && result.at("scale")==Json(pose.scale),"Dungeon Gizmo placement authored readback differs");result["cost"]=cost;
        } else if(operation=="dungeons.rename_gizmo") {
            const auto target=object(c,args.at("gizmo"),0x174fe50);const auto members=gizmos(c,o,args);need(std::find(members.begin(),members.end(),target)!=members.end(),"Dungeon Gizmo does not belong to the selected scene");
            const auto name=args.at("name").get<std::string>();need(!name.empty() && name.size()<=256 && name.find('\0')==std::string::npos,"Dungeon Gizmo name must contain 1..256 bytes without NUL");
            pin(c,{0x4d5490,16,"9c8ca9aedcea34fab0b67d35965958bb416129b7d3ed5a144ed05dd9b2c1207e"});const Text text{name.data(),uint64_t(name.size())};
            attempted=true;seh(rename_native(c.image,target,&text),"Gizmo Rename");result=gizmo_json(c,object(c,args.at("gizmo"),0x174fe50));need(result.at("name")==name,"Dungeon Gizmo name readback differs");
        } else if(operation=="dungeons.set_gizmo_runtime") {
            const auto target=object(c,args.at("gizmo"),0x174fe50);const auto members=gizmos(c,o,args);need(std::find(members.begin(),members.end(),target)!=members.end(),"Dungeon Gizmo does not belong to the selected scene");
            const auto& state=args.at("state");need(state.is_object() && !state.empty(),"Dungeon Gizmo runtime state must contain at least one field");
            for(const auto& [name,value]:state.items()){
                need(name=="triggered" || name=="state" || name=="immediate" || name=="on_map","Dungeon Gizmo runtime field is unsupported: "+name);
                if(name=="triggered" || name=="on_map")need(value.is_boolean(),"Dungeon Gizmo runtime boolean required: "+name);
                else need(value.is_number_integer() && value.get<int64_t>()>=0 && value.get<int64_t>()<=2,"Dungeon Gizmo Door state must be 0..2");
            }
            const auto before=gizmo_json(c,target);auto expected=before;
            if(state.contains("on_map") && state.at("on_map")!=before.at("on_map")){
                pin(c,{0x148fc0,448,"16d87357caa1ce4e45d374f4e7c796f4f3aec760936cfa4a6437c1448a0448b3"});
                attempted=true;seh(on_map_native(c.image,target,state.at("on_map").get<bool>()),"Gizmo SetOnMap");expected["on_map"]=state.at("on_map");need(gizmo_json(c,object(c,args.at("gizmo"),0x174fe50))==expected,"Dungeon Gizmo SetOnMap changed unexpected authored fields");
            }
            for(const auto& [name,offset]:std::array<std::pair<const char*,size_t>,3>{{{"triggered",0x250},{"state",0x254},{"immediate",0x328}}}){
                if(!state.contains(name) || state.at(name)==expected.at(name))continue;
                need(gizmo_json(c,object(c,args.at("gizmo"),0x174fe50))==expected,"Dungeon Gizmo changed before runtime CAS");
                const bool byte=offset==0x250;const int32_t prior=byte?int32_t(expected.at(name).get<bool>()):expected.at(name).get<int32_t>();const int32_t desired=byte?int32_t(state.at(name).get<bool>()):state.at(name).get<int32_t>();bool exchanged=false;
                attempted=true;seh(runtime_scalar(target+offset,byte,prior,desired,exchanged),"Gizmo runtime CAS");need(exchanged,"Dungeon Gizmo runtime CAS prior value differs; do not replay");expected[name]=state.at(name);
            }
            result=gizmo_json(c,object(c,args.at("gizmo"),0x174fe50));need(result==expected,"Dungeon Gizmo runtime final readback differs");
        } else if(operation=="dungeons.set_palette") {
            pin(c,{0x970590,407,"8889b8a130368f7924544d45b67e57d18248bd5a1489a5a700246f2af8609842"});
            pin(c,{0x9708c0,148,"bb4624bbc533d99539535f19391f65e947168a081318a35d4e7eae53082253f9"});
            pin(c,{0x97bcb0,93,"8c536acbf69b9ba24c1cae7f4698681894a777a5080eaa146f925ecd86be5398"});
            pin(c,{0x8aad90,114,"58a73d6165cc7ad88d4526cfdbc015529feb73f539b86db9b2027478ad89c653"});
            pin(c,{0xe4ba70,236,"77e7f993efeff1def37672c1478bc00736a840302bd14bc9749406f5db9ea6fe"});
            const auto colors=color_input(args.at("colors"));const bool defaults=args.value("defaults",false);
            std::array<uint32_t,4> packed{};for(size_t i=0;i<4;++i){for(float x:colors[i])need(std::isfinite(x) && x>=0 && x<=1,"Dungeon palette channels must be finite 0..1");seh(pack_color(c.image,&packed[i],&colors[i]),"ColorPacked conversion");}
            const auto before=layout(c,o);need(c.read<uintptr_t>(o.dungeon+0x1a0)==c.image+0x1732320,"Dungeon ColorConfig class differs");const auto data=c.read<uintptr_t>(o.dungeon+0x1c0);
            CommandReceipt receipt;attempted=true;seh(palette_native(c.image,o.dungeon,data,defaults,&colors,receipt),"palette leaves");seh(receipt.cleanup,"palette record cleanup");need(receipt.returned && receipt.accepted,"Dungeon palette native boolean loader rejected flag");
            const auto after=layout(c,owner(c,args));need(c.read<uint8_t>(o.dungeon+0x1d0)==uint8_t(defaults) && std::equal(packed.begin(),packed.end(),after.palette.begin()),"Dungeon palette/default flag readback differs");
            need(before.cells.size()==after.cells.size(),"Dungeon palette changed cell count");for(size_t i=0;i<before.cells.size();++i)need(before.cells[i].pointer==after.cells[i].pointer && before.cells[i].open==after.cells[i].open && before.cells[i].theme==after.cells[i].theme && before.cells[i].colors==after.cells[i].colors,"Dungeon palette changed existing cells");result={{"palette",after.palette},{"defaults",defaults}};
        } else if(operation=="dungeons.set_lighting") {
            for(const auto& p:light_pins)pin(c,p);
            LightInput input{integers<4>(args.at("spinners"),"light Spinners"),integers<3>(args.at("sliders"),"light Sliders"),color_input(args.at("colors"))};
            for(const auto& color:input.colors)for(float x:color)need(std::isfinite(x) && x>=0 && x<=1,"Dungeon light color channels must be finite 0..1");
            const auto w=window(c,"dungeon_light",0x17324a0);const auto widgets=light_widgets(c,w);std::array<float,23> expected{};
            seh(light_numbers(c.image,&widgets,&input,&expected),"lighting numeric conversion");for(auto x:expected)need(std::isfinite(x),"Dungeon native lighting conversion is non-finite");
            attempted=true;seh(light_prepare(c.image,w,o.dungeon,&widgets,&input),"lighting Window preparation");need(c.read<uintptr_t>(w+0x4e8)==o.dungeon,"Dungeon lighting Window owner differs");
            for(size_t i=0;i<4;++i)need(c.read<int32_t>(widgets.spinners[i]+0x44c)==input.spinners[i] && c.read<std::array<float,4>>(widgets.colors[i]+0x2c8)==input.colors[i],"Dungeon lighting native Spinner/color readback differs");
            for(size_t i=0;i<3;++i)need(c.read<int32_t>(widgets.sliders[i]+0x4d0)==input.sliders[i],"Dungeon lighting native Slider readback differs");
            command(c,0x6db7a0,w,"MainLight",attempted);need(c.read<std::array<float,23>>(owner(c,args).dungeon+0x1f0)==expected,"Dungeon lighting authored readback differs");result={{"light",expected}};
        } else if(cell) {
            for(const auto& p:cell_pins)pin(c,p);
            c.verify_original(0x1270520);
            const int mode=operation=="dungeons.close_cell"?0:operation=="dungeons.open_cell"?1:operation=="dungeons.set_cell_theme"?2:3;
            const auto before=layout(c,o);const int32_t index=integer(args.at("cell"),0,INT32_MAX,"cell index");need(size_t(index)<before.cells.size(),"Dungeon cell index is outside layout");
            const auto& prior=before.cells[size_t(index)];std::string theme=mode==1 || mode==2?args.at("theme").get<std::string>():prior.theme;
            need(theme.size()<=256,"Dungeon Theme key exceeds native bound");const Text key{theme.data(),uint64_t(theme.size())};
            if(mode==1 || mode==2){need(!theme.empty(),"Dungeon Theme key is empty");uintptr_t resource=0;seh(theme_native(c.image,c.singleton(0x13eb700),&key,resource),"Theme lookup");need(resource && c.read<uintptr_t>(resource)==c.image+0x1705870,"Dungeon Theme resource is absent or wrong type");}
            if((mode==0 && !prior.open) || (mode==1 && prior.open)){result={{"cell",index},{"changed",false}};cause.clear();return true;}
            need(mode<=1 || prior.open,"Dungeon cell must be open before applying Theme or palette");
            if(mode<=1){bool allowed=false;seh(cell_allowed(c.image,o.dungeon,index,mode==1,allowed),"cell eligibility");need(allowed,"Dungeon native cell eligibility/obstruction guard rejected edit");}
            need(c.read<uintptr_t>(c.image+0x1a41a80)==0,"Dungeon cell edit requires no active native Undo graph");const auto undo=c.singleton(0x13eb380);need(undo!=0,"Dungeon Undo singleton is absent");
            const auto undo_before=undo_stack(c,undo);CellReceipt receipt;attempted=true;const auto code=cell_native(c.image,o.dungeon,undo,index,mode,&key,receipt);
            const std::string cleanup=" active="+std::to_string(receipt.active_cleanup)+" end="+std::to_string(receipt.end_cleanup)+" marker="+std::to_string(receipt.marker_cleanup)+" args="+std::to_string(receipt.args_cleanup)+" text="+std::to_string(receipt.text_cleanup);
            need(!code && !receipt.active_cleanup && !receipt.end_cleanup && !receipt.marker_cleanup && !receipt.args_cleanup && !receipt.text_cleanup && receipt.committed,
                 "Dungeon cell native transaction failed SEH="+std::to_string(code)+cleanup+"; do not replay");
            committed_graph(c,undo,undo_before,receipt.graph);
            const auto after=layout(c,owner(c,args));need(after.cells.size()==before.cells.size(),"Dungeon cell transaction changed layout ownership/count");
            for(size_t i=0;i<before.cells.size();++i)need(before.cells[i].pointer==after.cells[i].pointer,"Dungeon cell transaction changed owned Cell slot identity");const auto& current=after.cells[size_t(index)];
            need(mode==0?!current.open:current.open,"Dungeon cell open flag readback differs");
            if(mode==1 || mode==2)need(current.theme==theme,"Dungeon cell Theme readback differs");
            if(mode==3)need(std::equal(current.colors.begin(),current.colors.end(),after.palette.begin()),"Dungeon cell palette readback differs");
            result={{"cell",index},{"changed",true},{"open",current.open},{"theme",current.theme},{"colors",current.colors}};
        } else if(instance) {
            const int kind=operation=="dungeons.make_instance"?0:operation=="dungeons.destroy_instance"?1:operation=="dungeons.reset_instance"?2:3;
            pin(c,instance_pins[kind]);const auto before=instances(c,o);const auto counter=c.read<int32_t>(o.dungeon+0x414);need(counter>=0 && (kind || counter<INT32_MAX),"Dungeon Instance next saved index is exhausted or invalid");uintptr_t target=0,made=0;int32_t wanted=-1;
            if(kind){wanted=integer(args.at("instance"),0,INT32_MAX,"Instance index");for(const auto& row:before)if(row.index==wanted)target=row.pointer;need(target!=0,"Dungeon Instance saved index is absent");}
            attempted=true;seh(lifecycle(c.image,kind,o.dungeon,target,made),"Instance lifecycle");const auto after=instances(c,owner(c,args));
            const auto same=[](const Instance& a,const Instance& b){return a.pointer==b.pointer && a.index==b.index && a.shard==b.shard && a.region==b.region;};
            auto expected=before;if(kind==1)std::erase_if(expected,[&](const auto& row){return row.pointer==target;});
            need(c.read<int32_t>(o.dungeon+0x414)==counter+(kind==0?1:0),"Dungeon lifecycle next saved index readback differs");
            need(after.size()>=expected.size() && std::equal(expected.begin(),expected.end(),after.begin(),same),"Dungeon lifecycle replaced or reordered surviving native Instances");
            if(kind==0){need(made && after.size()==before.size()+1 && after.back().pointer==made && after.back().index==counter,"Dungeon MakeInstance roster effect differs");result=instance_json(after.back());}
            else if(kind==1){need(after.size()+1==before.size() && std::none_of(after.begin(),after.end(),[&](const auto& row){return row.index==wanted;}),"Dungeon DestroyInstance roster effect differs");result={{"destroyed",wanted}};}
            else {need(after.size()==before.size(),"Dungeon reset changed Instance pool size");const auto found=std::find_if(after.begin(),after.end(),[&](const auto& row){return row.index==wanted;});need(found!=after.end() && found->pointer==target && !found->started,"Dungeon reset did not confirm retained unstarted Instance");result=instance_json(*found);}
        } else if(operation=="dungeons.configure_instances") {
            const int32_t count=integer(args.at("count"),1,10,"desired Instance count");
            pin(c,{0x6b5d80,348,"5c7c4e1c10769cba6e3f40297322de23210225222e40733f81c72f088b51a331"});
            pin(c,{0x6b5ee0,24,"e0e8f9900dfde377d73258a11bd39bc13d3df89245a5202c93d2626e247b50cd"});
            pin(c,{0x6b4fb0,3527,"43cb0bb61db029186204f9b08b0f93b83a3d4e2cabf02f580a7fa424f01ad2b4"});
            const auto w=window(c,"instance_config_window",0x1742280);attempted=true;seh(config_prepare(c.image,w,o.dungeon,count),"configure Window");
            need(c.read<uintptr_t>(w+0x510)==o.dungeon && c.read<int32_t>(w+0x508)==count,"Dungeon prepared config Window differs");
            command(c,0x6b5d80,w,"Accept",attempted);need(c.read<int32_t>(owner(c,args).dungeon+0x40c)==count,"Dungeon desired Instance count readback differs");result={{"desired_instances",count}};
        } else {
            const int32_t atmosphere=integer(args.at("atmosphere"),0,5,"atmosphere");
            pin(c,{0x6dc410,215,"e3c5af3a1c3617dc9945d4a323306c4b6290580daa4b0af240e584fdd947ee3b"});
            pin(c,{0x6dc0f0,785,"6d46cbc6d9d7f227b3d511f9283e8e6b42823248b7126a57ecbc167f90ab640f"});
            const auto w=window(c,"dungeon_sound",0x1732760);attempted=true;seh(sound_prepare(c.image,w,o.dungeon),"sound Window");
            need(c.read<uintptr_t>(w+0x4e8)==o.dungeon,"Dungeon sound Window owner differs");command(c,0x6dc410,w,"SetAtmos "+std::to_string(atmosphere),attempted);
            need(c.read<int32_t>(owner(c,args).dungeon+0x830)==atmosphere,"Dungeon atmosphere readback differs");result={{"atmosphere",atmosphere}};
        }
        cause.clear();return true;
    });
}
}
