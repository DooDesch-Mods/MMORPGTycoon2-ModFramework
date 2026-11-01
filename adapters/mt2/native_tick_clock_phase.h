#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace mt2fw::mt2::tick_clock_phase {
struct Clock {
    uint64_t address=0,data=0;
    uint32_t count=0,capacity=0,period=0,budget=0,rate=0,cursor=0;
    std::vector<uint64_t> members;
};
struct Receipt {std::array<Clock,4> before,after;};
inline void need(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
template<class R64,class R32,class R8,class W32,class Guard>
Receipt initialize(uint64_t image,uint64_t game,R64 read64,R32 read32,R8 read8,W32 write32,Guard guard){
    constexpr std::array<float,4> periods{1.0f,15.0f,30.0f,60.0f};
    guard();const auto manager=read64(image+0x13df350);
    auto inspect=[&](){
        guard();need(manager&&read64(image+0x13df350)==manager&&read64(game+0x150)==manager&&read64(manager)==image+0x16fdc40,"TickManager owner changed");
        need(read8(manager+0x28)==0,"TickManager is paused");std::array<Clock,4> clocks{};
        for(size_t i=0;i<clocks.size();++i){auto& c=clocks[i];c.address=read64(manager+8+i*8);
            need(c.address&&read64(c.address)==image+0x1704c80,"TickClock owner changed");
            for(size_t j=0;j<i;++j)need(clocks[j].address!=c.address,"TickClock owners are not distinct");
            c.data=read64(c.address+8);c.count=read32(c.address+0x10);c.capacity=read32(c.address+0x14);
            c.period=read32(c.address+0x18);c.budget=read32(c.address+0x1c);c.rate=read32(c.address+0x20);c.cursor=read32(c.address+0x24);
            need(c.count<=100000&&static_cast<int32_t>(c.capacity)>=0&&c.capacity>=c.count&&(!c.count||c.data),"TickClock extent invalid");
            need(c.period==std::bit_cast<uint32_t>(periods[i]),"TickClock period differs");
            need(c.rate==std::bit_cast<uint32_t>(static_cast<float>(c.count)/periods[i]),"TickClock rate differs");
            const float budget=std::bit_cast<float>(c.budget);need(std::isfinite(budget)&&budget>=0&&static_cast<int32_t>(c.cursor)>=0,"TickClock phase invalid");
            for(uint32_t j=0;j<c.count;++j)c.members.push_back(read64(c.data+uint64_t(j)*8));
        }return clocks;
    };
    Receipt receipt;receipt.before=inspect();
    for(const auto& c:receipt.before){guard();write32(c.address+0x1c,0);guard();write32(c.address+0x24,0);}
    receipt.after=inspect();
    for(size_t i=0;i<receipt.before.size();++i){const auto& a=receipt.before[i];const auto& b=receipt.after[i];
        need(a.address==b.address&&a.data==b.data&&a.count==b.count&&a.capacity==b.capacity&&a.period==b.period&&a.rate==b.rate&&a.members==b.members,"TickClock non-phase state changed");
        need(b.budget==0&&b.cursor==0,"TickClock phase readback differs");
    }
    guard();return receipt;
}
}
