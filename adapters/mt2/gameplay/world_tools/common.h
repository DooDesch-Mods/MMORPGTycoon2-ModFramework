#pragma once
#include "world_tools.h"
#include <Windows.h>
#include <array>
#include <map>
#include <set>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace mt2fw::mt2::gameplay::world_tools::detail {
inline void need(bool condition,const std::string& cause){if(!condition)throw std::runtime_error(cause);}
inline int32_t integer(const Json& j,int32_t minimum=0,int32_t maximum=INT32_MAX){need(j.is_number_integer()&&(j.is_number_unsigned()?j.get<uint64_t>()<=uint64_t(maximum):j.get<int64_t>()>=minimum&&j.get<int64_t>()<=maximum),"World tools integer outside native domain");return j.get<int32_t>();}
inline std::string text(const Json& j,size_t maximum=200){auto s=j.get<std::string>();need(s.size()<=maximum&&s.find('\0')==std::string::npos,"World tools text extent or NUL invalid");return s;}
using Point=std::array<float,3>;
inline Point point(const Json& j){need(j.is_array()&&j.size()==3,"World tools point requires three coordinates");Point out{};for(size_t n=0;n<3;++n){need(j[n].is_number(),"World tools coordinate must be numeric");out[n]=j[n].get<float>();need(std::isfinite(out[n]),"World tools coordinate is not finite");}return out;}
inline void type(Context& c,uintptr_t p,uint64_t table){need(p&&c.read<uintptr_t>(p)==c.image+table,"World tools native class differs RVA="+std::to_string(table));}
inline uintptr_t owner(Context& c,uint64_t global,uint64_t table){auto p=c.singleton(global);type(c,p,table);return p;}
template<class T=uintptr_t>std::vector<T> array(Context& c,uintptr_t p,size_t offset,size_t maximum=4096){auto data=c.read<uintptr_t>(p+offset);auto count=c.read<int32_t>(p+offset+8),capacity=c.read<int32_t>(p+offset+12);need(count>=0&&size_t(count)<=maximum&&capacity>=count&&capacity<=1000000&&(!capacity||data)&&data<=UINTPTR_MAX-size_t(capacity)*sizeof(T),"World tools owned array extent invalid");std::vector<T> result;for(int32_t n=0;n<count;++n)result.push_back(c.read<T>(data+size_t(n)*sizeof(T)));return result;}
inline std::vector<uintptr_t> objects(Context& c,uintptr_t p,size_t offset,size_t maximum=4096){auto rows=array(c,p,offset,maximum);std::set<uintptr_t> seen;for(auto row:rows)need(row&&seen.insert(row).second,"World tools owned array contains duplicate or null object");return rows;}
inline Json handle(Context& c,uintptr_t p,uint64_t table){auto uid=c.read<uint64_t>(p+0x18);need(uid!=0,"World tools native UID is zero");Json h={{"epoch",c.epoch},{"uid",std::to_string(uid)}};need(c.resolve(h,table)==p,"World tools UID registry owner differs");return h;}
template<class F>auto stable(F&& f){auto a=f(),b=f();need(a==b,"World tools native state changed during double read");return b;}
struct Pin {uint64_t rva;size_t bytes;const char* sha;};
inline constexpr Pin pins[]={
#include "native_pins.inc"
};
inline void code(Context& c,uint64_t rva){auto found=std::find_if(std::begin(pins),std::end(pins),[&](const Pin& p){return p.rva==rva;});if(found==std::end(pins))c.verify_original(rva);else c.verify_hash(rva,found->bytes,found->sha);}
template<class R,class... Args>uint32_t invoke(R(*entry)(Args...),R* value,Args... args)noexcept{__try{*value=entry(args...);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
template<class... Args>uint32_t invoke_void(void(*entry)(Args...),Args... args)noexcept{__try{entry(args...);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
template<class R,class... Args>R call(Context& c,uint64_t rva,Args... args){code(c,rva);R value{};auto fault=invoke(reinterpret_cast<R(*)(Args...)>(c.image+rva),&value,args...);need(!fault,"World tools native call failed RVA="+std::to_string(rva)+" SEH="+std::to_string(fault)+"; no replay");return value;}
template<class... Args>void call_void(Context& c,uint64_t rva,Args... args){code(c,rva);auto fault=invoke_void(reinterpret_cast<void(*)(Args...)>(c.image+rva),args...);need(!fault,"World tools native call failed RVA="+std::to_string(rva)+" SEH="+std::to_string(fault)+"; no replay");}
struct Ledger {int32_t cash=0;std::array<int32_t,5> record{};std::array<std::array<int32_t,2>,20> categories{};bool operator==(const Ledger&)const=default;};
inline Ledger ledger(Context& c){auto b=owner(c,0x13df220,0x16d97e0),r=c.read<uintptr_t>(b+0x100);type(c,r,0x1712c00);need(c.read<int32_t>(r+0x18)==2,"World tools current financial record version differs");Ledger out;out.cash=c.read<int32_t>(b+0x38);out.record=c.read<std::array<int32_t,5>>(r+0x1c);for(size_t n=0;n<20;++n){auto p=r+0x30+n*0x48;type(c,p,0x171d0d0);need(c.read<int32_t>(p+0x20)==int32_t(n),"World tools financial category differs");out.categories[n]=c.read<std::array<int32_t,2>>(p+0x18);}return out;}
inline void add(int32_t& value,int64_t delta){auto sum=int64_t(value)+delta;need(sum>=INT32_MIN&&sum<=INT32_MAX,"World tools financial int32 overflow");value=int32_t(sum);}
inline Ledger payment(Ledger value,int32_t amount,size_t category,bool refund=false){need(amount>=0&&category<20&&(refund||value.cash>=amount),"World tools payment amount, category or affordability invalid");auto delta=refund?int64_t(amount):-int64_t(amount);add(value.cash,delta);add(value.record[1],delta);add(value.record[refund?3:4],amount);add(value.categories[category][0],1);add(value.categories[category][1],delta);return value;}
inline Json financial(const Ledger& l){return {{"cash",l.cash},{"record",l.record},{"categories",l.categories}};}
inline void paid(const Ledger& expected,const Ledger& measured){need(expected==measured,"World tools full financial readback differs cash required="+std::to_string(expected.cash)+" measured="+std::to_string(measured.cash)+"; no replay");}
inline bool done(bool ok,Json before,Json after,Json& result,std::string& cause){result["before"]=std::move(before);result["after"]=std::move(after);if(!ok){cause="World tools native readback differs from intended effect; no replay";return false;}cause.clear();return true;}
}
