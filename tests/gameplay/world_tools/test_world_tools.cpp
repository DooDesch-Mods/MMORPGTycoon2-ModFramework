#include "world_tools/world_tools.h"
#include "world_tools/common.h"
#include <iostream>
#include <map>
using namespace mt2fw;
namespace g=mt2fw::mt2::gameplay;
namespace w=g::world_tools;
namespace {std::map<uintptr_t,unsigned char> memory;template<class T>void put(uintptr_t address,T value){auto bytes=reinterpret_cast<const unsigned char*>(&value);for(size_t n=0;n<sizeof(T);++n)memory[address+n]=bytes[n];}void expect(bool condition){if(!condition)throw std::runtime_error("World tools test assertion failed");}template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}expect(failed);}}
namespace mt2fw::mt2::gameplay {
void Context::require_world()const{if(epoch!=9)throw std::runtime_error("Synthetic world epoch expired");}
void Context::read_bytes(uintptr_t address,void* output,size_t bytes)const{auto p=static_cast<unsigned char*>(output);for(size_t n=0;n<bytes;++n)p[n]=memory.at(address+n);}
std::string Context::text(uintptr_t,size_t)const{throw std::runtime_error("Synthetic native string absent");}
uintptr_t Context::singleton(uint64_t rva)const{auto p=read<uintptr_t>(image+rva);if(!p)throw std::runtime_error("Synthetic singleton absent");return p;}
void Context::verify(uint64_t,std::initializer_list<unsigned char>)const{throw std::runtime_error("Synthetic host has no native executable");}
void Context::verify_hash(uint64_t,size_t,const std::string&)const{throw std::runtime_error("Synthetic host blocks native calls");}
void Context::verify_original(uint64_t,size_t)const{throw std::runtime_error("Synthetic host blocks native calls");}
uintptr_t Context::resolve(const Json&,uint64_t)const{throw std::runtime_error("Synthetic object registry has no handle");}
}
int main(){try{g::Context c{0x10000000,1,9};expect(w::detail::integer(Json(INT32_MAX))==INT32_MAX);rejects([]{w::detail::integer(Json(uint64_t(INT32_MAX)+1));});rejects([]{w::detail::integer(Json(-1));});rejects([]{w::detail::point(Json::array({1,2}));});rejects([]{w::detail::point(Json::array({1,"2",3}));});expect(w::detail::point(Json::array({1,2,3}))[2]==3);w::detail::Ledger money;money.cash=1000;money.record[1]=1000;auto paid=w::detail::payment(money,500,10);expect(paid.cash==500&&paid.record[4]==500&&paid.categories[10][0]==1&&paid.categories[10][1]==-500);rejects([&]{w::detail::payment(money,1001,10);});money.categories[10][0]=INT32_MAX;rejects([&]{w::detail::payment(money,1,10);});
auto lounge=uintptr_t(0x20000000);put(c.image+0x13df290,lounge);put(lounge,c.image+0x16de6c0);put(lounge+0xa8,uintptr_t(0));put(lounge+0xb0,0.0f);put(lounge+0xb4,int32_t(0));put(lounge+0xb8,int32_t(0));put(lounge+0xbc,int32_t(0));put(lounge+0xc4,int32_t(0));auto current=w::query(c,"projects.current",{});expect(current&&current->at("source").is_null()&&current->at("workers")==0);put(lounge+0xbc,int32_t(1));rejects([&]{w::query(c,"projects.current",{});});expect(!w::query(c,"unknown.query",{}));
uintptr_t map=0x21000000,region=0x22000000,regions=0x23000000;put(c.image+0x13df8c0,map);put(map,c.image+0x1742860);put(map+0x358,regions);put(map+0x360,int32_t(1));put(map+0x364,int32_t(1));put(regions,region);put(region,c.image+0x1787930);put(region+0x834,int32_t(3));put(region+0x70,map+0x1c0);put(region+0x978,uintptr_t(0));put(region+0x850,uint8_t(1));auto network=w::query(c,"network.snapshot",{});expect(network&&network->at("nodes").empty()&&network->at("pipes").empty()&&network->at("regions")[0]["id"]==3);put(region+0x70,uintptr_t(0));rejects([&]{w::query(c,"network.snapshot",{});});
c.epoch=10;rejects([&]{w::query(c,"projects.current",{});});bool attempted=false;Json result;std::string cause;rejects([&]{w::action(c,"projects.set_workers",{{"count",1}},attempted,result,cause);});expect(!attempted);std::cout<<"World tools tests passed: project and network queries, source absence, integer/point domains, financial overflow, stale-world and shard rejection\n";return 0;}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
