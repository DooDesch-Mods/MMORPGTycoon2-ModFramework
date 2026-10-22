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
    c.require_world(); Owner out;out.exterior=object(c,args.at("handle"),0x16d93c0);
    const auto map=c.singleton(0x13df8c0);need(map && c.read<uintptr_t>(map)==c.image+0x1742860,"Dungeon current WorldMap class differs");
    need(c.read<uintptr_t>(out.exterior+0x170)==map+0x1c0,"Dungeon exterior does not belong to the current root world");
    out.dungeon=c.read<uintptr_t>(out.exterior+0xa38);
    need(out.dungeon && c.read<uintptr_t>(out.dungeon)==c.image+0x16c0560 && c.read<uintptr_t>(out.dungeon+0x6f8)==out.exterior,"Dungeon reciprocal exterior owner differs");
    out.shard=c.read<uintptr_t>(out.dungeon+0x48);out.region=c.read<uintptr_t>(out.dungeon+0x50);
    need(out.shard && out.region && c.read<uintptr_t>(out.shard)==c.image+0x174ad20 && c.read<int32_t>(out.shard+0x48)==1 &&
         c.read<uintptr_t>(out.shard+0x18)==out.dungeon && c.read<uintptr_t>(out.region)==c.image+0x1787930 && c.read<uintptr_t>(out.region+0x70)==out.shard,
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
        need(c.read<uintptr_t>(p)==c.image+0x1719450 && row.index>=0 && ids.insert(row.index).second && started<=1 && c.read<uintptr_t>(p+0x30)==o.dungeon &&
             row.shard && row.region && c.read<uintptr_t>(row.shard+0x20)==p && c.read<uintptr_t>(row.region+0x70)==row.shard,
             "Dungeon Instance index/owner/Shard/Region differs");row.started=started!=0;out.push_back(row);
    }return out;
}
Json instance_json(const Instance& row) {return {{"index",row.index},{"started",row.started}};}
struct Cell {uintptr_t pointer;bool open;std::string theme;std::array<uint32_t,4> colors;};
struct Layout {std::vector<Cell> cells;std::vector<uint32_t> palette;};
Layout layout(Context& c,const Owner& o) {
    Layout out;need(c.read<uintptr_t>(o.dungeon+0x108)==c.image+0x1706690,"Dungeon Layout class differs");
    const auto slots=array(c,o.dungeon+0x120);need(size_t(c.read<int32_t>(o.dungeon+0x80))==slots.size(),"Dungeon grid/layout cell counts differ");
    for(auto p:slots){const auto open=c.read<uint8_t>(p+0x60);need(c.read<uintptr_t>(p)==c.image+0x1723990 && c.read<uintptr_t>(p+0x38)==c.image+0x17378b0 && open<=1,"Dungeon layout Cell/Data class differs");
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
    need(manager && c.read<uintptr_t>(manager)==c.image+0x1700260 && c.read<uintptr_t>(manager+0x68)==shard && c.read<uintptr_t>(manager+0x70)==region,"Dungeon Gizmo manager scene ownership differs");
    auto rows=array(c,manager+0x18);for(auto p:rows){need(c.read<uintptr_t>(p)==c.image+0x1749ab0 && c.read<uintptr_t>(p+0x170)==shard && c.read<uintptr_t>(p+0x178)==region,"Dungeon Gizmo scene membership differs");
        need(object(c,handle(c,p),0x1749ab0)==p,"Dungeon Gizmo original UID registry differs");}return rows;
}
std::vector<uintptr_t> monsters(Context& c,const Owner& o,const Json& args) {
    uintptr_t shard=0;const auto region=scene_region(c,o,args,shard),manager=c.read<uintptr_t>(region+0x988);
    need(manager && c.read<uintptr_t>(manager)==c.image+0x1714100 && c.read<uintptr_t>(manager+0x80)==region,"Dungeon Monster manager reciprocal Region differs");
    const auto rows=array(c,manager+0x38);for(auto p:rows)need(c.read<uintptr_t>(p)==c.image+0x16c0770 && c.read<uintptr_t>(p+0x170)==shard && c.read<uintptr_t>(p+0x178)==region && object(c,handle(c,p),0x16c0770)==p,"Dungeon Monster class/full UID/scene membership differs");return rows;
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
    const Text key{name,size};__try {p=reinterpret_cast<uintptr_t(*)(const Text&)>(image+0x81aa90)(key);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uintptr_t window(Context& c,const char* name,uintptr_t ap) {
    c.verify_original(0x81aa90,5);
    uintptr_t p=0;seh(window_native(c.image,name,std::strlen(name),p),"Window lookup");need(p && c.read<uintptr_t>(p)==c.image+ap,"Dungeon native named Window class differs");return p;
}
struct CommandReceipt {bool constructed=false,returned=false,accepted=false;uint32_t cleanup=0;};
uint32_t command_native(uintptr_t image,uintptr_t method,uintptr_t receiver,const char* command,CommandReceipt& r) {
    alignas(8) unsigned char record[0x78]{};uint32_t code=0;
    __try {reinterpret_cast<void(*)(void*,const char*)>(image+0x967370)(record,command);r.constructed=true;
        r.accepted=reinterpret_cast<bool(*)(uintptr_t,void*)>(image+method)(receiver,record);r.returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(r.constructed){__try{reinterpret_cast<void(*)(void*)>(image+0x9676a0)(record);}__except(EXCEPTION_EXECUTE_HANDLER){r.cleanup=GetExceptionCode();}}return code;
}
void command(Context& c,uintptr_t method,uintptr_t receiver,const std::string& label,bool& attempted) {
    pin(c,{0x967370,407,"6baa3c9cdf746116d6c6788539dfb0a35cd8a778e1c8131549eda3e4b04ed81d"});
    pin(c,{0x9676a0,148,"7f6e66c76e2be9e2d7f43ab04d8721cc060f16ddcd111a422605af135ab1bb4a"});
    CommandReceipt r;attempted=true;seh(command_native(c.image,method,receiver,label.c_str(),r),"UI command");seh(r.cleanup,"UI record cleanup");need(r.returned && r.accepted,"Dungeon native UI command did not accept; inspect state before retry");
}
uint32_t config_prepare(uintptr_t image,uintptr_t window,uintptr_t dungeon,int32_t count) {
    __try {reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x6aebc0)(window,dungeon);*reinterpret_cast<int32_t*>(window+0x508)=count;
        reinterpret_cast<void(*)(uintptr_t)>(image+0x6adc90)(window);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t sound_prepare(uintptr_t image,uintptr_t window,uintptr_t dungeon) {
    __try {*reinterpret_cast<uintptr_t*>(window+0x4e8)=dungeon;reinterpret_cast<void(*)(uintptr_t)>(image+0x6d4cd0)(window);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t lifecycle(uintptr_t image,int kind,uintptr_t dungeon,uintptr_t target,uintptr_t& made) {
    __try {
        if(kind==0)made=reinterpret_cast<uintptr_t(*)(uintptr_t)>(image+0xa7670)(dungeon);
        else if(kind==1)reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0xa7a00)(dungeon,target);
        else reinterpret_cast<void(*)(uintptr_t)>(image+(kind==2?0xb6b40:0xb66e0))(target);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
constexpr Pin instance_pins[]{
    {0xa7670,792,"50506183d19615df1ebae10c45d538120cc030d0aabb01fcc974e1e46d36fa8a"},
    {0xa7a00,1320,"0cc9c9d7ada6454b0c0fdcf62a7a3791c018ca81a9876bb8d22d93b5a7c97b6e"},
    {0xb6b40,69,"4a99d44a3f9947070f18777b652ff2da01ad98c8ac04cc5bf114098b36332182"},
    {0xb66e0,1091,"a02bba835291a7c9026dff200783bf80dd2c9d436cc0ac34cab45b781e7e615a"}};
constexpr Pin cell_pins[]{
    {0xa41f0,117,"2a2c260c3a12c265af9113276439e94361e65b5905c91e5bc99a8156b5abb6e3"},
    {0xa4c60,244,"cf44a9f25fa3e6d28856a16a8e14cfbde2198986ee6c5504e80f0dc42a42fa7a"},
    {0xa81d0,328,"88a8f02370af099a1da9ec88a2f84ab77e9411ddd24e7a33799fa12a42b12626"},
    {0xa8320,373,"218496fe1c0a970676c619af22ea6d9f5938ca97263126ad6a0ec4d886536f31"},
    {0xa84e0,644,"f83ae9d9c7392356ad42218e1df2bfbfba737d52caf046381b0dee03d533273c"},
    {0xa8b80,118,"f09ed40e658ca5a471d4348e45a9494c3ddcf3c5b7096ff1243bf2a06ad8e4b3"},
    {0xcab90,131,"08c80326a51c6ecf1c864225f7cb6c24fd3297170ca0de87829dec2c491564b6"},
    {0x265650,4313,"38011d862a83350cd09c8b0f89ad97503c6030a6d623ab1ad1c8d7ef428d7220"},
    {0x537840,68,"1d11452a0fa92de4a2cddbe821675930f45fa3dacbcb32aafd7c916a65fe76fe"},
    {0x53b260,649,"d2ed6c2b0734c126a77570a04a626f2414d4e8beb57bed10b5f81a54be959514"},
    {0x544680,797,"b7c8960f2546cd211310decd66f7c683db941684b24c84a627b5e53415a0d0b7"},
    {0x59d230,350,"17757ab45ebae98557bfd032f904f95109b6f2e48a94e28e8e9a8333f9133597"},
    {0x59d390,1711,"f71a2f85a37dfae01c8f05caed62daf2f94024bc46a00cf5dda8d77ff99eddbe"},
    {0xa16f10,180,"2594640aa4208824aae60cdbc2fe587344beae9c01075cee18a4967171aa09ff"},
    {0x11b2b00,169,"5b7aa40c98ff98b519edcd7e80939117f39f09026153bce7dbadbb33198a1c2d"},
    {0x11bd1e0,25,"597e3ae5eeab016d15fdf122e88327caaae59fd7855a926a01d2f24c3747e8c5"}};
uint32_t cell_allowed(uintptr_t image,uintptr_t dungeon,int32_t index,bool opening,bool& allowed) {
    __try {
        if(opening)allowed=reinterpret_cast<bool(*)(uintptr_t,int32_t)>(image+0xa41f0)(dungeon,index);
        else allowed=!reinterpret_cast<bool(*)(uintptr_t,uintptr_t,int32_t,bool)>(image+0x59d390)(0,dungeon,index,false);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t theme_native(uintptr_t image,uintptr_t manager,const Text* theme,uintptr_t& result) {
    __try {result=reinterpret_cast<uintptr_t(*)(uintptr_t,const Text&)>(image+0xcab90)(manager,*theme);return 0;}
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
        reinterpret_cast<void(*)(void*,const Text&)>(image+0x59d230)(storage,edit_label);
        r.graph=*reinterpret_cast<uintptr_t*>(storage+0x338);r.marker=*reinterpret_cast<uintptr_t*>(storage+0x348);
        r.edit_open=r.marker && *reinterpret_cast<const uint8_t*>(r.marker)!=0;
        if(!r.graph || !r.marker || !r.edit_open)code=ERROR_INVALID_STATE;
        else {
            r.active=true;reinterpret_cast<void(*)(uintptr_t)>(image+0x537840)(r.graph);
            if(mode==0)r.changed=reinterpret_cast<bool(*)(uintptr_t,int32_t)>(image+0xa8320)(dungeon,index);
            else if(mode==1)r.changed=reinterpret_cast<bool(*)(uintptr_t,int32_t,const Text&)>(image+0xa84e0)(dungeon,index,*theme);
            else if(mode==2){reinterpret_cast<void(*)(uintptr_t,int32_t,const Text&)>(image+0xa81d0)(dungeon,index,*theme);r.changed=true;}
            else {reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0xa4c60)(dungeon,index);r.changed=true;}
            if(r.changed){reinterpret_cast<void(*)(uintptr_t)>(image+0x544680)(r.graph);reinterpret_cast<void(*)(uintptr_t,bool)>(image+0xa8b80)(dungeon,true);}
            else code=ERROR_INVALID_STATE;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();r.graph=*reinterpret_cast<uintptr_t*>(storage+0x338);r.marker=*reinterpret_cast<uintptr_t*>(storage+0x348);}
    if(r.active){__try{reinterpret_cast<void(*)(uintptr_t)>(image+0x537840)(0);r.active=false;}__except(EXCEPTION_EXECUTE_HANDLER){r.active_cleanup=GetExceptionCode();}}
    if(r.edit_open){__try{reinterpret_cast<void(*)()>(image+0x265650)();}__except(EXCEPTION_EXECUTE_HANDLER){r.end_cleanup=GetExceptionCode();}}
    if(r.marker){__try{reinterpret_cast<void(*)(uintptr_t,size_t)>(image+0x1264b60)(r.marker,1);r.marker=0;}__except(EXCEPTION_EXECUTE_HANDLER){r.marker_cleanup=GetExceptionCode();}}
    if(!code && !r.active_cleanup && !r.end_cleanup && !r.marker_cleanup){
        __try {reinterpret_cast<void(*)(void*,const char*)>(image+0xa16f10)(label,"Edit dungeon tiles");constructed=true;
            reinterpret_cast<void(*)(uintptr_t,const void*,uintptr_t)>(image+0x53b260)(undo,label,r.graph);r.committed=true;
        }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    }
    if(constructed){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.text_cleanup=GetExceptionCode();}
    }return code;
}
struct LightInput {std::array<int32_t,4> spinners;std::array<int32_t,3> sliders;std::array<std::array<float,4>,4> colors;};
struct LightWidgets {std::array<uintptr_t,4> spinners,colors;std::array<uintptr_t,3> sliders;};
LightWidgets light_widgets(Context& c,uintptr_t w) {
    LightWidgets out;constexpr size_t so[]{0x508,0x510,0x500,0x518},co[]{0x4f8,0x4f0,0x530,0x548},lo[]{0x528,0x538,0x540};std::set<uintptr_t> seen;
    const auto check=[&](uintptr_t p,uintptr_t ap){need(p && seen.insert(p).second && c.read<uintptr_t>(p)==c.image+ap,"Dungeon lighting widget class/uniqueness differs");};
    for(size_t i=0;i<4;++i){out.spinners[i]=c.read<uintptr_t>(w+so[i]);out.colors[i]=c.read<uintptr_t>(w+co[i]);check(out.spinners[i],0x16c1720);check(out.colors[i],0x17224a0);
        need(!c.read<uintptr_t>(out.spinners[i]+0x428) && !c.read<uintptr_t>(out.spinners[i]+0x430) && !c.read<uintptr_t>(out.colors[i]+0x370),"Dungeon lighting input has native gameplay bindings");}
    for(size_t i=0;i<3;++i){out.sliders[i]=c.read<uintptr_t>(w+lo[i]);check(out.sliders[i],0x1787a30);need(!c.read<uintptr_t>(out.sliders[i]+0x420) && !c.read<uintptr_t>(out.sliders[i]+0x428),"Dungeon lighting slider has native gameplay bindings");}return out;
}
uint32_t light_numbers(uintptr_t image,const LightWidgets* widgets,const LightInput* input,std::array<float,23>* output) {
    float spinner[4]{},slider[3]{};alignas(16) unsigned char angle[16]{};float vector[3]{},forward[2]{0,-1};uint64_t rotated=0;
    __try {
        for(size_t i=0;i<4;++i)spinner[i]=reinterpret_cast<float(*)(uintptr_t,int32_t)>(image+0x7bf9e0)(widgets->spinners[i],input->spinners[i]);
        for(size_t i=0;i<3;++i)slider[i]=reinterpret_cast<float(*)(uintptr_t,int32_t)>(image+0x7b95a0)(widgets->sliders[i],input->sliders[i]);
        reinterpret_cast<void(*)(void*,float)>(image+0x9eec90)(angle,spinner[1]*-0.01745329424738884f);
        rotated=reinterpret_cast<uint64_t(*)(const void*,const void*)>(image+0x9eefa0)(angle,forward);
        std::memcpy(vector,&rotated,4);vector[1]=1;std::memcpy(vector+2,reinterpret_cast<const unsigned char*>(&rotated)+4,4);
        reinterpret_cast<void(*)(void*)>(image+0x9ffd80)(vector);
        for(size_t i=0;i<4;++i){(*output)[i]=input->colors[0][i]*spinner[0];(*output)[7+i]=input->colors[1][i]*spinner[2];(*output)[11+i]=input->colors[2][i];(*output)[16+i]=input->colors[3][i];}
        for(size_t i=0;i<3;++i)(*output)[4+i]=vector[i];
        (*output)[15]=slider[0]*0.0010000000474974513f;(*output)[20]=slider[1]*0.009999999776482582f;(*output)[21]=slider[2];(*output)[22]=spinner[3];return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t light_prepare(uintptr_t image,uintptr_t w,uintptr_t dungeon,const LightWidgets* widgets,const LightInput* input) {
    __try {
        *reinterpret_cast<uintptr_t*>(w+0x4e8)=dungeon;
        for(size_t i=0;i<4;++i){reinterpret_cast<void(*)(uintptr_t,const void*,bool)>(image+0x839d30)(widgets->colors[i],input->colors[i].data(),false);
            reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0x7c2c60)(widgets->spinners[i],input->spinners[i]);}
        for(size_t i=0;i<3;++i)reinterpret_cast<void(*)(uintptr_t,int32_t,bool)>(image+0x7bbdd0)(widgets->sliders[i],input->sliders[i],false);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
constexpr Pin light_pins[]{
    {0x6d40b0,713,"1ad8ecbb064ecddeeee5936bb2e67430098874fb29180ac49eb105945a82ed19"},
    {0x6d4380,455,"0e741457047bfa2b023e98ee805b404f5decd287e588a731754628b0df74b78c"},
    {0x7b95a0,17,"6a1c053fd5d66b35bd798028ad140017ae60f17f0d43fcdfe7fec43c7e4f323b"},
    {0x7bbdd0,612,"3b44ae45b68ae31be9d9c3e90fe09bf5da31f9e93eef2b6857cb2f7137019395"},
    {0x7bf9e0,17,"5c0b1940d701d2edec21e468c9fdb3bceae6a9f8dee6410d0139dca26d8da7c6"},
    {0x7c2c60,356,"541bd29e30903332e7f9db685a733dba9dc7d3e6c37c505c50bb7eb83896f9b7"},
    {0x839d30,553,"b27fbda9da6bf1f012092e32deed3b930b16593fc7d45e45e9bcb0b481821d8d"},
    {0x9eec90,121,"619e4123b48f0c7f46ae3abbbfd2a37c63fbc9cddc2304de7382659894bae66a"},
    {0x9eefa0,117,"5766b29a4c6590ec7f9198cb5a587ec364da3430fb2103057bc95a333fbc14be"},
    {0x9ffd80,366,"a5d730b5db40198a08dc51475f49bc7ebc63dc7d859d04295b047eab6c10725c"}};
uint32_t pack_color(uintptr_t image,uint32_t* output,const std::array<float,4>* color) {
    __try {reinterpret_cast<void(*)(void*,const void*)>(image+0x972960)(output,color);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t palette_native(uintptr_t image,uintptr_t dungeon,uintptr_t data,bool defaults,const std::array<std::array<float,4>,4>* colors,CommandReceipt& r) {
    alignas(8) unsigned char record[0x78]{};uint32_t code=0;uintptr_t slots[4]{};
    __try {
        for(int32_t i=0;i<4;++i){slots[i]=reinterpret_cast<uintptr_t(*)(uintptr_t,int32_t)>(image+0xe3ff80)(dungeon+0x1b8,i);if(slots[i]!=data+size_t(i)*4)return ERROR_INVALID_ADDRESS;}
        reinterpret_cast<void(*)(void*,const char*)>(image+0x967370)(record,defaults?"useDefault true":"useDefault false");r.constructed=true;
        r.accepted=reinterpret_cast<bool(*)(void*,uintptr_t,uintptr_t)>(image+0x8a1c70)(record,dungeon+0x1d0,0);
        if(r.accepted){for(size_t i=0;i<4;++i)reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0x972960)(slots[i],&(*colors)[i]);r.returned=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(r.constructed){__try{reinterpret_cast<void(*)(void*)>(image+0x9676a0)(record);}__except(EXCEPTION_EXECUTE_HANDLER){r.cleanup=GetExceptionCode();}}return code;
}
uint32_t runtime_scalar(uintptr_t address,bool byte,int32_t expected,int32_t desired,bool& exchanged) {
    __try {
        if(byte)exchanged=uint8_t(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address),char(desired),char(expected)))==uint8_t(expected);
        else exchanged=_InterlockedCompareExchange(reinterpret_cast<volatile long*>(address),long(desired),long(expected))==expected;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t on_map_native(uintptr_t image,uintptr_t p,bool value) {
    __try{reinterpret_cast<void(*)(uintptr_t,bool)>(image+0x147f90)(p,value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t rename_native(uintptr_t image,uintptr_t p,const Text* name) {
    __try{reinterpret_cast<void(*)(uintptr_t,const Text*)>(image+0x4cf710)(p,name);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Pose {std::array<float,4> rotation{0,0,0,1};std::array<float,3> position{},scale{1,1,1};};
struct Placement {uintptr_t object=0,graph=0;bool purchased=false,committed=false,edit_open=false;uint32_t begin_cleanup=0,end_cleanup=0,args_cleanup=0,label_cleanup=0;};
uint32_t placement_probe(uintptr_t image,uintptr_t dungeon,uintptr_t business,uintptr_t shard,const Text* type,const Pose* pose,int32_t& cost,bool& offline,bool& affordable,uintptr_t& region) {
    __try {
        offline=reinterpret_cast<bool(*)(uintptr_t)>(image+0xa29a0)(dungeon);
        cost=reinterpret_cast<int32_t(*)(const Text&)>(image+0x146af0)(*type);
        affordable=cost>=0 && reinterpret_cast<bool(*)(uintptr_t,int32_t)>(image+0x54d00)(business,cost);
        region=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*,uintptr_t)>(image+0x5089b0)(shard,pose->position.data(),0);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t region_native(uintptr_t image,uintptr_t shard,const Pose* pose,uintptr_t& region) {
    __try{region=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*,uintptr_t)>(image+0x5089b0)(shard,pose->position.data(),0);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t place_gizmo_native(uintptr_t image,uintptr_t dungeon,uintptr_t manager,uintptr_t undo,uintptr_t sound,const Text* type,const Text* variant,const Pose* pose,int tool,int32_t cost,Placement& r) {
    alignas(16) unsigned char transform[0x70]{},edit_name[0x20]{},label[0x38]{};
    bool edit_name_ready=false,label_ready=false;uintptr_t inverse=0;int32_t edge[2]{-1,-1};uint32_t code=0;
    __try {
        reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(image+0x9ff370)(transform,pose->rotation.data(),pose->position.data(),pose->scale.data());
        r.purchased=reinterpret_cast<bool(*)(const Text&,const void*)>(image+0x147a20)(*type,pose->position.data());
        if(!r.purchased)code=ERROR_ACCESS_DENIED;
        else {
            if(tool==2)reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0x50e810)(sound,0x20);
            if(tool==0 && reinterpret_cast<bool(*)()>(image+0x264c30)()){
                reinterpret_cast<void(*)(void*,const char*)>(image+0x5c04b0)(edit_name,"Dungeon input");edit_name_ready=true;
                reinterpret_cast<void(*)(const void*)>(image+0x265300)(edit_name);r.edit_open=true;
            }
            r.object=reinterpret_cast<uintptr_t(*)(uintptr_t,const Text&,const Text&,const void*)>(image+0x153920)(manager,*type,*variant,transform);
            if(!r.object)code=ERROR_INVALID_HANDLE;
            else {
                if(tool==0){reinterpret_cast<void(*)(void*,uintptr_t,const void*)>(image+0xa4f10)(edge,dungeon,pose->position.data());reinterpret_cast<void(*)(uintptr_t,int32_t,int32_t)>(image+0x145950)(r.object,edge[0],edge[1]);}
                r.graph=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x110);
                if(!r.graph)code=ERROR_NOT_ENOUGH_MEMORY;
                else {
                    reinterpret_cast<void(*)(uintptr_t)>(image+0x5400f0)(r.graph);
                    if(tool==0 || tool==2){
                        inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xc8);
                        if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
                        else {reinterpret_cast<void(*)(uintptr_t,int32_t,const void*)>(image+0x53ccb0)(inverse,cost,pose->position.data());reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x540760)(r.graph,inverse);}
                    }
                    if(!code){
                        inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xc0);
                        if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
                        else {reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x53dc30)(inverse,r.object);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x540760)(r.graph,inverse);}
                    }
                    if(!code && tool==1 && cost!=0){
                        inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xc8);
                        if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
                        else {reinterpret_cast<void(*)(uintptr_t,int32_t,const void*)>(image+0x53ccb0)(inverse,cost,pose->position.data());reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x540760)(r.graph,inverse);}
                    }
                    if(!code){reinterpret_cast<void(*)(void*,const char*)>(image+0xa16f10)(label,"Place Dungeon Gizmo");label_ready=true;
                        reinterpret_cast<void(*)(uintptr_t,const void*,uintptr_t)>(image+0x53b260)(undo,label,r.graph);r.committed=true;}
                }
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(label_ready){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}
    }
    if(edit_name_ready){__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(edit_name);}__except(EXCEPTION_EXECUTE_HANDLER){r.begin_cleanup=GetExceptionCode();}}
    if(r.edit_open){__try{reinterpret_cast<void(*)()>(image+0x265650)();}__except(EXCEPTION_EXECUTE_HANDLER){r.end_cleanup=GetExceptionCode();}}
    return code;
}
struct NativeArray {uintptr_t ap;const uintptr_t* data;int32_t count,capacity;};
uint32_t selection_native(uintptr_t image,uintptr_t selection,const NativeArray* rows,bool destroy) {
    __try {reinterpret_cast<void(*)(uintptr_t,const NativeArray*)>(image+(destroy?0x2d42c0:0x2d5910))(selection,rows);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
std::vector<uintptr_t> selection(Context& c,uintptr_t manager) {
    need(manager && c.read<uintptr_t>(manager)==c.image+0x17205d0 && !c.read<uint8_t>(manager+0xd1) && !c.read<uint8_t>(manager+0xd3),"Dungeon control requires idle native SelectionManager");return array(c,manager+0x20);
}
std::vector<uintptr_t> undo_stack(Context& c,uintptr_t manager) {
    need(manager && c.read<uintptr_t>(manager)==c.image+0x16fe070 && c.read<uintptr_t>(manager+8)==c.image+0x16e1320,"Dungeon Undo manager/array class differs");return array(c,manager+8,20);
}
void committed_graph(Context& c,uintptr_t undo,const std::vector<uintptr_t>& before,uintptr_t expected=0) {
    const auto after=undo_stack(c,undo);need(!after.empty(),"Dungeon control original Undo commit is absent");const auto graph=after.back();
    need(detail::committed_history(before,after,array(c,undo+0x20,20).empty(),c.read<uint8_t>(graph+0x10c),c.read<int32_t>(graph+0x58),c.read<uintptr_t>(c.image+0x1a3aa80)),"Dungeon control original Undo/Redo history or locked settlement readback differs");
    const auto nodes=c.read<int32_t>(graph+0x10),capacity=c.read<int32_t>(graph+0x14);
    need((!expected || graph==expected) && c.read<uintptr_t>(graph)==c.image+0x16e32d0 && c.read<uintptr_t>(graph+0x18)==c.image+0x16e2dc0 && nodes>=0 && nodes<=100000 && capacity>=nodes && (!nodes || c.read<uintptr_t>(graph+8)) && c.read<int32_t>(graph+0x28)>0 && c.read<uintptr_t>(graph+0x48)==c.image+0x1748c50,"Dungeon control graph ownership/content differs");
}
struct MoveReceipt {uintptr_t graph=0;bool label=false,begun=false,opened=false,constructed=false,moved=false;uint32_t move_cleanup=0,close_cleanup=0,end_cleanup=0,args_cleanup=0,label_cleanup=0;};
uint32_t move_native(uintptr_t image,uintptr_t target,const Pose* pose,MoveReceipt& r) {
    alignas(16) unsigned char transform[0x70]{},label[0x38]{},action[0x130]{};uint32_t code=0;
    __try {
        reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(image+0x9ff370)(transform,pose->rotation.data(),pose->position.data(),pose->scale.data());
        reinterpret_cast<void(*)(void*,const char*)>(image+0xa16f10)(label,"Move");r.label=true;
        reinterpret_cast<void(*)(const void*)>(image+0x265300)(label);r.begun=true;
        r.graph=reinterpret_cast<uintptr_t(*)(const void*)>(image+0x539990)(label);r.opened=true;
        if(!r.graph)code=ERROR_INVALID_HANDLE;
        else {reinterpret_cast<void(*)(void*,uintptr_t,const void*)>(image+0x55e8e0)(action,target,transform);r.constructed=true;
            reinterpret_cast<void(*)(void*,uintptr_t)>(image+0x55ed80)(action,r.graph);r.moved=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(r.constructed){__try{reinterpret_cast<void(*)(void*)>(image+0xcf1600)(action);}__except(EXCEPTION_EXECUTE_HANDLER){r.move_cleanup=GetExceptionCode();}}
    if(r.opened && r.graph){__try{reinterpret_cast<void(*)()>(image+0x5379c0)();}__except(EXCEPTION_EXECUTE_HANDLER){r.close_cleanup=GetExceptionCode();}}
    if(r.begun){__try{reinterpret_cast<void(*)()>(image+0x265650)();}__except(EXCEPTION_EXECUTE_HANDLER){r.end_cleanup=GetExceptionCode();}}
    if(r.label){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}
    }return code;
}
uint32_t duplicate_price(uintptr_t p,int32_t& amount,bool& allowed) {
    __try{const auto table=*reinterpret_cast<const uintptr_t*>(p);allowed=reinterpret_cast<bool(*)(uintptr_t)>(*reinterpret_cast<const uintptr_t*>(table+0xd8))(p);amount=reinterpret_cast<int32_t(*)(uintptr_t)>(*reinterpret_cast<const uintptr_t*>(table+0x278))(p);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t container_feature(uintptr_t image,uintptr_t target,bool& container) {
    __try{container=reinterpret_cast<bool(*)(uintptr_t,int32_t)>(image+0x145560)(target,0);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t duplicate_native(uintptr_t image,uintptr_t business,uintptr_t undo,uintptr_t source,const Pose* pose,int32_t price,Placement& r) {
    alignas(16) unsigned char transform[0x70]{},name[0x20]{},label[0x38]{};uintptr_t inverse=0;bool name_ready=false,label_ready=false;uint32_t code=0;
    __try {
        r.purchased=price<=0 || reinterpret_cast<bool(*)(uintptr_t,int32_t,int32_t)>(image+0x54d80)(business,price,6);
        if(!r.purchased)code=ERROR_ACCESS_DENIED;
        else {
            if(reinterpret_cast<bool(*)()>(image+0x264c30)()){
                reinterpret_cast<void(*)(void*,const char*)>(image+0x5c04b0)(name,"Duplicating collection");name_ready=true;
                reinterpret_cast<void(*)(const void*)>(image+0x265300)(name);r.edit_open=true;
            }
            r.graph=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x110);
            if(!r.graph)code=ERROR_NOT_ENOUGH_MEMORY;
            else {
                reinterpret_cast<void(*)(uintptr_t)>(image+0x5400f0)(r.graph);inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xc8);
                if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
                else {
                    reinterpret_cast<void(*)(uintptr_t,int32_t,const void*)>(image+0x53ccb0)(inverse,price,pose->position.data());reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x540760)(r.graph,inverse);
                    reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(image+0x9ff370)(transform,pose->rotation.data(),pose->position.data(),pose->scale.data());
                    r.object=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*)>(image+0x144670)(source,transform);
                    if(!r.object)code=ERROR_INVALID_HANDLE;
                    else {inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xc0);if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
                        else {reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x53dc30)(inverse,r.object);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x540760)(r.graph,inverse);
                            reinterpret_cast<void(*)(void*,const char*)>(image+0xa16f10)(label,"Duplicate Dungeon Gizmo");label_ready=true;
                            reinterpret_cast<void(*)(uintptr_t,const void*,uintptr_t)>(image+0x53b260)(undo,label,r.graph);r.committed=true;}}
                }
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(label_ready){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}
    }
    if(name_ready){__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(name);}__except(EXCEPTION_EXECUTE_HANDLER){r.begin_cleanup=GetExceptionCode();}}
    if(r.edit_open){__try{reinterpret_cast<void(*)()>(image+0x265650)();}__except(EXCEPTION_EXECUTE_HANDLER){r.end_cleanup=GetExceptionCode();}}return code;
}
uint32_t spawn_monster_native(uintptr_t image,uintptr_t manager,uintptr_t shard,uintptr_t undo,uintptr_t sound,const Pose* pose,int32_t rank,int32_t character,int32_t level,Placement& r) {
    alignas(16) unsigned char transform[0x70]{},label[0x38]{};bool label_ready=false;uintptr_t inverse=0;uint32_t code=0;
    __try {
        reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(image+0x9ff370)(transform,pose->rotation.data(),pose->position.data(),pose->scale.data());
        r.object=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*,int32_t,uintptr_t,int32_t)>(image+0x293520)(manager,transform,rank,0,character);
        if(!r.object)code=ERROR_INVALID_HANDLE;
        else {
            r.graph=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x110);
            if(!r.graph)code=ERROR_NOT_ENOUGH_MEMORY;
            else {
                reinterpret_cast<void(*)(uintptr_t)>(image+0x5400f0)(r.graph);inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xc0);
                if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
                else {
                    reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x53dc30)(inverse,r.object);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x540760)(r.graph,inverse);
                    reinterpret_cast<void(*)(void*,const char*)>(image+0xa16f10)(label,"Place Dungeon Monster");label_ready=true;
                    reinterpret_cast<void(*)(uintptr_t,const void*,uintptr_t)>(image+0x53b260)(undo,label,r.graph);r.committed=true;
                    if(rank==2){const auto definition=reinterpret_cast<uintptr_t(*)(uintptr_t)>(image+0x39cdf0)(r.object);
                        if(definition && reinterpret_cast<int32_t(*)(uintptr_t)>(image+0x938c0)(definition)>1)reinterpret_cast<void(*)(int32_t)>(image+0x458e10)(15);}
                    reinterpret_cast<void(*)(uintptr_t,int32_t,uintptr_t,const void*)>(image+0x50eb70)(sound,0x2f,shard,pose->position.data());
                    const auto prior=*reinterpret_cast<const int32_t*>(r.object+0x6f8);
                    if(_InterlockedCompareExchange(reinterpret_cast<volatile long*>(r.object+0x6f8),long(level),long(prior))!=prior)code=ERROR_INVALID_STATE;
                    else reinterpret_cast<void(*)(uintptr_t)>(image+0x3c06e0)(r.object);
                }
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(label_ready){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}
    }return code;
}
struct Point {int64_t x,z;};
struct VectorView {uintptr_t begin,end,capacity;};
struct EraseReceipt {uintptr_t graph=0;float area=0;bool shape=false,committed=false;uint32_t stage=0,shape_cleanup=0,args_cleanup=0,label_cleanup=0;};
uint32_t erase_native(uintptr_t image,uintptr_t dungeon,uintptr_t undo,const VectorView* paths,EraseReceipt& r) {
    alignas(16) unsigned char shape[0x70]{},label[0x38]{};uintptr_t inverse=0;bool label_ready=false;uint32_t code=0;
    __try {
        r.stage=1;reinterpret_cast<void(*)(void*)>(image+0xf1810)(shape);r.shape=true;
        r.stage=2;reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0xf7ed0)(*reinterpret_cast<const uintptr_t*>(shape+0x18),paths);
        r.stage=3;reinterpret_cast<void(*)(void*)>(image+0xf3b60)(shape);
        r.graph=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x110);
        if(!r.graph)code=ERROR_NOT_ENOUGH_MEMORY;
        else {
            r.stage=4;reinterpret_cast<void(*)(uintptr_t)>(image+0x5400f0)(r.graph);inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xd8);
            if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
            else {
                r.stage=5;reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x53e6e0)(inverse,dungeon);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x540760)(r.graph,inverse);
                r.stage=6;r.area=reinterpret_cast<float(*)(uintptr_t,const void*,uintptr_t)>(image+0x157720)(dungeon+0x500,shape,0);
                if(!std::isfinite(r.area) || r.area<=0)code=ERROR_INVALID_STATE;
                else {
                    r.stage=7;reinterpret_cast<void(*)(void*,const char*)>(image+0xa16f10)(label,"Edit dungeon paint");label_ready=true;
                    reinterpret_cast<void(*)(uintptr_t,const void*,uintptr_t)>(image+0x53b260)(undo,label,r.graph);r.committed=true;r.stage=8;
                }
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(label_ready){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}
    }
    if(r.shape){__try{reinterpret_cast<void(*)(void*)>(image+0xf15a0)(shape);}__except(EXCEPTION_EXECUTE_HANDLER){r.shape_cleanup=GetExceptionCode();}}return code;
}
#include "fill.inc"
struct PaintReceipt {uintptr_t graph=0,stroke=0,pending=0;float painted=0,erased=0;uint32_t stage=0,shape_cleanup=0,accumulated_cleanup=0,args_cleanup=0,label_cleanup=0;bool shape=false,accumulated=false,committed=false;};
uint32_t paint_native(uintptr_t image,uintptr_t dungeon,uintptr_t undo,uintptr_t fill,const VectorView* paths,PaintReceipt& r) {
    alignas(16) unsigned char shape[0x70]{},accumulated[0x70]{},label[0x38]{};uintptr_t inverse=0;bool label_ready=false;uint32_t code=0;
    __try {
        r.stage=1;reinterpret_cast<void(*)(void*)>(image+0xf1810)(shape);r.shape=true;
        reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0xf7ed0)(*reinterpret_cast<const uintptr_t*>(shape+0x18),paths);reinterpret_cast<void(*)(void*)>(image+0xf3b60)(shape);
        r.stage=2;reinterpret_cast<void(*)(void*)>(image+0xf1810)(accumulated);r.accumulated=true;
        reinterpret_cast<void(*)(void*,const void*)>(image+0xf8c30)(accumulated,shape);
        r.graph=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x110);
        if(!r.graph)code=ERROR_NOT_ENOUGH_MEMORY;
        else {
            r.stage=3;reinterpret_cast<void(*)(uintptr_t)>(image+0x5400f0)(r.graph);inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xd8);
            if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
            else {
                reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x53e6e0)(inverse,dungeon);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x540760)(r.graph,inverse);
                r.stage=4;r.stroke=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*,uintptr_t)>(image+0x157680)(dungeon+0x500,shape,fill);
                if(r.stroke)r.painted=reinterpret_cast<float(*)(uintptr_t,const void*)>(image+0x15d610)(r.stroke,shape);
                else {
                    const auto area=reinterpret_cast<float(*)(const void*)>(image+0xf1230)(accumulated);
                    if(area>50.0f){
                        r.stage=5;r.pending=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x140);
                        if(!r.pending)code=ERROR_NOT_ENOUGH_MEMORY;
                        else {reinterpret_cast<void(*)(uintptr_t)>(image+0x15d440)(r.pending);
                            reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0x15d140)(r.pending,shape);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x15d250)(r.pending,fill);
                            reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x1563c0)(dungeon+0x500,r.pending);r.stroke=r.pending;r.pending=0;
                            reinterpret_cast<void(*)(uintptr_t)>(image+0x15d360)(r.stroke);r.painted=area;}
                    }
                }
                if(!code && r.stroke){r.stage=6;reinterpret_cast<void(*)(uintptr_t)>(image+0x15d1c0)(r.stroke);
                    r.erased=reinterpret_cast<float(*)(uintptr_t,uintptr_t,uintptr_t)>(image+0x157720)(dungeon+0x500,r.stroke+0x18,r.stroke);
                    reinterpret_cast<void(*)(uintptr_t)>(image+0x15d3d0)(r.stroke);r.stroke=0;}
                if(!code && std::isfinite(r.painted) && std::isfinite(r.erased) && (r.painted>0 || r.erased>0)){
                    r.stage=7;reinterpret_cast<void(*)(void*,const char*)>(image+0xa16f10)(label,"Edit dungeon paint");label_ready=true;
                    reinterpret_cast<void(*)(uintptr_t,const void*,uintptr_t)>(image+0x53b260)(undo,label,r.graph);r.committed=true;r.stage=8;
                }else if(!code)code=ERROR_INVALID_STATE;
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(label_ready){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}
    }
    if(r.shape){__try{reinterpret_cast<void(*)(void*)>(image+0xf15a0)(shape);}__except(EXCEPTION_EXECUTE_HANDLER){r.shape_cleanup=GetExceptionCode();}}
    if(r.accumulated){__try{reinterpret_cast<void(*)(void*)>(image+0xf15a0)(accumulated);}__except(EXCEPTION_EXECUTE_HANDLER){r.accumulated_cleanup=GetExceptionCode();}}return code;
}
#include "building.inc"
#include "gizmo_state.inc"
}

