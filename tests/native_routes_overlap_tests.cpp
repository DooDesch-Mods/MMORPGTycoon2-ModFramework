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
    int batch_calls=0,batch_fail_after=-1;
    bool batch_throw=false,batch_no_effect=false,read_fail=false;
    bool post_read_throw=false,post_prove_throw=false;
    bool corrupt_after_enable=false,wrong_proof=false,bad_pristine=false;
    MT2FW_MT2_RoutesV1 api{},other{};
    Fixture(bool with_batch=true){
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
            if(post_read_throw&&batch_calls)throw std::runtime_error("synthetic post-activation read exception");
            if(read_fail){cause="synthetic unreadable batch code";return false;}
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
            if(post_prove_throw&&batch_calls)throw std::runtime_error("synthetic post-activation prove exception");
            const auto address=reinterpret_cast<uintptr_t>(at),offset=address-image;
            int32_t displacement=0;std::memcpy(&displacement,live.data()+offset+1,4);
            if(!wrong_proof&&live[offset]==0xe9&&address+5+displacement==reinterpret_cast<uintptr_t>(detour))return true;
            cause="wrong exact owned detour";return false;
        };
        if(with_batch)backend.enable_batch=[this,enable=backend.enable](const std::vector<std::pair<void*,void*>>& hooks,std::string& cause){
            ++batch_calls;if(batch_throw)throw std::runtime_error("synthetic batch exception");
            if(batch_no_effect)return true;
            int applied=0;
            for(const auto& [address,detour]:hooks){
                if(applied==batch_fail_after){cause="synthetic partial batch failure";return false;}
                if(!enable(address,detour,cause))return false;++applied;
            }
            return true;
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
MT2FW_MT2_RoutesV2 batch_api(Fixture& f){
    MT2FW_MT2_RoutesV2 api{};
    check(f.service.query(&f.a.host,2,&api.v1,sizeof(api))==MT2FW_MT2_OK,"discover version2 with original export and prefix");
    check(api.v1.size==sizeof(api)&&api.v1.version==2&&api.v1.context==f.api.context&&api.hook_enable_batch,"version2 size and authenticated context");
    return api;
}
void batch_contract(){
    static_assert(offsetof(MT2FW_MT2_RoutesV2,v1)==0);
    static_assert(offsetof(MT2FW_MT2_RoutesV2,hook_enable_batch)==sizeof(MT2FW_MT2_RoutesV1));
    for(bool reverse:{false,true}){
        Fixture f;auto api=batch_api(f);f.prepare(description_name,0x200100);f.prepare(title_name,0x200200);
        const char* names[]={reverse?title_name:description_name,reverse?description_name:title_name};
        MT2FW_MT2_RouteReceipt out[2]{{sizeof(out[0])},{sizeof(out[1])}};
        check(api.hook_enable_batch(api.v1.context,names,2,out,2)==MT2FW_MT2_OK,"adjacent owned batch activation");
        check(f.batch_calls==1&&out[0].state==MT2FW_MT2_ROUTE_ENABLED&&out[1].state==MT2FW_MT2_ROUTE_ENABLED,"one batch and two confirmed enabled receipts");
        check(out[0].rva==(reverse?title:description)&&out[1].rva==(reverse?description:title),"receipt order matches caller order");
        check(std::string(out[0].owner)=="test.owner"&&out[0].generation==1&&out[1].generation==1,"each target retains owner and generation");
        f.verify(description_name,description,1);f.verify(title_name,title,1);
        auto count=f.batch_calls;check(api.hook_enable_batch(api.v1.context,names,2,out,2)==MT2FW_MT2_INVALID&&f.batch_calls==count,"enabled batch is never replayed");
    }
    Fixture f;auto api=batch_api(f);f.prepare(description_name,0x200100);f.prepare(title_name,0x200200);
    const char* names[]={description_name,title_name};MT2FW_MT2_RouteReceipt out[2]{{sizeof(out[0])},{sizeof(out[1])}};
    auto saved=f.live;
    check(api.hook_enable_batch(api.v1.context,names,0,out,2)==MT2FW_MT2_INVALID,"empty batch rejected");
    check(api.hook_enable_batch(api.v1.context,names,129,out,2)==MT2FW_MT2_INVALID,"batch budget enforced before reading arrays");
    check(api.hook_enable_batch(api.v1.context,nullptr,2,out,2)==MT2FW_MT2_INVALID,"null name array rejected");
    check(api.hook_enable_batch(api.v1.context,names,2,out,1)==MT2FW_MT2_INVALID,"receipt capacity is an element count");
    out[1].size=0;check(api.hook_enable_batch(api.v1.context,names,2,out,2)==MT2FW_MT2_INVALID,"all receipt sizes checked before admission");out[1].size=sizeof(out[1]);
    for(const char* bad:{description_name,"unknown",static_cast<const char*>(nullptr)}){
        const char* invalid[]={description_name,bad};check(api.hook_enable_batch(api.v1.context,invalid,2,out,2)!=MT2FW_MT2_OK,"duplicate, absent or null target rejects whole batch");
    }
    f.service.catalog("foreign",128);MT2FW_MT2_RouteReceipt foreign{sizeof(foreign)};
    check(f.other.hook_prepare(f.other.context,"foreign",reinterpret_cast<void*>(0x200300),&foreign)==MT2FW_MT2_OK,"foreign owner prepares independent slot");
    const char* mixed[]={description_name,"foreign"};check(api.hook_enable_batch(api.v1.context,mixed,2,out,2)==MT2FW_MT2_UNAVAILABLE,"foreign targets cannot enter an owned batch");
    check(f.batch_calls==0&&f.live==saved,"all admission rejections leave code and backend untouched");
    f.live[title+27]^=1;check(api.hook_enable_batch(api.v1.context,names,2,out,2)==MT2FW_MT2_FAILED&&f.batch_calls==0,"foreign tail rejects before backend call");f.live=saved;
    f.read_fail=true;check(api.hook_enable_batch(api.v1.context,names,2,out,2)==MT2FW_MT2_FAILED&&f.batch_calls==0,"unreadable admission rejects before backend call");f.read_fail=false;
    check(f.api.hook_enable(f.api.context,description_name,out)==MT2FW_MT2_OK&&f.enable_calls==1&&f.batch_calls==0,"V1 immediate enable stays immediate with a prepared neighbour");
    check(api.hook_enable_batch(api.v1.context,names,2,out,2)==MT2FW_MT2_INVALID&&f.batch_calls==0,"mixed enabled and prepared cohort rejects before backend");
    Fixture missing(false);auto missing_api=batch_api(missing);missing.prepare(title_name,0x200200);
    const char* one[]={title_name};check(missing_api.hook_enable_batch(missing_api.v1.context,one,1,out,2)==MT2FW_MT2_UNAVAILABLE&&out[0].state==MT2FW_MT2_ROUTE_PREPARED,"missing backend preserves prepared state, without per-hook fallback");
    struct Guard {MT2FW_MT2_RoutesV1 api{};uint64_t canary=0x1122334455667788;} guard;
    check(f.service.query(&f.a.host,1,&guard.api,sizeof(guard.api))==MT2FW_MT2_OK&&guard.api.version==1&&guard.api.size==sizeof(guard.api)&&guard.canary==0x1122334455667788,"old V1 query never overwrites its buffer");
    check(f.service.query(&f.a.host,2,&guard.api,sizeof(guard.api))==MT2FW_MT2_INVALID&&guard.canary==0x1122334455667788,"short V2 buffer rejected without write");
    check(f.service.retire(f.a),"fixture owner retired");check(api.hook_enable_batch(api.v1.context,names,2,out,2)==MT2FW_MT2_UNAVAILABLE,"retired owner cannot batch");
}
void batch_failures(){
    for(int mode=0;mode<7;++mode){
        Fixture f;auto api=batch_api(f);f.prepare(description_name,0x200100);f.prepare(title_name,0x200200);
        if(mode==0)f.batch_fail_after=0;if(mode==1)f.batch_fail_after=1;if(mode==2)f.batch_throw=true;if(mode==3)f.batch_no_effect=true;if(mode==4)f.corrupt_after_enable=true;
        if(mode==5)f.post_read_throw=true;if(mode==6)f.post_prove_throw=true;
        const char* names[]={description_name,title_name};MT2FW_MT2_RouteReceipt out[2]{{sizeof(out[0])},{sizeof(out[1])}};
        check(api.hook_enable_batch(api.v1.context,names,2,out,2)==MT2FW_MT2_FAILED,"failed, partial, throwing or false-success backend cannot report success");
        check(f.batch_calls==1&&out[0].state==MT2FW_MT2_ROUTE_UNKNOWN&&out[1].state==MT2FW_MT2_ROUTE_UNKNOWN,"entire failed cohort stays unknown");
        check(out[0].cause[0]&&out[1].cause[0]&&std::string(out[0].owner)=="test.owner","failed receipts preserve original cause and authenticated owner");
        if(mode!=5)check(std::memcmp(out[0].current,f.live.data()+description,32)==0&&std::memcmp(out[1].current,f.live.data()+title,32)==0,"failure readback contains actual bytes for both routes");
        else check(std::string(out[0].cause).find("readback:")!=std::string::npos,"unreadable failed receipt does not claim a live observation");
        check(api.hook_enable_batch(api.v1.context,names,2,out,2)==MT2FW_MT2_INVALID&&f.batch_calls==1,"unknown batch never auto-replayed");
        check(!f.service.drain_errors().empty(),"original batch failure remains visible to runtime diagnostics");
    }
}
}
int main(){try{for(bool reverse:{false,true})for(bool both:{false,true})install_orders(reverse,both);rejects_foreign();rejects_during_admission();batch_contract();batch_failures();std::cout<<checks<<" adjacent and batch native route checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<"Adjacent route check failed after "<<checks<<": "<<error.what()<<'\n';return 1;}}
