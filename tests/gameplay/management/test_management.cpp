#include "management/management.h"
#include "management/model.h"
#include <cstring>
#include <iostream>
#include <map>

using namespace mt2fw;
namespace g=mt2fw::mt2::gameplay;
namespace m=g::management;
namespace {
std::map<uintptr_t,unsigned char> bytes;
std::map<std::string,uintptr_t> registry;
template<class T>void put(uintptr_t address,const T& value){auto p=reinterpret_cast<const unsigned char*>(&value);for(size_t n=0;n<sizeof(T);++n)bytes[address+n]=p[n];}
void put_text(uintptr_t address,const std::string& value){static uintptr_t next=0x30000000;auto data=next;next+=value.size()+16;put(address,data);put(address+8,uint64_t(value.size()));for(size_t n=0;n<value.size();++n)put(data+n,value[n]);}
void expect(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F>void rejects(F&& f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}expect(rejected,"Invalid input was accepted");}
}
namespace mt2fw::mt2::gameplay {
void Context::require_world() const {if(epoch!=7)throw std::runtime_error("Test world epoch expired");}
void Context::read_bytes(uintptr_t address,void* output,size_t count) const {auto p=static_cast<unsigned char*>(output);for(size_t n=0;n<count;++n){auto it=bytes.find(address+n);if(it==bytes.end())throw std::runtime_error("Test memory absent at "+std::to_string(address+n));p[n]=it->second;}}
std::string Context::text(uintptr_t address,size_t maximum) const {auto p=read<uintptr_t>(address);auto count=read<uint64_t>(address+8);if(count>maximum)throw std::runtime_error("Test native text extent");std::string s(size_t(count),' ');read_bytes(p,s.data(),s.size());return s;}
uintptr_t Context::singleton(uint64_t rva) const {return read<uintptr_t>(image+rva);}
void Context::verify(uint64_t,std::initializer_list<unsigned char>) const {throw std::runtime_error("Synthetic host does not execute native calls");}
void Context::verify_hash(uint64_t,size_t,const std::string&) const {throw std::runtime_error("Synthetic host does not execute native calls");}
void Context::verify_original(uint64_t,size_t) const {throw std::runtime_error("Synthetic host does not execute native calls");}
uintptr_t Context::resolve(const Json& handle,uint64_t table) const {if(handle.at("epoch")!=epoch)throw std::runtime_error("Test handle epoch expired");auto object=registry.at(handle.at("uid").get<std::string>());if(read<uintptr_t>(object)!=image+table)throw std::runtime_error("Test handle native class differs");return object;}
}
int main(){try{
    expect(m::model::integer(Json(0))==0,"Zero int32 rejected");expect(m::model::integer(Json(INT32_MAX))==INT32_MAX,"Max int32 rejected");
    for(Json bad:{Json(-1),Json(uint64_t(INT32_MAX)+1),Json(1.5),Json(true),Json("1")})rejects([&]{m::model::integer(bad);});
    rejects([]{m::model::boolean(Json(1));});rejects([]{m::model::text(Json(std::string("a\0b",3)),20);});
    expect(m::model::next_version({1,2,3},1)==std::array<int32_t,3>{1,2,4},"Hotfix version differs");
    expect(m::model::next_version({1,2,3},2)==std::array<int32_t,3>{1,3,0},"Minor version differs");
    expect(m::model::next_version({1,2,3},3)==std::array<int32_t,3>{2,0,0},"Major version differs");
    rejects([]{m::model::next_version({INT32_MAX,2,3},3);});rejects([]{m::model::next_version({1,2,3},0);});
    rejects([]{m::model::allocation(Json::array({0,0,0,0,0,0,0,0,0,0}));});
    rejects([]{m::model::allocation(Json::array({INT32_MAX,1,0,0,0,0,0,0,0,0}));});
    expect(m::model::allocation(Json::array({1,0,0,0,0,0,0,0,0,0}))[0]==1,"Point allocation differs");
    for(size_t n=0;n<7;++n)expect(m::model::policy_field(m::model::policy_names[n])==n,"Policy field lookup differs");
    rejects([]{m::model::policy_field("unknown");});
    g::Context c{0x10000000,1,7};uintptr_t b=0x20000000;
    put(c.image+0x13df220,b);put(b,uintptr_t(c.image+0x16d97e0));put(b+0x40,int32_t(25));put(b+0x44,int32_t(10));
    auto fees=m::query(c,"management.fees",Json::object());expect(fees&&fees->at("purchase")==25&&fees->at("subscription")==10,"Native fee query differs");
    expect(!m::query(c,"management.unknown",{}),"Unknown query was handled");
    put(b+0x44,int32_t(-1));rejects([&]{m::query(c,"management.fees",{});});put(b+0x44,int32_t(10));
    auto q=uintptr_t(0x21000000);put(c.image+0x13df360,q);put(q,uintptr_t(c.image+0x16fdc60));
    for(size_t n=0;n<2;++n){auto p=q+0x48+n*40;put(p,uintptr_t(c.image+0x16dab90));for(size_t f=0;f<7;++f)if(m::model::policy_integer(f))put(p+m::model::policy_offsets[f],int32_t(f==1?60:2));else put(p+m::model::policy_offsets[f],uint8_t(f==0));}
    auto policies=m::query(c,"management.policies",{});expect(policies&&(*policies)[0]["watchSeconds"]==60&&(*policies)[1]["enabled"]==true,"Typed policy query differs");
    put(q+0x48+0x24,int32_t(31));rejects([&]{m::query(c,"management.policies",{});});
    uintptr_t building=0x22000000,service=0x22100000,config_manager=0x22200000,config=0x22300000,service_array=0x22400000,config_array=0x22500000;
    put(building,uintptr_t(c.image+0x16d93c0));put(building+0x18,uint64_t(900));registry["900"]=building;
    put(building+0x9d8,service_array);put(building+0x9e0,int32_t(1));put(building+0x9e4,int32_t(1));put(service_array,service);
    put(service,uintptr_t(c.image+0x1718480));put(service+0x18,uint64_t(901));registry["901"]=service;put(service+0x148,building);put(service+0x20,int32_t(3));put(service+0x28,uintptr_t(c.image+0x174ab50));put(service+0x40,std::array<int32_t,2>{0,20});put(service+0x48,uint8_t(1));put(service+0x178,uintptr_t(0));put(service+0x150,config);put_text(service+0x158,"healing");
    put(c.image+0x13df840,config_manager);put(config_manager,uintptr_t(c.image+0x173fba0));put(config_manager+0x20,config_array);put(config_manager+0x28,int32_t(1));put(config_manager+0x2c,int32_t(1));put(config_array,config);
    put(config,uintptr_t(c.image+0x1736660));put_text(config+0x18,"healing");put(config+0x38,uintptr_t(c.image+0x174ab50));put(config+0x50,std::array<int32_t,2>{0,20});
    Json service_args={{"building",{{"epoch",7},{"uid","900"}}}};auto services=m::query(c,"management.services",service_args);expect(services&&services->size()==1&&(*services)[0]["config"]["price"]==20&&(*services)[0]["handle"]["uid"]=="901","Native service configuration or UID query differs");
    put(service+0x150,uintptr_t(0x99999999));rejects([&]{m::query(c,"management.services",service_args);});put(service+0x150,config);
    auto one=service_args;one["handle"]={{"epoch",7},{"uid","901"}};
    auto selected=m::query(c,"management.service",one);expect(selected&&*selected==(*services)[0],"Single service lost current configuration or exact UID");
    auto bad=one;bad["handle"]["epoch"]=8;rejects([&]{m::query(c,"management.service",bad);});
    bad=one;bad["handle"]["uid"]="900";rejects([&]{m::query(c,"management.service",bad);});
    bool attempted=false;Json result;std::string cause;
    rejects([&]{m::action(c,"management.set_purchase_fee",{{"value",-1}},attempted,result,cause);});expect(!attempted,"Invalid price attempted native mutation");
    rejects([&]{m::action(c,"management.set_fees",{{"purchase",41},{"subscription",-1}},attempted,result,cause);});expect(!attempted,"Invalid second fee attempted first setter");
    rejects([&]{m::action(c,"management.set_fees",{{"purchase",41},{"subscription",11}},attempted,result,cause);});expect(!attempted,"Unverified paired setters reached native execution");
    rejects([&]{m::action(c,"management.set_policy",{{"policy",2},{"field","enabled"},{"value",true}},attempted,result,cause);});expect(!attempted,"Invalid policy attempted native mutation");
    rejects([&]{m::action(c,"management.set_purchase_fee",{{"value",30}},attempted,result,cause);});expect(!attempted,"Synthetic native guard failure attempted mutation");
    c.epoch=8;rejects([&]{m::query(c,"management.fees",{});});
    std::cout<<"Management tests passed: exact queries, native domains, versions, allocation overflow, pre-mutation guards\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: management tests: "<<e.what()<<'\n';return 1;}}
