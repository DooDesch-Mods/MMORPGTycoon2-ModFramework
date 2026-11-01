#include "native_tick_clock_phase.h"
#include <cassert>
#include <cstring>
#include <map>
#include <iostream>
using namespace mt2fw::mt2::tick_clock_phase;
struct Fixture {
    uint64_t image=0x10000000,game=0x20000000,manager=0x30000000;std::map<uint64_t,uint8_t> mem;int writes=0;bool valid=true,fail_write=false;
    template<class T>void put(uint64_t p,T v){for(size_t i=0;i<sizeof v;++i)mem[p+i]=uint8_t(uint64_t(v)>>(i*8));}
    template<class T>T get(uint64_t p){uint64_t v=0;for(size_t i=0;i<sizeof(T);++i)v|=uint64_t(mem.at(p+i))<<(i*8);return T(v);}
    Fixture(){put<uint64_t>(image+0x13df350,manager);put<uint64_t>(game+0x150,manager);put<uint64_t>(manager,image+0x16fdc40);put<uint8_t>(manager+0x28,0);
        std::array<uint32_t,4> count{0,1,3,17},budget{0,1062556980,1061184872,1061932097},cursor{0,1,2,7};std::array<float,4> periods{1,15,30,60};
        for(size_t i=0;i<4;++i){uint64_t c=0x40000000+i*0x1000,d=0x50000000+i*0x1000;put<uint64_t>(manager+8+i*8,c);put<uint64_t>(c,image+0x1704c80);put<uint64_t>(c+8,d);put<uint32_t>(c+0x10,count[i]);put<uint32_t>(c+0x14,count[i]);put<uint32_t>(c+0x18,std::bit_cast<uint32_t>(periods[i]));put<uint32_t>(c+0x1c,budget[i]);put<uint32_t>(c+0x20,std::bit_cast<uint32_t>(float(count[i])/periods[i]));put<uint32_t>(c+0x24,cursor[i]);for(uint32_t j=0;j<count[i];++j)put<uint64_t>(d+j*8,0x60000000+i*0x100+j);}}
    Receipt run(){return initialize(image,game,[&](auto p){return get<uint64_t>(p);},[&](auto p){return get<uint32_t>(p);},[&](auto p){return get<uint8_t>(p);},[&](auto p,auto x){++writes;if(fail_write&&writes==2)throw std::runtime_error("write failed");put<uint32_t>(p,x);},[&](){if(!valid)throw std::runtime_error("guard failed");});}
};
int main(){
    Fixture f;auto original=f.mem;auto r=f.run();assert(f.writes==8&&r.before[1].cursor==1&&r.before[3].cursor==7);
    for(size_t i=0;i<4;++i){assert(r.after[i].budget==0&&r.after[i].cursor==0);for(size_t b=0;b<4;++b){original[r.before[i].address+0x1c+b]=0;original[r.before[i].address+0x24+b]=0;}}
    assert(f.mem==original);auto fresh=f.run();for(auto& c:fresh.after)assert(c.budget==0&&c.cursor==0);
    for(int kind=0;kind<8;++kind){Fixture x;uint64_t c=x.get<uint64_t>(x.manager+16);if(kind==0)x.valid=false;if(kind==1)x.put<uint64_t>(x.manager,0);if(kind==2)x.put<uint64_t>(c,0);if(kind==3)x.put<uint32_t>(c+0x14,0);if(kind==4)x.put<uint32_t>(c+0x1c,0x7fc00000);if(kind==5)x.put<uint32_t>(c+0x20,0);if(kind==6)x.put<uint64_t>(x.manager+24,c);if(kind==7)x.put<uint8_t>(x.manager+0x28,1);bool failed=false;try{x.run();}catch(const std::exception&){failed=true;}assert(failed&&x.writes==0);}
    Fixture partial;partial.fail_write=true;bool failed=false;try{partial.run();}catch(const std::exception&){failed=true;}assert(failed&&partial.writes==2);
    std::cout<<"actual TickClock phase helper: PASS\n";
}