std::optional<Json> query(Context& c,const std::string& operation,const Json& args) {
    if(operation=="dungeons.list"){
        c.require_world();const auto map=c.singleton(0x13df8c0);need(map && c.read<uintptr_t>(map)==c.image+0x1742860,"Dungeon current WorldMap class differs");Json rows=Json::array();
        for(auto region:array(c,map+0x230)){
            const auto manager=c.read<uintptr_t>(region+0x950);need(c.read<uintptr_t>(region)==c.image+0x1787930 && c.read<uintptr_t>(region+0x70)==map+0x1c0 && manager && c.read<uintptr_t>(manager)==c.image+0x17183c0 && c.read<uintptr_t>(manager+0x450)==map+0x1c0 && c.read<uintptr_t>(manager+0x458)==region,"Dungeon root region/Building manager owner differs");
            for(auto exterior:array(c,manager+0x408))if(c.read<uintptr_t>(exterior)==c.image+0x16d93c0){const auto h=handle(c,exterior);const auto o=owner(c,Json{{"handle",h}});rows.push_back({{"handle",h},{"dungeon_uid",std::to_string(c.read<uint64_t>(o.dungeon+0x18))}});}
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
        need(manager && c.read<uintptr_t>(manager)==c.image+0x17183c0 && c.read<uintptr_t>(manager+0x450)==shard && c.read<uintptr_t>(manager+0x458)==region,"Dungeon Building manager owner differs");
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
            for(uintptr_t rva:{0xf1810,0xf7ed0,0xf3b60,0xa02670,0x5400f0,0x53e6e0,0x540760,0x157720,0xa16f10,0x53b260,0x11b2b00,0x11bd1e0,0xf15a0})c.verify_original(rva);
            need(c.read<uintptr_t>(o.dungeon+0x500)==c.image+0x1786c50,"Dungeon embedded Canvas class differs");need(!c.read<uintptr_t>(c.image+0x1a3aa80),"Dungeon paint requires idle native Undo graph");const auto undo=c.singleton(0x13df380);const auto before=undo_stack(c,undo);
            if(operation=="dungeons.paint"){
                fill_records(args.at("fill_records"));verify_fill_code(c);for(uintptr_t rva:{0xf8c30,0x157680,0x15d610,0xf1230,0x15d440,0x15d140,0x15d250,0x1563c0,0x15d360,0x15d1c0,0x15d3d0})c.verify_original(rva);
                attempted=true;auto fill=create_fill(c,args.at("fill_records"));PaintReceipt receipt;const auto code=paint_native(c.image,o.dungeon,undo,fill.pointer,&borrowed,receipt);const auto cleanup=fill_dispose_native(c.image,fill.pointer);
                need(!code && !cleanup && receipt.committed && !receipt.pending && !receipt.stroke && !receipt.shape_cleanup && !receipt.accumulated_cleanup && !receipt.args_cleanup && !receipt.label_cleanup,"Dungeon paint failed SEH="+std::to_string(code)+" stage="+std::to_string(receipt.stage)+" painted="+std::to_string(receipt.painted)+" erased="+std::to_string(receipt.erased)+" required_changed_area>0 fill_cleanup="+std::to_string(cleanup)+" shape_cleanup="+std::to_string(receipt.shape_cleanup)+" accumulated_cleanup="+std::to_string(receipt.accumulated_cleanup)+" args_cleanup="+std::to_string(receipt.args_cleanup)+" label_cleanup="+std::to_string(receipt.label_cleanup)+" pending_stroke="+std::to_string(bool(receipt.pending || receipt.stroke))+"; do not replay");
                need(owner(c,args).dungeon==o.dungeon,"Dungeon Canvas owner changed during paint");committed_graph(c,undo,before,receipt.graph);result={{"painted_area",receipt.painted},{"erased_area",receipt.erased}};return true;
            }
            EraseReceipt receipt;attempted=true;const auto code=erase_native(c.image,o.dungeon,undo,&borrowed,receipt);
            need(!code && receipt.committed && !receipt.shape_cleanup && !receipt.args_cleanup && !receipt.label_cleanup,"Dungeon paint erasure failed SEH="+std::to_string(code)+" stage="+std::to_string(receipt.stage)+" erased="+std::to_string(receipt.area)+" required_area>0 shape_cleanup="+std::to_string(receipt.shape_cleanup)+" args_cleanup="+std::to_string(receipt.args_cleanup)+" label_cleanup="+std::to_string(receipt.label_cleanup)+"; do not replay");
            need(owner(c,args).dungeon==o.dungeon,"Dungeon Canvas owner changed during erasure");committed_graph(c,undo,before,receipt.graph);result={{"erased_area",receipt.area}};
        } else if(operation=="dungeons.spawn_monster") {
            const auto character=object(c,args.at("character"),0x1705990);need(c.read<int32_t>(character+0x2c)==1 && !c.read<uint8_t>(character+0x490),"Dungeon Monster requires a live Monster Character definition");const auto type=c.read<int32_t>(character+0x30);
            const auto definitions=c.singleton(0x13df7a0);need(definitions && c.read<uintptr_t>(definitions)==c.image+0x1732430,"Dungeon Monster Character manager class differs");const auto types=array(c,definitions+0xd0);need(type>=0 && size_t(type)<types.size() && types[size_t(type)]==character,"Dungeon Monster Character full UID/global type slot differs");
            const int32_t rank=integer(args.at("rank"),0,2,"Monster rank"),level=integer(args.at("level"),1,INT32_MAX,"Monster level");Pose pose;pose.position=numbers<3>(args.at("position"),"Monster position");if(args.contains("rotation"))pose.rotation=numbers<4>(args.at("rotation"),"Monster rotation");float norm=0;for(float x:pose.rotation)norm+=x*x;need(std::abs(norm-1)<0.002f,"Dungeon Monster rotation must be a normalized quaternion");
            for(uintptr_t rva:{0x293520,0x9ff370,0xa02670,0x5400f0,0x53dc30,0x540760,0xa16f10,0x53b260,0x39cdf0,0x938c0,0x458e10,0x50eb70,0x3c06e0,0x11b2b00,0x11bd1e0,0x5089b0})c.verify_original(rva);
            need(c.read<uintptr_t>(c.image+0x16c0770+0x470)==c.image+0x3c06e0,"Dungeon Monster LevelChanged virtual differs");need(!c.read<uintptr_t>(c.image+0x1a3aa80),"Dungeon Monster placement requires idle native Undo");uintptr_t shard=0;const auto region=scene_region(c,o,args,shard),manager=c.read<uintptr_t>(region+0x988),undo=c.singleton(0x13df380),sound=c.singleton(0x13df450);need(sound && undo,"Dungeon Monster placement requires Sound/Undo singleton");
            uintptr_t destination=0;seh(region_native(c.image,shard,&pose,destination),"Monster destination RegionAt");need(destination==region,"Dungeon Monster position leaves the selected scene Region");
            const auto before=monsters(c,o,args),undo_before=undo_stack(c,undo);Placement receipt;attempted=true;const auto code=spawn_monster_native(c.image,manager,shard,undo,sound,&pose,rank,type,level,receipt);
            need(!code && receipt.committed && !receipt.args_cleanup && !receipt.label_cleanup,"Dungeon Monster placement failed SEH="+std::to_string(code)+" args_cleanup="+std::to_string(receipt.args_cleanup)+" label_cleanup="+std::to_string(receipt.label_cleanup)+"; do not replay");
            committed_graph(c,undo,undo_before,receipt.graph);const auto after=monsters(c,owner(c,args),args);need(after.size()==before.size()+1 && after.back()==receipt.object && std::equal(before.begin(),before.end(),after.begin()),"Dungeon Monster placement surviving roster differs");result=monster_json(c,receipt.object);
            need(result.at("rank")==rank && result.at("character_type")==type && result.at("character_kind")== (rank==2?4:1) && result.at("level")==level && result.at("tag")==0 && result.at("status")==3 && result.at("position")==Json(pose.position) && result.at("rotation")==Json(pose.rotation) && result.at("scale")==Json(pose.scale),"Dungeon Monster placement authored readback differs");
        } else if(operation=="dungeons.set_gizmo_contents") {
            const auto target=object(c,args.at("gizmo"),0x1749ab0);const auto members=gizmos(c,o,args);need(std::find(members.begin(),members.end(),target)!=members.end(),"Dungeon Gizmo does not belong to the selected scene");
            pin(c,{0x145560,38,"cd1def127ff461cfccd56ad5c12b07e201cd625757245b02f98c41e472a0bb1e"});bool container=false;seh(container_feature(c.image,target,container),"Gizmo container feature");need(container,"Dungeon Gizmo content edits require the native container feature");
            const auto& contents=args.at("contents");need(contents.is_object() && !contents.empty(),"Dungeon contents must contain at least one field");
            for(const auto& [name,value]:contents.items()){need(name=="gold" || name=="loot" || name=="vendor_trash" || name=="keys" || name=="boss_keys","Dungeon content field is unsupported: "+name);integer(value,0,5,"container content");}
            auto expected=gizmo_json(c,target);
            for(const auto& [name,offset]:std::array<std::pair<const char*,size_t>,5>{{{"gold",0x260},{"loot",0x264},{"vendor_trash",0x268},{"keys",0x26c},{"boss_keys",0x270}}}){
                if(!contents.contains(name) || contents.at(name)==expected.at(name))continue;need(gizmo_json(c,object(c,args.at("gizmo"),0x1749ab0))==expected,"Dungeon container changed before content CAS");
                bool exchanged=false;attempted=true;seh(runtime_scalar(target+offset,false,expected.at(name).get<int32_t>(),integer(contents.at(name),0,5,"container content"),exchanged),"Gizmo content CAS");need(exchanged,"Dungeon content CAS prior value differs; do not replay");expected[name]=contents.at(name);
            }
            result=gizmo_json(c,object(c,args.at("gizmo"),0x1749ab0));need(result==expected,"Dungeon container content readback differs");
        } else if(operation=="dungeons.move_gizmo" || operation=="dungeons.destroy_gizmo" || operation=="dungeons.duplicate_gizmo") {
            const auto target=object(c,args.at("gizmo"),0x1749ab0);const auto members=gizmos(c,o,args);need(std::find(members.begin(),members.end(),target)!=members.end(),"Dungeon Gizmo does not belong to the selected scene");
            need(!args.contains("instance") && instances(c,o).empty(),"Dungeon Gizmo control requires authored template with zero Instances");
            c.verify_original(0xa29a0);c.verify_original(0x146af0);c.verify_original(0x54d00);c.verify_original(0x5089b0);
            const auto before=gizmo_json(c,target);Pose pose;pose.position=before.at("position").get<std::array<float,3>>();pose.rotation=before.at("rotation").get<std::array<float,4>>();pose.scale=before.at("scale").get<std::array<float,3>>();
            if(operation=="dungeons.move_gizmo" || operation=="dungeons.duplicate_gizmo"){
                pose.position=numbers<3>(args.at("position"),"position");if(args.contains("rotation"))pose.rotation=numbers<4>(args.at("rotation"),"rotation");if(args.contains("scale"))pose.scale=numbers<3>(args.at("scale"),"scale");
                for(float x:pose.position)need(std::isfinite(x),"Dungeon Gizmo move position is non-finite");for(float x:pose.scale)need(std::isfinite(x) && x>0,"Dungeon Gizmo move scale must be positive and finite");float norm=0;for(float x:pose.rotation){need(std::isfinite(x),"Dungeon Gizmo move quaternion is non-finite");norm+=x*x;}need(std::abs(norm-1)<0.002f,"Dungeon Gizmo move quaternion is not normalized");
                need(c.read<uintptr_t>(c.image+0x1749ab0+0xd0)==c.image+0xe470c0 && c.read<uintptr_t>(c.image+0x1749ab0+0x160)==c.image+0x146870,"Dungeon Gizmo movement virtual methods differ");
            }
            const auto type=before.at("type").get<std::string>();const Text key{type.data(),uint64_t(type.size())};int32_t ignored_cost=0;bool offline=false,ignored_affordable=false;uintptr_t destination=0;
            seh(placement_probe(c.image,o.dungeon,c.singleton(0x13df220),o.shard,&key,&pose,ignored_cost,offline,ignored_affordable,destination),"Gizmo control preflight");need(offline && destination==o.region,"Dungeon Gizmo control requires offline native owner and same Region");
            need(!c.read<uintptr_t>(c.image+0x1a3aa80),"Dungeon Gizmo control requires idle Undo graph");const auto undo=c.singleton(0x13df380),selection_manager=c.singleton(0x13df630);const auto undo_before=undo_stack(c,undo);const auto prior=selection(c,selection_manager);std::vector<Json> prior_handles;for(auto p:prior)prior_handles.push_back(handle(c,p));
            c.verify_original(0x2d5910);uintptr_t expected_graph=0,created=0;
            if(operation=="dungeons.destroy_gizmo"){
                pin(c,{0x2d42c0,2768,"c526304f6ce4e9014154daf39a936bae914f448e7788f80d816b52615fb68e8f"});const NativeArray row{c.image+0x1748c50,&target,1,1};attempted=true;seh(selection_native(c.image,selection_manager,&row,true),"DestroyProps");
            }else if(operation=="dungeons.duplicate_gizmo"){
                need(c.read<uintptr_t>(c.image+0x1749ab0+0xd8)==c.image+0xe46ff0 && c.read<uintptr_t>(c.image+0x1749ab0+0xe8)==c.image+0x144670 && c.read<uintptr_t>(c.image+0x1749ab0+0x278)==c.image+0xe47040,"Dungeon Gizmo duplication virtual methods differ");
                for(uintptr_t rva:{0xe46ff0,0x144670,0xe47040,0x54d80,0x264c30,0x5c04b0,0x265300,0xa02670,0x5400f0,0x53ccb0,0x540760,0x9ff370,0x53dc30,0xa16f10,0x53b260,0x11b2b00,0x11bd1e0,0x265650})c.verify_original(rva);
                int32_t price=-1;bool allowed=false;seh(duplicate_price(target,price,allowed),"Gizmo duplicate capability/price");need(allowed && price>=0,"Dungeon Gizmo native CanDuplicate/price rejected target");
                bool affordable=false;int32_t actual_type_cost=0;uintptr_t same_region=0;bool same_offline=false;seh(placement_probe(c.image,o.dungeon,c.singleton(0x13df220),o.shard,&key,&pose,actual_type_cost,same_offline,affordable,same_region),"duplicate affordability");need(affordable && actual_type_cost==price,"Dungeon Gizmo duplicate native price/affordability differs");
                Placement receipt;attempted=true;const auto code=duplicate_native(c.image,c.singleton(0x13df220),undo,target,&pose,price,receipt);need(!code && receipt.committed && !receipt.begin_cleanup && !receipt.end_cleanup && !receipt.args_cleanup && !receipt.label_cleanup,"Dungeon Gizmo Duplicate failed SEH="+std::to_string(code)+" begin_cleanup="+std::to_string(receipt.begin_cleanup)+" end_cleanup="+std::to_string(receipt.end_cleanup)+" args_cleanup="+std::to_string(receipt.args_cleanup)+" label_cleanup="+std::to_string(receipt.label_cleanup)+"; do not replay");expected_graph=receipt.graph;created=receipt.object;
            }else{
                for(uintptr_t rva:{0x9ff370,0xa16f10,0x265300,0x539990,0x55e8e0,0x55ed80,0xcf1600,0x5379c0,0x265650,0x11b2b00,0x11bd1e0,0xe470c0,0x146870})c.verify_original(rva);
                MoveReceipt receipt;attempted=true;const auto code=move_native(c.image,target,&pose,receipt);need(!code && receipt.moved && !receipt.move_cleanup && !receipt.close_cleanup && !receipt.end_cleanup && !receipt.args_cleanup && !receipt.label_cleanup,"Dungeon Gizmo Move failed SEH="+std::to_string(code)+" move_cleanup="+std::to_string(receipt.move_cleanup)+" close_cleanup="+std::to_string(receipt.close_cleanup)+" end_cleanup="+std::to_string(receipt.end_cleanup)+" args_cleanup="+std::to_string(receipt.args_cleanup)+" label_cleanup="+std::to_string(receipt.label_cleanup)+"; do not replay");expected_graph=receipt.graph;
            }
            std::vector<uintptr_t> restored;for(size_t i=0;i<prior.size();++i){if(operation=="dungeons.destroy_gizmo" && prior[i]==target)continue;need(c.resolve(prior_handles[i])==prior[i],"Dungeon control changed another selected object's UID lifetime");restored.push_back(prior[i]);}
            const NativeArray restore{c.image+0x1748c50,restored.data(),int32_t(restored.size()),int32_t(restored.size())};seh(selection_native(c.image,selection_manager,&restore,false),"restore Selection");need(selection(c,selection_manager)==restored,"Dungeon control Selection readback differs");committed_graph(c,undo,undo_before,expected_graph);
            const auto after=gizmos(c,owner(c,args),args);if(operation=="dungeons.destroy_gizmo"){auto expected=members;std::erase(expected,target);need(after==expected,"Dungeon DestroyProps surviving scene roster differs");result={{"destroyed",true}};}
            else if(operation=="dungeons.duplicate_gizmo"){need(created && after.size()==members.size()+1 && after.back()==created && std::equal(members.begin(),members.end(),after.begin()),"Dungeon Duplicate changed surviving scene roster");result=gizmo_json(c,created);need(result.at("type")==before.at("type") && result.at("variant")==before.at("variant") && result.at("position")==Json(pose.position) && result.at("rotation")==Json(pose.rotation) && result.at("scale")==Json(pose.scale),"Dungeon Gizmo Duplicate authored readback differs");}
            else{need(after==members,"Dungeon Move changed scene roster");auto expected=before;expected["position"]=pose.position;expected["rotation"]=pose.rotation;expected["scale"]=pose.scale;result=gizmo_json(c,object(c,args.at("gizmo"),0x1749ab0));need(result==expected,"Dungeon Gizmo Move authored readback differs");}
        } else if(operation=="dungeons.place_gizmo") {
            need(!args.contains("instance"),"Dungeon Gizmo placement targets the authored template");
            const auto type=args.at("type").get<std::string>(),variant=args.value("variant",std::string{}),tool_name=args.at("tool").get<std::string>();
            need(!type.empty() && type.size()<=256 && variant.size()<=256 && type.find('\0')==std::string::npos && variant.find('\0')==std::string::npos,"Dungeon Gizmo resource key is invalid");
            const int tool=tool_name=="door"?0:tool_name=="key"?1:tool_name=="treasure"?2:-1;need(tool>=0,"Dungeon Gizmo tool must be door, key or treasure");
            Pose pose;pose.position=numbers<3>(args.at("position"),"position");if(args.contains("rotation"))pose.rotation=numbers<4>(args.at("rotation"),"rotation");if(args.contains("scale"))pose.scale=numbers<3>(args.at("scale"),"scale");
            for(float x:pose.position)need(std::isfinite(x),"Dungeon placement position is non-finite");for(float x:pose.scale)need(std::isfinite(x) && x>0,"Dungeon placement scale must be finite and positive");
            float norm=0;for(float x:pose.rotation){need(std::isfinite(x),"Dungeon placement rotation is non-finite");norm+=x*x;}need(std::abs(norm-1)<0.002f,"Dungeon placement rotation must be a normalized quaternion");
            for(uintptr_t rva:{0xa29a0,0x146af0,0x54d00,0x5089b0,0x9ff370,0x147a20,0x50e810,0x264c30,0x5c04b0,0x265300,0x153920,0xa4f10,0x145950,0xa02670,0x5400f0,0x53ccb0,0x540760,0x53dc30,0xa16f10,0x53b260,0x11b2b00,0x11bd1e0,0x265650})c.verify_original(rva);
            need(c.read<uintptr_t>(c.image+0x1a3aa80)==0,"Dungeon Gizmo placement requires idle native Undo");const auto before=gizmos(c,o,args);const auto manager=c.read<uintptr_t>(o.region+0x9a8),business=c.singleton(0x13df220),undo=c.singleton(0x13df380),sound=c.singleton(0x13df450);
            need(business && undo && (tool!=2 || sound),"Dungeon placement Business/Undo/Sound singleton is absent");const Text key{type.data(),uint64_t(type.size())},variant_key{variant.data(),uint64_t(variant.size())};
            int32_t cost=-1;bool offline=false,affordable=false;uintptr_t destination=0;seh(placement_probe(c.image,o.dungeon,business,o.shard,&key,&pose,cost,offline,affordable,destination),"placement preflight");
            need(offline && instances(c,o).empty(),"Dungeon Gizmo placement requires original offline Dungeon with zero Instances");need(cost>=0 && affordable,"Dungeon native Gizmo resource is unaffordable or has invalid price");need(destination==o.region,"Dungeon placement position leaves its template Region");
            const auto undo_before=undo_stack(c,undo);Placement receipt;attempted=true;const auto code=place_gizmo_native(c.image,o.dungeon,manager,undo,sound,&key,&variant_key,&pose,tool,cost,receipt);
            need(!code && !receipt.begin_cleanup && !receipt.end_cleanup && !receipt.args_cleanup && !receipt.label_cleanup && receipt.committed,"Dungeon Gizmo placement transaction failed SEH="+std::to_string(code)+" begin_cleanup="+std::to_string(receipt.begin_cleanup)+" end_cleanup="+std::to_string(receipt.end_cleanup)+" args_cleanup="+std::to_string(receipt.args_cleanup)+" label_cleanup="+std::to_string(receipt.label_cleanup)+"; do not replay");
            committed_graph(c,undo,undo_before,receipt.graph);const auto after=gizmos(c,owner(c,args),args);need(after.size()==before.size()+1 && after.back()==receipt.object && std::equal(before.begin(),before.end(),after.begin()),"Dungeon Gizmo placement roster readback differs");
            result=gizmo_json(c,receipt.object);need(result.at("type")==type && result.at("variant")==variant && result.at("position")==Json(pose.position) && result.at("rotation")==Json(pose.rotation) && result.at("scale")==Json(pose.scale),"Dungeon Gizmo placement authored readback differs");result["cost"]=cost;
        } else if(operation=="dungeons.rename_gizmo") {
            const auto target=object(c,args.at("gizmo"),0x1749ab0);const auto members=gizmos(c,o,args);need(std::find(members.begin(),members.end(),target)!=members.end(),"Dungeon Gizmo does not belong to the selected scene");
            const auto name=args.at("name").get<std::string>();need(!name.empty() && name.size()<=256 && name.find('\0')==std::string::npos,"Dungeon Gizmo name must contain 1..256 bytes without NUL");
            pin(c,{0x4cf710,16,"a09973ac6e34d4b78efa50878d5da6dfd7004a87abee0fec1014e20f7119aeaf"});const Text text{name.data(),uint64_t(name.size())};
            attempted=true;seh(rename_native(c.image,target,&text),"Gizmo Rename");result=gizmo_json(c,object(c,args.at("gizmo"),0x1749ab0));need(result.at("name")==name,"Dungeon Gizmo name readback differs");
        } else if(operation=="dungeons.set_gizmo_runtime") {
            const auto target=object(c,args.at("gizmo"),0x1749ab0);const auto members=gizmos(c,o,args);need(std::find(members.begin(),members.end(),target)!=members.end(),"Dungeon Gizmo does not belong to the selected scene");
            const auto& state=args.at("state");need(state.is_object() && !state.empty(),"Dungeon Gizmo runtime state must contain at least one field");
            for(const auto& [name,value]:state.items()){
                need(name=="triggered" || name=="state" || name=="immediate" || name=="on_map","Dungeon Gizmo runtime field is unsupported: "+name);
                if(name=="triggered" || name=="on_map")need(value.is_boolean(),"Dungeon Gizmo runtime boolean required: "+name);
                else need(value.is_number_integer() && value.get<int64_t>()>=0 && value.get<int64_t>()<=2,"Dungeon Gizmo Door state must be 0..2");
            }
            const auto before=gizmo_json(c,target);auto expected=before;
            if(state.contains("on_map") && state.at("on_map")!=before.at("on_map")){
                pin(c,{0x147f90,448,"db7cb6b989c4aa75a8c82a33e228f95e95410e5b241c07730ccb068d5fab77dd"});
                attempted=true;seh(on_map_native(c.image,target,state.at("on_map").get<bool>()),"Gizmo SetOnMap");expected["on_map"]=state.at("on_map");need(gizmo_json(c,object(c,args.at("gizmo"),0x1749ab0))==expected,"Dungeon Gizmo SetOnMap changed unexpected authored fields");
            }
            for(const auto& [name,offset]:std::array<std::pair<const char*,size_t>,3>{{{"triggered",0x250},{"state",0x254},{"immediate",0x328}}}){
                if(!state.contains(name) || state.at(name)==expected.at(name))continue;
                need(gizmo_json(c,object(c,args.at("gizmo"),0x1749ab0))==expected,"Dungeon Gizmo changed before runtime CAS");
                const bool byte=offset==0x250;const int32_t prior=byte?int32_t(expected.at(name).get<bool>()):expected.at(name).get<int32_t>();const int32_t desired=byte?int32_t(state.at(name).get<bool>()):state.at(name).get<int32_t>();bool exchanged=false;
                attempted=true;seh(runtime_scalar(target+offset,byte,prior,desired,exchanged),"Gizmo runtime CAS");need(exchanged,"Dungeon Gizmo runtime CAS prior value differs; do not replay");expected[name]=state.at(name);
            }
            result=gizmo_json(c,object(c,args.at("gizmo"),0x1749ab0));need(result==expected,"Dungeon Gizmo runtime final readback differs");
        } else if(operation=="dungeons.set_palette") {
            pin(c,{0x967370,407,"6baa3c9cdf746116d6c6788539dfb0a35cd8a778e1c8131549eda3e4b04ed81d"});
            pin(c,{0x9676a0,148,"7f6e66c76e2be9e2d7f43ab04d8721cc060f16ddcd111a422605af135ab1bb4a"});
            pin(c,{0x972960,93,"a1db2cb402fa1f6edd2483e5da5f56ca3adb201353164a3e699b542a45e294af"});
            pin(c,{0x8a1c70,114,"9ac95f247b570d7823388d82c701fafc540e83bc2615b87c96a13a7b72d954d6"});
            pin(c,{0xe3ff80,236,"0ca63c27bb113b022fbfb88fa12bf596c3cf8856d35ad12e7811dbe8630bd88f"});
            const auto colors=color_input(args.at("colors"));const bool defaults=args.value("defaults",false);
            std::array<uint32_t,4> packed{};for(size_t i=0;i<4;++i){for(float x:colors[i])need(std::isfinite(x) && x>=0 && x<=1,"Dungeon palette channels must be finite 0..1");seh(pack_color(c.image,&packed[i],&colors[i]),"ColorPacked conversion");}
            const auto before=layout(c,o);need(c.read<uintptr_t>(o.dungeon+0x1a0)==c.image+0x172bf80,"Dungeon ColorConfig class differs");const auto data=c.read<uintptr_t>(o.dungeon+0x1c0);
            CommandReceipt receipt;attempted=true;seh(palette_native(c.image,o.dungeon,data,defaults,&colors,receipt),"palette leaves");seh(receipt.cleanup,"palette record cleanup");need(receipt.returned && receipt.accepted,"Dungeon palette native boolean loader rejected flag");
            const auto after=layout(c,owner(c,args));need(c.read<uint8_t>(o.dungeon+0x1d0)==uint8_t(defaults) && std::equal(packed.begin(),packed.end(),after.palette.begin()),"Dungeon palette/default flag readback differs");
            need(before.cells.size()==after.cells.size(),"Dungeon palette changed cell count");for(size_t i=0;i<before.cells.size();++i)need(before.cells[i].pointer==after.cells[i].pointer && before.cells[i].open==after.cells[i].open && before.cells[i].theme==after.cells[i].theme && before.cells[i].colors==after.cells[i].colors,"Dungeon palette changed existing cells");result={{"palette",after.palette},{"defaults",defaults}};
        } else if(operation=="dungeons.set_lighting") {
            for(const auto& p:light_pins)pin(c,p);
            LightInput input{integers<4>(args.at("spinners"),"light Spinners"),integers<3>(args.at("sliders"),"light Sliders"),color_input(args.at("colors"))};
            for(const auto& color:input.colors)for(float x:color)need(std::isfinite(x) && x>=0 && x<=1,"Dungeon light color channels must be finite 0..1");
            const auto w=window(c,"dungeon_light",0x172c100);const auto widgets=light_widgets(c,w);std::array<float,23> expected{};
            seh(light_numbers(c.image,&widgets,&input,&expected),"lighting numeric conversion");for(auto x:expected)need(std::isfinite(x),"Dungeon native lighting conversion is non-finite");
            attempted=true;seh(light_prepare(c.image,w,o.dungeon,&widgets,&input),"lighting Window preparation");need(c.read<uintptr_t>(w+0x4e8)==o.dungeon,"Dungeon lighting Window owner differs");
            for(size_t i=0;i<4;++i)need(c.read<int32_t>(widgets.spinners[i]+0x44c)==input.spinners[i] && c.read<std::array<float,4>>(widgets.colors[i]+0x2c8)==input.colors[i],"Dungeon lighting native Spinner/color readback differs");
            for(size_t i=0;i<3;++i)need(c.read<int32_t>(widgets.sliders[i]+0x4d0)==input.sliders[i],"Dungeon lighting native Slider readback differs");
            command(c,0x6d4380,w,"MainLight",attempted);need(c.read<std::array<float,23>>(owner(c,args).dungeon+0x1f0)==expected,"Dungeon lighting authored readback differs");result={{"light",expected}};
        } else if(cell) {
            for(const auto& p:cell_pins)pin(c,p);
            c.verify_original(0x1264b60);
            const int mode=operation=="dungeons.close_cell"?0:operation=="dungeons.open_cell"?1:operation=="dungeons.set_cell_theme"?2:3;
            const auto before=layout(c,o);const int32_t index=integer(args.at("cell"),0,INT32_MAX,"cell index");need(size_t(index)<before.cells.size(),"Dungeon cell index is outside layout");
            const auto& prior=before.cells[size_t(index)];std::string theme=mode==1 || mode==2?args.at("theme").get<std::string>():prior.theme;
            need(theme.size()<=256,"Dungeon Theme key exceeds native bound");const Text key{theme.data(),uint64_t(theme.size())};
            if(mode==1 || mode==2){need(!theme.empty(),"Dungeon Theme key is empty");uintptr_t resource=0;seh(theme_native(c.image,c.singleton(0x13df700),&key,resource),"Theme lookup");need(resource && c.read<uintptr_t>(resource)==c.image+0x16ff4d0,"Dungeon Theme resource is absent or wrong type");}
            if((mode==0 && !prior.open) || (mode==1 && prior.open)){result={{"cell",index},{"changed",false}};cause.clear();return true;}
            need(mode<=1 || prior.open,"Dungeon cell must be open before applying Theme or palette");
            if(mode<=1){bool allowed=false;seh(cell_allowed(c.image,o.dungeon,index,mode==1,allowed),"cell eligibility");need(allowed,"Dungeon native cell eligibility/obstruction guard rejected edit");}
            need(c.read<uintptr_t>(c.image+0x1a3aa80)==0,"Dungeon cell edit requires no active native Undo graph");const auto undo=c.singleton(0x13df380);need(undo!=0,"Dungeon Undo singleton is absent");
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
            pin(c,{0x6aea60,348,"d83bc1cc9ba2ec0cb1a974ad81ba102ce240e141ab298b4593493ae648a53ce5"});
            pin(c,{0x6aebc0,24,"e0e8f9900dfde377d73258a11bd39bc13d3df89245a5202c93d2626e247b50cd"});
            pin(c,{0x6adc90,3527,"25113285afd8eb594bf67050a63c2554405edb17fd2f1d98f38af792aeebb3f9"});
            const auto w=window(c,"instance_config_window",0x173bee0);attempted=true;seh(config_prepare(c.image,w,o.dungeon,count),"configure Window");
            need(c.read<uintptr_t>(w+0x510)==o.dungeon && c.read<int32_t>(w+0x508)==count,"Dungeon prepared config Window differs");
            command(c,0x6aea60,w,"Accept",attempted);need(c.read<int32_t>(owner(c,args).dungeon+0x40c)==count,"Dungeon desired Instance count readback differs");result={{"desired_instances",count}};
        } else {
            const int32_t atmosphere=integer(args.at("atmosphere"),0,5,"atmosphere");
            pin(c,{0x6d4ff0,215,"94e4fd8e9934644efaf67f495886e82969ad509bb61c33fbb956f409a8edf4c7"});
            pin(c,{0x6d4cd0,785,"24a50eb36bc1dc9355ae0352258d7d06a8fdedfcc395aab307458c8dbdce33b2"});
            const auto w=window(c,"dungeon_sound",0x172c3c0);attempted=true;seh(sound_prepare(c.image,w,o.dungeon),"sound Window");
            need(c.read<uintptr_t>(w+0x4e8)==o.dungeon,"Dungeon sound Window owner differs");command(c,0x6d4ff0,w,"SetAtmos "+std::to_string(atmosphere),attempted);
            need(c.read<int32_t>(owner(c,args).dungeon+0x830)==atmosphere,"Dungeon atmosphere readback differs");result={{"atmosphere",atmosphere}};
        }
        cause.clear();return true;
    });
}
}
