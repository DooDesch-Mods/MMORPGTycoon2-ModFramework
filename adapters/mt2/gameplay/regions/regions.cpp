#include "regions.h"
#include "internal/mt2mp/game/native_engine.h"
#include "level_census.h"
#include "paint_model.h"
#include <windows.h>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <cstring>
#include <bit>
#include <type_traits>

namespace mt2fw::mt2::gameplay::regions {
namespace {
void need(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
struct Text {const char* data;uint64_t size;uint64_t capacity=0,reserved=0;};
struct Region {uintptr_t object;int32_t id;};
void ap(Context& c,uintptr_t object,uint64_t rva){need(object&&c.read<uintptr_t>(object)==c.image+rva,"Region native object class differs");}
void require_root_view(Context& c){
    auto map=c.singleton(0x13df8c0);ap(c,map,0x1742860);ap(c,map+0x1c0,0x174ad20);auto manager=c.singleton(0x13df440);ap(c,manager,0x1702b00);
    need(c.read<uintptr_t>(manager+0x60)==map+0x1c0,"Native root-world tool requires the root world view");
}
std::vector<Region> index(Context& c){
    c.require_world();auto map=c.singleton(0x13df8c0);ap(c,map,0x1742860);ap(c,map+0x1c0,0x174ad20);
    need(c.read<int32_t>(map+0x208)==0,"Region query requires the root world Shard");
    ap(c,map+0x230,0x16f04e0);auto data=c.read<uintptr_t>(map+0x238);
    auto count=c.read<int32_t>(map+0x240),capacity=c.read<int32_t>(map+0x244);
    need(count>0&&count<=4096&&capacity>=count&&data&&size_t(count)<=(UINTPTR_MAX-data)/8,"Region saved array bounds differ");
    std::vector<Region> rows;std::set<int32_t> ids;std::set<uintptr_t> pointers;
    for(int32_t i=0;i<count;++i){auto object=c.read<uintptr_t>(data+size_t(i)*8);ap(c,object,0x1787930);
        auto id=c.read<int32_t>(object+0x834);need(id>=0&&ids.insert(id).second&&pointers.insert(object).second&&c.read<uintptr_t>(object+0x70)==map+0x1c0,"Region saved identity or ownership differs");
        rows.push_back({object,id});}
    need(c.read<uintptr_t>(map+0x238)==data&&c.read<int32_t>(map+0x240)==count&&c.read<int32_t>(map+0x244)==capacity,"Region roster changed during read");return rows;
}
int64_t integer(const Json& value,const char* name){need(value.is_number_integer(),std::string(name)+" requires an integer");
    if(value.is_number_unsigned())need(value.get<uint64_t>()<=uint64_t(INT64_MAX),std::string(name)+" exceeds signed integer range");return value.get<int64_t>();}
Region select(Context& c,const Json& handle){
    need(handle.is_object()&&integer(handle.at("epoch"),"epoch")>=0&&handle.at("epoch").get<uint64_t>()==c.epoch,"Region handle belongs to another world");
    auto id=integer(handle.at("id"),"region ID");need(id>=0&&id<=INT32_MAX,"Region ID is outside its native range");
    for(auto row:index(c))if(row.id==id)return row;throw std::runtime_error("Region is absent from the saved world roster");
}
template<size_t N>Json vector(Context& c,uintptr_t address){auto value=c.read<std::array<float,N>>(address);Json out=Json::array();
    for(auto number:value){need(std::isfinite(number),"Region native vector contains a nonfinite value");out.push_back(number);}return out;}
Json state(Context& c,const Region& region){
    auto p=region.object;auto owned=c.read<uint8_t>(p+0x850);need(owned<=1,"Region ownership is not a native Boolean");
    auto styles=c.read<uintptr_t>(p+0x860);need(styles&&c.read<int32_t>(p+0x868)==4&&c.read<int32_t>(p+0x86c)>=4,"Region style array differs");
    Json names=Json::array();for(size_t i=0;i<4;++i)names.push_back(c.text(styles+i*32));
    auto particles=c.read<int32_t>(p+0x900);auto enabled=c.read<uint8_t>(p+0x8fc);
    need(particles>=0&&particles<=6&&enabled<=1&&bool(enabled)==(particles!=0),"Region particle configuration differs");
    return {{"handle",{{"epoch",c.epoch},{"id",region.id}}},{"name",c.text(p+0x810)},
        {"type",c.read<int32_t>(p+0x830)},{"owned",owned!=0},{"status",c.read<int32_t>(p+0x854)},
        {"level",c.read<int32_t>(p+0x838)},{"max_level",c.read<int32_t>(p+0x83c)},
        {"position",vector<3>(c,p+0x38)},{"preset",c.text(p+0x8b0)},{"styles",names},
        {"tint",vector<4>(c,p+0x840)},{"sky",vector<16>(c,p+0x870)},{"light",vector<4>(c,p+0x8dc)},
        {"ambient",vector<4>(c,p+0x8ec)},{"particles",particles},{"particle_color",vector<4>(c,p+0x904)}};
}
std::vector<float> input(const Json& value,size_t count){need(value.is_array()&&value.size()==count,"Region vector has the wrong number of channels");std::vector<float> out;
    for(const auto& number:value){need(number.is_number(),"Region vector channels must be numbers");auto d=number.get<double>();
        need(std::isfinite(d)&&std::abs(d)<=std::numeric_limits<float>::max(),"Region vector channel is not a finite native float");out.push_back(float(d));}return out;}
DWORD call_vector(uintptr_t entry,uintptr_t region,const void* value){
    __try{reinterpret_cast<void(*)(void*,const void*)>(entry)(reinterpret_cast<void*>(region),value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD rename(uintptr_t entry,uintptr_t widget,const Text* value){
    __try{reinterpret_cast<void(*)(void*,const Text*)>(entry)(reinterpret_cast<void*>(widget),value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD preset_lookup(uintptr_t image,uintptr_t manager,const Text* value,uintptr_t& result){
    __try{result=uintptr_t(reinterpret_cast<void*(*)(void*,const Text*)>(image+0x2c9d40)(reinterpret_cast<void*>(manager),value));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
DWORD refresh_window(uintptr_t image,uintptr_t window){
    __try{reinterpret_cast<void(*)(void*)>(image+0x797360)(reinterpret_cast<void*>(window));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
void refresh_region(Context& c,uintptr_t region){
    auto selection=c.singleton(0x13df630);ap(c,selection,0x17205d0);auto handler=c.read<uintptr_t>(selection+0xb8);
    if(!handler||c.read<uintptr_t>(handler)!=c.image+0x17317e0)return;auto window=c.read<uintptr_t>(handler+0x10);if(!window)return;
    ap(c,window,0x171fad0);auto visible=c.read<uint8_t>(window+0x45f);need(visible<=1,"Region info window visibility is not a native Boolean");
    if(c.read<uintptr_t>(window+0x4e8)!=region||!visible)return;c.verify_original(0x797360);
    auto code=refresh_window(c.image,window);need(!code,"Native Region info refresh failed SEH="+std::to_string(code));
}
DWORD particle_copy(uintptr_t region,int32_t kind,const float* color){
    __try{if(color)std::memcpy(reinterpret_cast<void*>(region+0x904),color,16);
        else{*reinterpret_cast<int32_t*>(region+0x900)=kind;*reinterpret_cast<uint8_t*>(region+0x8fc)=kind!=0;}return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
Json style_names(Context& c){
    Json out=Json::array();std::set<std::string> unique;
    for(size_t i=0;i<9;++i){auto value=c.text(c.image+0x1a317e0+i*0x80);
        need(unique.insert(value).second,"Region terrain style catalog contains duplicate names");out.push_back(value);}return out;
}
struct StyleCall {bool opened=false,end_attempted=false,ended=false;int calls=0;DWORD cleanup=0;};
DWORD apply_styles(uintptr_t image,uintptr_t region,const Text* values,StyleCall& receipt){
    static constexpr char label[]="region terrain types change";Text description{label,sizeof(label)-1};DWORD code=0;
    __try{
        if(reinterpret_cast<bool(*)()>(image+0x264c30)()){
            reinterpret_cast<void(*)(const void*)>(image+0x265300)(&description);receipt.opened=true;}
        for(int32_t i=0;i<4;++i){reinterpret_cast<void(*)(void*,int32_t,const void*,bool)>(image+0x355c90)(reinterpret_cast<void*>(region),i,values+i,true);++receipt.calls;}
        if(receipt.opened){receipt.end_attempted=true;reinterpret_cast<void(*)()>(image+0x265650)();receipt.ended=true;}
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(receipt.opened&&!receipt.end_attempted){receipt.end_attempted=true;
        __try{reinterpret_cast<void(*)()>(image+0x265650)();receipt.ended=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){receipt.cleanup=GetExceptionCode();}}
    return code;
}
#include "height.inc"
#include "levels.inc"
#include "ownership.inc"
#include "paint.inc"
}
std::optional<Json> query(Context& c,const std::string& operation,const Json& args){
    if(operation=="regions.list"){
        auto offset=integer(args.value("offset",Json(0)),"offset"),limit=integer(args.value("limit",Json(100)),"limit");
        need(offset>=0&&offset<=4096&&limit>0&&limit<=128,"Region page requires offset 0..4096 and limit 1..128");auto rows=index(c);Json out=Json::array();
        for(size_t i=size_t(offset);i<rows.size()&&out.size()<size_t(limit);++i)out.push_back({{"epoch",c.epoch},{"id",rows[i].id}});
        return Json{{"handles",out},{"total",rows.size()},{"next_offset",size_t(offset)+out.size()},{"complete",size_t(offset)+out.size()>=rows.size()}};
    }
    if(operation=="regions.get")return state(c,select(c,args.at("handle")));
    if(operation=="terrain.heights")return height_page(c,args);
    if(operation=="terrain.contours")return terrain_contour_page(c,args);
    if(operation=="regions.styles")return style_names(c);
    if(operation=="regions.presets"){
        auto manager=c.singleton(0x13df7f0);
        auto data=c.read<uintptr_t>(manager+8);auto count=c.read<int32_t>(manager+0x10),capacity=c.read<int32_t>(manager+0x14);
        need(data&&count>0&&count<=4096&&capacity>=count,"Region preset catalog bounds differ");Json out=Json::array();std::set<std::string> names;
        for(int32_t i=0;i<count;++i){auto object=c.read<uintptr_t>(data+size_t(i)*8);auto name=c.text(object+0x18);need(names.insert(name).second,"Region preset catalog has duplicate names");out.push_back(name);}return out;
    }
    return std::nullopt;
}
bool action(Context& c,const std::string& operation,const Json& args,bool& attempted,Json& result,std::string& cause){
    try{
        if(operation=="terrain.paint"){result=paint_terrain(c,args,attempted);cause.clear();return true;}
        if(operation=="regions.purchase"||operation=="regions.sell"){result=edit_ownership(c,args,operation=="regions.sell",attempted);cause.clear();return true;}
        if(operation=="regions.set_levels"){result=edit_levels(c,args,attempted);cause.clear();return true;}
        if(operation=="terrain.change_height"){result=edit_height(c,args,attempted);cause.clear();return true;}
        static const std::map<std::string,std::pair<uint64_t,const char*>> setters{{"regions.set_tint",{0x352980,"tint"}},
            {"regions.set_light",{0x3531d0,"light"}},{"regions.set_ambient",{0x353260,"ambient"}},{"regions.set_sky",{0x350280,"sky"}}};
        need(operation=="regions.rename"||operation=="regions.set_preset"||operation=="regions.set_styles"||operation=="regions.set_particles"||operation=="regions.set_particle_color"||setters.contains(operation),"Unsupported Region action: "+operation);
        c.require_mutation();auto region=select(c,args.at("handle"));auto before=state(c,region),expected=before;
        auto money=height_ledger();
        if(operation=="regions.set_preset"){
            auto name=args.at("name").get<std::string>();need(!name.empty()&&name.size()<=256&&name.find('\0')==std::string::npos,"Region preset requires a catalog name up to 256 bytes");
            Text text{name.data(),name.size()};auto styles=before.at("styles");
            if(name!=before.at("preset").get<std::string>()&&name!="{region_preset_custom}"){
                auto manager=c.singleton(0x13df7f0);c.verify_original(0x2c9d40);uintptr_t preset=0;auto code=preset_lookup(c.image,manager,&text,preset);
                need(!code&&preset&&c.text(preset+0x18)==name,"Native Region preset lookup differs SEH="+std::to_string(code));
                auto data=c.read<uintptr_t>(preset+0x60);need(data&&c.read<int32_t>(preset+0x68)==4&&c.read<int32_t>(preset+0x6c)>=4,"Region preset must contain four native styles");
                styles=Json::array();for(size_t i=0;i<4;++i)styles.push_back(c.text(data+i*32));
            }
            c.verify_original(0x3575d0);expected["preset"]=name;expected["styles"]=styles;attempted=true;
            auto code=rename(c.image+0x3575d0,region.object,&text);need(!code,"Native Region SetPreset failed SEH="+std::to_string(code));
        }else if(operation=="regions.set_styles"){
            const auto& values=args.at("styles");need(values.is_array()&&values.size()==4,"Region styles require exactly four catalog names");
            auto catalog=style_names(c);std::array<std::string,4> strings;std::array<Text,4> native{};
            for(size_t i=0;i<4;++i){need(values[i].is_string(),"Region style name must be a string");strings[i]=values[i].get<std::string>();
                need(std::find(catalog.begin(),catalog.end(),values[i])!=catalog.end(),"Region style is absent from the native catalog");native[i]={strings[i].data(),strings[i].size()};}
            for(auto entry:{0x264c30,0x265300,0x265650,0x355c90})c.verify_original(entry);
            const auto depth=c.read<int32_t>(c.image+0x13beda0);expected["styles"]=values;StyleCall receipt;attempted=true;
            auto code=apply_styles(c.image,region.object,native.data(),receipt);
            need(!code&&!receipt.cleanup&&receipt.calls==4&&receipt.opened==receipt.ended&&c.read<int32_t>(c.image+0x13beda0)==depth,
                "Region native TerrainApply or EndEdit failed SEH="+std::to_string(code)+" cleanup="+std::to_string(receipt.cleanup)+" calls="+std::to_string(receipt.calls));
        }else if(operation=="regions.rename"){
            auto name=args.at("name").get<std::string>();need(!name.empty()&&name.size()<=256&&name.find('\0')==std::string::npos,"Region name requires 1..256 UTF-8 bytes without NUL");
            auto widget=c.read<uintptr_t>(region.object+0x370);ap(c,widget,0x17024b0);need(c.read<uintptr_t>(widget+0x178)==region.object,"Region widget owner differs");
            c.verify_original(0x36bad0);Text text{name.data(),name.size()};expected["name"]=name;attempted=true;
            auto code=rename(c.image+0x36bad0,widget,&text);need(!code,"Region native Rename failed SEH="+std::to_string(code));
        }else if(auto found=setters.find(operation);found!=setters.end()){
            auto values=input(args.at("values"),operation=="regions.set_sky"?16:4);c.verify_original(found->second.first);
            expected[found->second.second]=values;attempted=true;
            auto code=call_vector(c.image+found->second.first,region.object,values.data());need(!code,"Region native appearance setter failed SEH="+std::to_string(code));
        }else{
            if(operation=="regions.set_particles"){
                c.verify_original(0x797d13,0x14);
                auto kind=integer(args.at("type"),"particle type");need(kind>=0&&kind<=6,"Region particle type must be 0..6");expected["particles"]=kind;attempted=true;
                auto code=particle_copy(region.object,int32_t(kind),nullptr);need(!code,"Region native particle fields failed SEH="+std::to_string(code));
            }else{
                c.verify_original(0x797d91,5);
                auto values=input(args.at("values"),4);expected["particle_color"]=values;attempted=true;
                auto code=particle_copy(region.object,0,values.data());need(!code,"Region native particle color failed SEH="+std::to_string(code));
            }
        }
        auto current=select(c,args.at("handle"));need(current.object==region.object,"Region native lifetime changed during mutation");result=state(c,current);
        need(result==expected,"Region complete semantic readback differs after native mutation");need(height_ledger()==money,"Region mutation changed the native ledger");
        refresh_region(c,region.object);cause.clear();return true;
    }catch(const std::exception& error){cause=error.what();return false;}
}
}
