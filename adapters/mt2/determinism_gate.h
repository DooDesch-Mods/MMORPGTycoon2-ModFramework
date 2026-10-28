#pragma once
#include <cstdint>
#include <array>
#include <set>
#include <stdexcept>
#include <string>

namespace mt2fw::mt2::determinism {
// Private experiment only. One permit is one complete original coreGame::Go.
// No multiplayer role, state adoption or deterministic success is implied.
class Gate {
public:
    enum class Phase { Passive, Arming, Preload, Loading, Held, Stepping, Failed };
    Phase phase=Phase::Passive;
    std::string experiment,error;
    uint32_t dt_bits=0;
    uint64_t completed=0,attempted=0;
    uint32_t remaining=0;
    bool entered=false;
    std::set<std::string> tokens;
    static bool identifier(const std::string& value){
        return !value.empty()&&value.size()<=64&&value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")==std::string::npos;
    }
    bool active()const{return phase!=Phase::Passive;}
    bool held()const{return phase==Phase::Preload||phase==Phase::Held||phase==Phase::Failed;}
    const char* name()const{
        switch(phase){case Phase::Passive:return "passive";case Phase::Arming:return "arming";case Phase::Preload:return "preload";case Phase::Loading:return "loading";
        case Phase::Held:return completed?"stepped":"held";case Phase::Stepping:return "stepping";case Phase::Failed:return "failed";}
        return "failed";
    }
    void fail(const std::string& cause){error=cause.empty()?"Determinism gate failed without cause":cause;phase=Phase::Failed;remaining=0;}
    void arm(const std::string& id,uint32_t dt){
        if(active()||entered||!experiment.empty())throw std::runtime_error("Determinism gate was already armed; use a fresh owned process");
        if(!identifier(id)||dt!=0x3c800000u)throw std::runtime_error("Experiment requires a bounded ID and exact 1/64 second dt bits");
        experiment=id;dt_bits=dt;completed=attempted=remaining=0;tokens.clear();error.clear();phase=Phase::Arming;
    }
    void seeded(){if(phase!=Phase::Arming||entered)throw std::runtime_error("Seed barrier requires an unentered outer frame");phase=Phase::Preload;}
    void start_loading(){if(phase!=Phase::Preload||entered)throw std::runtime_error("Load requires the held seed barrier");phase=Phase::Loading;}
    static std::array<uint64_t,4> seed_words(uint64_t seed){
        if(!seed)seed=4;
        std::array<uint64_t,4> words{};
        for(auto& word:words){seed^=seed>>12;seed^=seed<<25;seed^=seed>>27;word=seed*2685821657736338717ull;}
        return words;
    }
    void loaded(){if(phase!=Phase::Loading||entered)throw std::runtime_error("Loaded barrier is outside completed loading frame");phase=Phase::Held;}
    void step(const std::string& token,uint64_t expected,uint32_t count){
        if(phase!=Phase::Held||entered)throw std::runtime_error("Step requires a completed held barrier");
        if(!identifier(token)||tokens.contains(token))throw std::runtime_error("Step token is invalid or was already attempted; no replay");
        if(expected!=completed||count<1||count>64||completed>UINT64_MAX-count||tokens.size()>=256)
            throw std::runtime_error("Step index/count/token budget differs");
        tokens.insert(token);remaining=count;phase=Phase::Stepping;
    }
    bool enter(){
        if(entered){fail("Nested original outer frame rejected");return false;}
        if(held())return false;
        entered=true;if(phase==Phase::Stepping)++attempted;return true;
    }
    void returned(){
        if(!entered)throw std::runtime_error("Outer frame return has no matching entry");
        entered=false;if(phase==Phase::Stepping){++completed;if(!--remaining)phase=Phase::Held;}
    }
    void raised(const std::string& cause){entered=false;fail(cause);}
    void cancel(){if(entered)throw std::runtime_error("Cannot release an entered outer frame");phase=Phase::Passive;remaining=0;}
};
}
