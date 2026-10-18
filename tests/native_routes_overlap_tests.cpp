#include "native_routes.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace mt2fw;
using namespace mt2fw::mt2;
namespace {
int checks=0;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
constexpr uintptr_t image=0x100000;
constexpr size_t description=64,title=80;
constexpr const char* description_name="mp.district-description.4173b0";
constexpr const char* title_name="mp.district-title-position.4173c0";
struct Fixture {
    Runtime runtime{fs::current_path()/"artifacts/overlap-routes"};
    Plugin a{&runtime},b{&runtime};
    NativeRoutes service{[&](const MT2FW_Host* host){return host==&a.host?&a:host==&b.host?&b:nullptr;}};
    std::array<unsigned char,256> pristine{},live{};
    std::map<uintptr_t,void*> slots;
    int enable_calls=0,remove_calls=0;
    bool corrupt_after_enable=false,wrong_proof=false,bad_pristine=false;
    MT2FW_MT2_RoutesV1 api{},other{};
    Fixture(){
        for(auto* owner:{&a,&b}){owner->active=true;owner->staging=false;owner->manifest.type="native";}
        a.manifest.id="test.owner";b.manifest.id="test.other";
        for(size_t i=0;i<pristine.size();++i)pristine[i]=static_cast<unsigned char>(0x40+i%64);
        auto entry=[&](size_t offset,const std::string& bytes){for(size_t i=0;i<bytes.size()/2;++i)pristine[offset+i]=static_cast<unsigned char>(std::stoul(bytes.substr(i*2,2),nullptr,16));};
        // Preserve the actual entry bytes and sixteen-byte separation while
        // keeping the test memory independent of a running game.
        entry(description,"4881c1b8020000e92489da000f1f4000");
        entry(title,"488b02488981440300008b4208c681500300000189814c030000c30f1f440000");
        live=pristine;
        NativeRoutes::Backend backend;
        backend.pristine=[&](uint64_t rva,uint32_t offset,void* out,size_t count,std::string& cause){
            if(rva+offset+count>pristine.size()){cause="pristine bounds";return false;}
            std::memcpy(out,pristine.data()+rva+offset,count);
            if(bad_pristine&&rva==title)static_cast<unsigned char*>(out)[0]^=1;
            return true;
        };
        backend.read=[&](uintptr_t at,void* out,size_t count,std::string& cause){
            if(at<image||at-image+count>live.size()){cause="live bounds";return false;}
            std::memcpy(out,live.data()+at-image,count);return true;
        };
        backend.create=[&](void* at,void* detour,void** original,std::string& cause){
            auto address=reinterpret_cast<uintptr_t>(at);
            if(slots.contains(address)){cause="foreign existing slot";return false;}
            slots[address]=detour;*original=reinterpret_cast<void*>(address+0x10000);return true;
        };
        backend.enable=[&](void* at,void* detour,std::string& cause){
            ++enable_calls;auto address=reinterpret_cast<uintptr_t>(at);
            if(!slots.contains(address)||slots.at(address)!=detour){cause="wrong prepared detour";return false;}
            const auto offset=address-image;live[offset]=0xe9;
            int32_t displacement=int32_t(reinterpret_cast<uintptr_t>(detour)-(address+5));std::memcpy(live.data()+offset+1,&displacement,4);
            if(corrupt_after_enable)live[offset+23]^=1;
            return true;
        };
        backend.prove=[&](void* at,void* detour,std::string& cause){
            const auto address=reinterpret_cast<uintptr_t>(at),offset=address-image;
            int32_t displacement=0;std::memcpy(&displacement,live.data()+offset+1,4);
            if(!wrong_proof&&live[offset]==0xe9&&address+5+displacement==reinterpret_cast<uintptr_t>(detour))return true;
            cause="wrong exact owned detour";return false;
        };
        backend.disable=[&](void* at,std::string&){auto offset=reinterpret_cast<uintptr_t>(at)-image;std::copy_n(pristine.begin()+offset,5,live.begin()+offset);return true;};
        backend.remove=[&](void* at,std::string&){++remove_calls;auto address=reinterpret_cast<uintptr_t>(at),offset=address-image;std::copy_n(pristine.begin()+offset,5,live.begin()+offset);slots.erase(address);return true;};
        service.configure(image,std::move(backend));
        check(service.query(&a.host,1,&api,sizeof(api))==MT2FW_MT2_OK,"owner service query");
        check(service.query(&b.host,1,&other,sizeof(other))==MT2FW_MT2_OK,"other service query");
        service.catalog(description_name,description);service.catalog(title_name,title);
    }
    MT2FW_MT2_RouteReceipt prepare(const char* name,uintptr_t detour){
        MT2FW_MT2_RouteReceipt r{sizeof(r)};check(api.hook_prepare(api.context,name,reinterpret_cast<void*>(detour),&r)==MT2FW_MT2_OK,"prepare adjacent target");return r;
    }
    MT2FW_MT2_RouteReceipt enable(const char* name){
        MT2FW_MT2_RouteReceipt r{sizeof(r)};auto code=api.hook_enable(api.context,name,&r);
        if(code!=MT2FW_MT2_OK)throw std::runtime_error(std::string("enable adjacent target: ")+r.cause);
        check(r.state==MT2FW_MT2_ROUTE_ENABLED,"enabled receipt");return r;
    }
    void verify(const char* name,size_t offset,uint64_t generation){
        MT2FW_MT2_RouteReceipt r{sizeof(r)};
        check(api.route_read(api.context,name,&r)==MT2FW_MT2_OK,"repeated adjacent route read");
        check(r.generation==generation,"neighbour lifecycle does not change own generation");
        check(std::memcmp(r.pristine,pristine.data()+offset,32)==0,"full original32 unchanged");
        check(std::memcmp(r.current,live.data()+offset,32)==0,"full current32 includes acknowledged adjacent patch");
        check(api.code_verify(api.context,name,0,pristine.data()+offset,32,&r)==MT2FW_MT2_OK,"complete original32 and both owned routes verify together");
    }
};
void install_orders(bool reverse,bool prepare_both){
    Fixture f;auto first=reverse?title_name:description_name,second=reverse?description_name:title_name;
    auto initial=f.prepare(first,0x200100);MT2FW_MT2_RouteReceipt later{sizeof(later)};
    if(prepare_both)later=f.prepare(second,0x200200);
    f.enable(first);if(!prepare_both)later=f.prepare(second,0x200200);f.enable(second);
    const auto desc_generation=reverse?later.generation:initial.generation,title_generation=reverse?initial.generation:later.generation;
    for(int i=0;i<8;++i){f.verify(description_name,description,desc_generation);f.verify(title_name,title,title_generation);}
    MT2FW_MT2_RouteReceipt r{sizeof(r)};
    check(f.other.hook_remove(f.other.context,title_name,&r)==MT2FW_MT2_UNAVAILABLE,"foreign owner cannot remove neighbour");
    check(f.api.hook_remove(f.api.context,title_name,&r)==MT2FW_MT2_OK,"remove title without removing description bytes");
    f.verify(description_name,description,desc_generation);
    auto newer=f.prepare(title_name,0x200300);check(newer.generation>title_generation,"reprepare publishes new own generation");f.enable(title_name);
    f.verify(title_name,title,newer.generation);f.verify(description_name,description,desc_generation);
    check(f.service.disable(f.a,description_name,&r)==MT2FW_MT2_OK,"disable description preserves title patch");
    f.verify(title_name,title,newer.generation);
    check(f.service.resume(f.a,description_name,&r)==MT2FW_MT2_OK,"resume description with active title");
    f.verify(description_name,description,desc_generation);f.verify(title_name,title,newer.generation);
    check(f.service.disable(f.a,title_name,&r)==MT2FW_MT2_OK,"disable title preserves description");
    check(f.service.disable(f.a,description_name,&r)==MT2FW_MT2_OK,"disable description with disabled neighbour");
    check(f.live==f.pristine,"disabled pair restores complete pristine region");
}
void rejects_foreign(){
    Fixture f;f.prepare(description_name,0x200100);f.enable(description_name);f.prepare(title_name,0x200200);f.enable(title_name);
    MT2FW_MT2_RouteReceipt r{sizeof(r)};
    auto saved=f.live;f.live[title+1]^=0x40;
    check(f.api.route_read(f.api.context,description_name,&r)==MT2FW_MT2_FAILED&&std::string(r.cause).find("differs")!=std::string::npos,"foreign overlapping bytes fail with byte cause");
    auto removals=f.remove_calls;check(f.api.hook_remove(f.api.context,description_name,&r)==MT2FW_MT2_FAILED&&f.remove_calls==removals,"foreign overlap is not overwritten in cleanup");
    f.live=saved;check(f.api.hook_remove(f.api.context,description_name,&r)==MT2FW_MT2_OK,"explicit cleanup after exact acknowledged bytes restored");
    f.verify(title_name,title,1);
    f.live[title+27]^=1;
    check(f.api.route_read(f.api.context,title_name,&r)==MT2FW_MT2_FAILED&&std::string(r.cause).find("byte=27")!=std::string::npos,"whole32 immutable neighbour tail retained");
    f.live=saved;std::copy_n(f.pristine.begin()+description,5,f.live.begin()+description);
    f.wrong_proof=true;check(f.api.route_read(f.api.context,title_name,&r)==MT2FW_MT2_FAILED&&std::string(r.cause).find("detour")!=std::string::npos,"wrong owned detour proof retained");
    f.wrong_proof=false;
    auto expected=f.pristine;expected[title+17]^=1;
    check(f.api.code_verify(f.api.context,title_name,0,expected.data()+title,32,&r)==MT2FW_MT2_FAILED&&std::string(r.cause).find("Pristine")!=std::string::npos,"incorrect underlying original guard rejected");
}
void rejects_during_admission(){
    Fixture f;MT2FW_MT2_RouteReceipt r{sizeof(r)};
    f.prepare(description_name,0x200100);f.enable(description_name);f.prepare(title_name,0x200200);
    f.live[description+7]^=1;auto enables=f.enable_calls;
    check(f.api.hook_enable(f.api.context,title_name,&r)==MT2FW_MT2_FAILED&&f.enable_calls==enables&&std::string(r.cause).find("differs")!=std::string::npos,"known adjacent foreign bytes reject before enable");
    Fixture after;after.prepare(title_name,0x200200);after.corrupt_after_enable=true;
    check(after.api.hook_enable(after.api.context,title_name,&r)==MT2FW_MT2_FAILED&&std::string(r.cause).find("outside its owned jump")!=std::string::npos,"enable cannot acknowledge unrelated modified tail");
    bool rejected=false;
    NativeRoutes inconsistent([](const MT2FW_Host*){return static_cast<Plugin*>(nullptr);});NativeRoutes::Backend backend;
    backend.pristine=[](uint64_t rva,uint32_t,void* out,size_t length,std::string&){std::memset(out,rva==64?0x90:0xcc,length);return true;};
    inconsistent.configure(image,std::move(backend));inconsistent.catalog("first",64);
    try{inconsistent.catalog("second",80);}catch(const std::exception& error){rejected=std::string(error.what()).find("Overlapping pristine")!=std::string::npos;}
    check(rejected,"inconsistent overlapping original receipts rejected before mutation");
}
}
int main(){try{for(bool reverse:{false,true})for(bool both:{false,true})install_orders(reverse,both);rejects_foreign();rejects_during_admission();std::cout<<checks<<" adjacent native route checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<"Adjacent route check failed after "<<checks<<": "<<error.what()<<'\n';return 1;}}
