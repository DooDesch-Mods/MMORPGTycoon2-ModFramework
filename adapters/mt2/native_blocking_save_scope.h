#pragma once
#include <cstdint>
#include <stdexcept>
namespace mt2fw::mt2 {
// Created only while the bound ordered owner consumes a prepared capture.
struct PreparedSaveState {
    uintptr_t mode=0,tasks=0;uint32_t thread=0;
    unsigned saves=0,flushes=0,clocks=0,resumes=0;bool asynchronous=false;
};
struct PreparedSaveScope : PreparedSaveState {
    inline static thread_local PreparedSaveScope* current=nullptr;
    PreparedSaveState* retained=nullptr;
    PreparedSaveScope(uintptr_t m,uintptr_t t,uint32_t owner):PreparedSaveState{m,t,owner}{
        if(current)throw std::runtime_error("Prepared snapshot capture reentered");current=this;
    }
    explicit PreparedSaveScope(PreparedSaveState& state):PreparedSaveState(state),retained(&state){
        if(current)throw std::runtime_error("Prepared snapshot pump reentered");current=this;
    }
    ~PreparedSaveScope(){if(retained)*retained=*this;current=nullptr;}
    PreparedSaveScope(const PreparedSaveScope&)=delete;
    PreparedSaveScope& operator=(const PreparedSaveScope&)=delete;
};
struct BlockingSaveScope {
    inline static thread_local BlockingSaveScope* current=nullptr;
    BlockingSaveScope* previous=current;
    uintptr_t mode;uint32_t thread;
    BlockingSaveScope(uintptr_t receiver,uint32_t owner):mode(receiver),thread(owner){current=this;}
    ~BlockingSaveScope(){current=previous;}
    BlockingSaveScope(const BlockingSaveScope&)=delete;
    BlockingSaveScope& operator=(const BlockingSaveScope&)=delete;
};
}
