#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <stdexcept>

namespace mt2fw::mt2::determinism::presentation {
enum class Site : unsigned { Other, Window, Meter };
inline Site site(uintptr_t return_rva) noexcept {
    if(return_rva==0x81b40f)return Site::Window;
    if(return_rva==0x8410fb)return Site::Meter;
    return Site::Other;
}
struct RandomState { std::array<uint64_t,4> s{};int32_t p=0; };
static_assert(sizeof(RandomState)==40 && alignof(RandomState)==8);
struct Local { RandomState random;uint64_t generation=0; };
struct Counters {
    std::atomic<uint64_t> matched{0},nondefault{0},initialized{0},attempted{0},completed{0};
};
struct Selection { void* source;Counters* count=nullptr; };
struct BuildingWidgetQuery {
    bool active=false;
    bool matches(uintptr_t helper,uintptr_t caller)const noexcept {
        return active&&helper==0x9fcf20&&caller==0xda6f2;
    }
};
template<class Initialize>
Selection select_building_widget(bool armed,uint64_t generation,const BuildingWidgetQuery& query,
    uintptr_t helper,uintptr_t caller,void* source,void* expected,Local& local,
    Counters& count,Initialize&& initialize){
    if(!armed||!query.matches(helper,caller))return {source};
    ++count.matched;
    if(source!=expected){++count.nondefault;return {source};}
    if(!generation)throw std::runtime_error("Building widget RNG generation is zero");
    if(local.generation!=generation){
        initialize(&local.random,uint64_t{0x4d543242574944});
        local.generation=generation;++count.initialized;
    }
    ++count.attempted;return {&local.random,&count};
}
// The NPC validity query tests only whether a respawn exists. Keep its native
// selection algorithm, but give its exact RNG call a presentation source.
struct RespawnQuery {
    bool active=false;
    bool matches(uintptr_t helper,uintptr_t caller) const noexcept {
        return active&&helper==0x9fc370&&caller==0x24e7e8;
    }
};
template<class Initialize>
Selection select_respawn(bool armed,uint64_t generation,const RespawnQuery& query,
    uintptr_t helper,uintptr_t caller,void* source,void* expected,Local& local,
    Counters& count,Initialize&& initialize){
    if(!armed||!query.matches(helper,caller))return {source};
    ++count.matched;
    if(source!=expected){++count.nondefault;return {source};}
    if(!generation)throw std::runtime_error("NPC validity RNG generation is zero");
    if(local.generation!=generation){
        initialize(&local.random,uint64_t{0x4d54324e504356});
        local.generation=generation;++count.initialized;
    }
    ++count.attempted;return {&local.random,&count};
}

// Scenery preview patterns become explicit captured stroke transforms. Their
// Perlin coefficients belong to that local cursor, not the gameplay RNG.
struct SceneryPattern {
    bool active=false;
    static bool constructor(uintptr_t caller) noexcept {
        return caller==0x5d3717||caller==0x5d4045||caller==0x5d3f2d;
    }
    bool matches(uintptr_t helper,uintptr_t caller) const noexcept {
        return active&&helper==0x9fc3d0&&
            (caller==0x9fa7da||caller==0x9fa7ef||caller==0x9fa805);
    }
};
template<class Initialize>
Selection select_scenery_pattern(bool armed,uint64_t generation,const SceneryPattern& query,
    uintptr_t helper,uintptr_t caller,void* source,void* expected,Local& local,
    Counters& count,Initialize&& initialize){
    if(!armed||!query.matches(helper,caller))return {source};
    ++count.matched;
    if(source!=expected){++count.nondefault;return {source};}
    if(!generation)throw std::runtime_error("Scenery pattern RNG generation is zero");
    if(local.generation!=generation){
        initialize(&local.random,uint64_t{0x4d543253435054});
        local.generation=generation;++count.initialized;
    }
    ++count.attempted;return {&local.random,&count};
}

// The Blueprint owns this preview Puppet. Its base constructor randomizes
// only the new Puppet, without advancing the gameplay source.
struct BlueprintPuppet {
    bool active=false;
    bool matches(uintptr_t helper,uintptr_t caller) const noexcept {
        return active&&helper==0x9fc4b0&&caller==0x3b41da;
    }
};
template<class Initialize>
Selection select_blueprint_puppet(bool armed,uint64_t generation,const BlueprintPuppet& query,
    uintptr_t helper,uintptr_t caller,void* source,void* expected,Local& local,
    Counters& count,Initialize&& initialize){
    if(!armed||!query.matches(helper,caller))return {source};
    ++count.matched;
    if(source!=expected){++count.nondefault;return {source};}
    if(!generation)throw std::runtime_error("Blueprint Puppet RNG generation is zero");
    if(local.generation!=generation){
        initialize(&local.random,uint64_t{0x4d543242505550});
        local.generation=generation;++count.initialized;
    }
    ++count.attempted;return {&local.random,&count};
}

// Running characters choose a visible jump animation and its next delay.
// These two exact calls do not select movement or gameplay outcomes.
inline bool character_jump_site(uintptr_t helper,uintptr_t caller) noexcept {
    return (helper==0x9fc370&&caller==0x3a1757)||(helper==0x9fc4b0&&caller==0x3a1777);
}
template<class Initialize>
Selection select_character_jump(bool armed,uint64_t generation,uintptr_t helper,uintptr_t caller,
    void* source,void* expected,Local& local,Counters& count,Initialize&& initialize){
    if(!armed||!character_jump_site(helper,caller))return {source};
    ++count.matched;
    if(source!=expected){++count.nondefault;return {source};}
    if(!generation)throw std::runtime_error("Character jump RNG generation is zero");
    if(local.generation!=generation){
        initialize(&local.random,uint64_t{0x4d543243484a50});
        local.generation=generation;++count.initialized;
    }
    ++count.attempted;return {&local.random,&count};
}

// Each return address is paired with its exact helper; nearby or unknown calls
// retain their receiver. The packed vector helper consumes two transitions.
inline unsigned digital_site(uintptr_t helper,uintptr_t caller) noexcept {
    if(helper==0x9fc370&&caller==0x16cb10)return 0;
    if(helper==0x9fc3d0&&caller==0x16d1b8)return 1;
    if(helper==0x9fc370&&caller==0x179151)return 2;
    if(helper==0x9fc3d0&&caller==0x179139)return 3;
    if(helper==0x9fc430&&caller==0x16df49)return 4;
    if(helper==0x9fc690&&caller==0x16df64)return 5;
    if(helper==0x9fc4b0&&caller==0x167753)return 6;
    if(helper==0x9fc370&&caller==0x16c277)return 7;
    if(helper==0x9fc4b0&&caller==0x16e12e)return 8;
    if(helper==0x9fc690&&caller==0x16e149)return 9;
    if(helper==0x9fc4b0&&caller==0x16e166)return 10;
    if(helper==0x9fc690&&caller==0x16e1ff)return 11;
    if(helper==0x9fc4b0&&caller==0x16e21c)return 12;
    if(helper==0x9fc4b0&&caller==0x16fc07)return 13;
    if(helper==0x9fc4b0&&caller==0x16fe59)return 14;
    if(helper==0x9fc4b0&&caller==0x16fe74)return 15;
    return 16;
}
// The original algorithm receives a separate object only at the two shader calls.
// A failed invocation is never repeated on the gameplay source.
template<class Initialize,class Original>
float dispatch(bool enabled,uint64_t generation,Site caller,void* source,void* expected,
               float limit,Local& local,Counters& count,Initialize&& initialize,Original&& original) {
    if(!enabled||caller==Site::Other)return original(source,limit);
    ++count.matched;
    if(source!=expected){++count.nondefault;return original(source,limit);}
    if(!generation)throw std::runtime_error("Presentation RNG generation is zero");
    if(local.generation!=generation){
        initialize(&local.random,uint64_t{0x4d543250524553});
        local.generation=generation;++count.initialized;
    }
    ++count.attempted;
    const float result=original(&local.random,limit);
    ++count.completed;
    return result;
}
}
