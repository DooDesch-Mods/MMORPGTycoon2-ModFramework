#pragma once
#include "modular.h"
#include <Windows.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <set>
#include <map>
#include <cstring>
namespace mt2fw::mt2::gameplay::modular::detail {
inline void need(bool yes,const std::string& why){if(!yes)throw std::runtime_error(why);}
using Point=std::array<float,3>;
inline Point point(const Json& j){need(j.is_array()&&j.size()==3,"Modular point requires three coordinates");Point out{};for(size_t n=0;n<3;++n){need(j[n].is_number(),"Modular coordinate must be numeric");out[n]=j[n].get<float>();need(std::isfinite(out[n]),"Modular coordinate is not finite");}return out;}
inline void type(Context& c,uintptr_t p,uint64_t ap){need(p&&c.read<uintptr_t>(p)==c.image+ap,"Modular native class differs RVA="+std::to_string(ap));}
struct Pin {uint64_t rva;size_t bytes;const char* sha;};
inline constexpr Pin pins[]={
#include "native_pins.inc"
};
inline void code(Context& c,uint64_t rva){auto p=std::find_if(std::begin(pins),std::end(pins),[&](const Pin& pin){return pin.rva==rva;});if(p==std::end(pins))c.verify_original(rva);else c.verify_hash(rva,p->bytes,p->sha);}
template<class R,class... A>uint32_t invoke(R(*fn)(A...),R* result,A... args)noexcept{__try{*result=fn(args...);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
template<class... A>uint32_t invoke_void(void(*fn)(A...),A... args)noexcept{__try{fn(args...);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
template<class R,class... A>R call(Context& c,uint64_t rva,A... args){code(c,rva);R out{};auto fault=invoke(reinterpret_cast<R(*)(A...)>(c.image+rva),&out,args...);need(!fault,"Modular native call failed RVA="+std::to_string(rva)+" SEH="+std::to_string(fault)+"; no replay");return out;}
template<class... A>void call_void(Context& c,uint64_t rva,A... args){code(c,rva);auto fault=invoke_void(reinterpret_cast<void(*)(A...)>(c.image+rva),args...);need(!fault,"Modular native call failed RVA="+std::to_string(rva)+" SEH="+std::to_string(fault)+"; no replay");}
struct String {const char* data;size_t size;unsigned char rest[16]{};};
struct Release {uintptr_t object=0;std::array<int32_t,8> edits{};bool operator==(const Release&)const=default;};
inline Release release(Context& c){auto manager=c.singleton(0x13df4f0);type(c,manager,0x1714ed0);Release out;out.object=c.read<uintptr_t>(manager+0x38);type(c,out.object,0x16c12b0);out.edits=c.read<std::array<int32_t,8>>(out.object+0x120);return out;}
inline Release released(Release r){need(r.edits[1]<INT32_MAX,"Modular release edit counter is at its int32 bound");++r.edits[1];return r;}
inline void* ptr(uintptr_t p){return reinterpret_cast<void*>(p);}
inline std::vector<uintptr_t> list(Context& c,uintptr_t p,uint64_t ap,size_t limit=100000){type(c,p,ap);auto data=c.read<uintptr_t>(p+8);auto n=c.read<int32_t>(p+16),capacity=c.read<int32_t>(p+20);need(n>=0&&size_t(n)<=limit&&capacity>=n&&capacity<=1000000&&(!capacity||data),"Modular owned array extent invalid");std::vector<uintptr_t> out;for(int32_t i=0;i<n;++i){auto x=c.read<uintptr_t>(data+size_t(i)*8);need(x!=0,"Modular owned array contains null");out.push_back(x);}return out;}
struct Stack {uintptr_t manager=0,active=0;std::vector<uintptr_t> undo,redo;bool operator==(const Stack&)const=default;};
inline Stack stack(Context& c){auto read=[&]{Stack s;s.manager=c.singleton(0x13df380);type(c,s.manager,0x16fe070);s.active=c.read<uintptr_t>(c.image+0x1a3aa80);s.undo=list(c,s.manager+8,0x16e1320,20);s.redo=list(c,s.manager+0x20,0x16e1320,20);return s;};auto a=read(),b=read();need(a==b,"Modular Undo stacks changed during read");return b;}
inline void graph(Context& c,uintptr_t p){type(c,p,0x16e32d0);list(c,p+0x18,0x16e2dc0);need(list(c,p+0x48,0x1748c50).empty()&&c.read<uint8_t>(p+0x10c)==1,"Modular Undo graph is not locked and drained");}
inline void commit_stack(const Stack& a,const Stack& b,uintptr_t graph){auto expected=a.undo;expected.push_back(graph);if(expected.size()>20)expected.erase(expected.begin());need(a.manager==b.manager&&!a.active&&!b.active&&b.redo.empty()&&b.undo==expected&&std::find(a.undo.begin(),a.undo.end(),graph)==a.undo.end(),"Modular Undo commit readback differs; no replay");}
inline void history_stack(const Stack& a,const Stack& b,bool undo,uintptr_t& inverse){auto source=undo?a.undo:a.redo;auto other=undo?a.redo:a.undo;need(!source.empty(),"Modular history source is empty");source.pop_back();auto actual=undo?b.redo:b.undo;need(!actual.empty(),"Modular history inverse missing");inverse=actual.back();need(std::find(other.begin(),other.end(),inverse)==other.end(),"Modular history inverse graph is not new");other.push_back(inverse);if(other.size()>20)other.erase(other.begin());need(a.manager==b.manager&&!a.active&&!b.active&&(undo?b.undo:b.redo)==source&&actual==other,"Modular history stack transition differs; no replay");}
Json record(Context&,uintptr_t);
Json capture(Context&,uintptr_t,int kind);
struct Building {uintptr_t object=0,root=0,region=0,layout=0,plan=0;uint64_t uid=0;std::array<unsigned char,0x6c> pose{};Json layout_record,plan_record;};
Building building(Context&,const Json&);
Json value(Context&,const Building&);
}
