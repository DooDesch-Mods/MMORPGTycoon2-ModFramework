#include "region_type/region_type.cpp"
#include <iostream>
using namespace mt2fw;
namespace g=mt2fw::mt2::gameplay;
namespace r=g::region_type;
namespace {
void check(bool ok){if(!ok)throw std::runtime_error("Region type test assertion failed");}
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed);}
}
namespace mt2fw::mt2::gameplay {
void Context::require_world()const{if(epoch!=9)throw std::runtime_error("Synthetic world expired");}
void Context::require_mutation()const{require_world();}
void Context::read_bytes(uintptr_t,void*,size_t)const{throw std::runtime_error("Synthetic memory absent");}
std::string Context::text(uintptr_t,size_t)const{throw std::runtime_error("Synthetic string absent");}
uintptr_t Context::singleton(uint64_t)const{throw std::runtime_error("Synthetic singleton absent");}
void Context::verify(uint64_t,std::initializer_list<unsigned char>)const{throw std::runtime_error("Synthetic native call prohibited");}
void Context::verify_hash(uint64_t,size_t,const std::string&)const{throw std::runtime_error("Synthetic native call prohibited");}
void Context::verify_original(uint64_t,size_t)const{throw std::runtime_error("Synthetic native call prohibited");}
uintptr_t Context::resolve(const Json&,uint64_t)const{throw std::runtime_error("Synthetic stale UID");}
namespace regions {std::optional<Json> query(Context&,const std::string&,const Json&){throw std::runtime_error("Synthetic Region query prohibited");}}
}
int main(){try{
check(r::detail::requested_type(0)==0&&r::detail::requested_type(1)==1);
rejects([]{r::detail::requested_type(-1);});rejects([]{r::detail::requested_type(2);});rejects([]{r::detail::requested_type("1");});rejects([]{r::detail::requested_type(uint64_t(UINT64_MAX));});
std::map<std::string,std::set<uint64_t>> members{{"buildings",{1,2}},{"scenery",{3}}};
check(r::detail::union_members(members)==std::set<uint64_t>{1,2,3});members["scenery"].insert(2);rejects([&]{r::detail::union_members(members);});members["scenery"]={0};rejects([&]{r::detail::union_members(members);});
members={{"buildings",{1}},{"scenery",{2}}};auto empty=members;empty["buildings"].clear();empty["scenery"].clear();r::detail::cleared(members,empty);empty["scenery"]={2};rejects([&]{r::detail::cleared(members,empty);});empty.erase("scenery");rejects([&]{r::detail::cleared(members,empty);});
check(r::extent({0,0,0},24,10)==0);check(r::extent({100,148,172},24,10)==2);rejects([]{r::extent({100,149,172},24,10);});rejects([]{r::extent({100,148,124},24,10);});rejects([]{r::extent({100,148,172},24,1);});
r::Closure before,after;
r::Entity node{100,1,7,-1,"path_nodes",{{"position",Json::array({1,2,3})}},{2},{}};
r::Entity path{200,2,8,7,"paths",{{"geometry","complete"}},{},{1,3}};
r::Entity outside{300,3,8,-1,"path_nodes",{{"position",Json::array({4,5,6})}},{2},{}};
before.entities={{1,node},{2,path},{3,outside}};after.entities={{3,outside}};after.entities.at(3).edges.clear();
r::Group group;group.uid=10;group.object=1000;group.region=7;group.state={{"pose",1}};group.members={{20,Json::array({1,2,3})}};
before.groups={{10,group}};after.groups=before.groups;after.groups.at(10).members.clear();
check(r::verify_closure(before,after,7)==std::set<uint64_t>{1,2});
auto wrong=after;wrong.entities.at(3).edges={2};rejects([&]{r::verify_closure(before,wrong,7);});wrong=after;wrong.entities.at(3).state["position"]=Json::array({7,8,9});rejects([&]{r::verify_closure(before,wrong,7);});wrong=after;wrong.groups.at(10).members=group.members;rejects([&]{r::verify_closure(before,wrong,7);});wrong=after;wrong.groups.clear();rejects([&]{r::verify_closure(before,wrong,7);});
r::Entity unrelated{400,4,9,-1,"network_nodes",{}, {},{}};before.entities[4]=unrelated;rejects([&]{r::verify_closure(before,after,7);});before.entities.erase(4);
g::Context c{0x10000000,1,9};bool attempted=false;Json result;std::string cause;
rejects([&]{r::action(c,"regions.set_type",{{"handle",{{"epoch",8},{"id",1}}},{"type",0},{"position",Json::array({1,2,3})}},attempted,result,cause);});check(!attempted);
rejects([&]{r::action(c,"regions.unknown",{},attempted,result,cause);});check(!attempted);check(!r::query(c,"regions.get",{}));
std::cout<<"Region type tests passed: integer domains, full family retirement, polygon bounds, stale handles and unsupported action guards\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
