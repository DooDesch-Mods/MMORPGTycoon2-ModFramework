#pragma once
#include "sdk/native_steps_draft.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>

namespace mt2fw::mt2::step_checkpoint {
using Json=nlohmann::json;
inline void require(bool ok,const char* cause){if(!ok)throw std::runtime_error(cause);}
struct Selection {
    unsigned attempted=0;
    std::array<uint64_t,2> ticks{256,512};
    void configure(const Json& value){
        require(!attempted,"Private Step checkpoint selection is already used");
        require(value.is_object()&&value.size()==1&&value.contains("ticks")&&value.at("ticks").is_array()&&value.at("ticks").size()==2,
            "Private Step checkpoint override requires exactly two ticks");
        std::array<uint64_t,2> requested{};
        for(size_t i=0;i<requested.size();++i){const auto& tick=value.at("ticks")[i];
            require(tick.is_number_integer()&&tick>0,"Private Step checkpoint tick must be a positive integer");
            requested[i]=tick.get<uint64_t>();}
        require(requested[0]<requested[1],"Private Step checkpoint ticks must be distinct and increasing");
        ticks=requested;
    }
    bool claim(const MT2FW_MT2_StepReceipt& value){
        const unsigned bit=value.completed_tick==ticks[0]?1u:value.completed_tick==ticks[1]?2u:0u;
        if(!bit||(attempted&bit))return false;
        attempted|=bit;
        require(value.size==sizeof(value)&&value.phase==MT2FW_MT2_STEP_HELD&&value.outcome==MT2FW_MT2_STEP_CONFIRMED&&
            !value.callbacks_inflight&&value.tick==value.completed_tick&&value.invocation&&value.lease&&value.session&&value.thread&&
            value.generation&&value.world_epoch&&value.original_attempted==1&&value.original_returned==1&&value.timer_calls==1&&
            value.workers_held==1&&value.dt_bits==MT2FW_MT2_STEP_FIXED_DT_BITS&&value.seed_attempted==1&&value.seed_returned==1&&
            value.bootstrap_held==1&&value.bootstrap_coverage==MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS,
            "Private Step checkpoint lacks confirmed completed held tick");
        return true;
    }
};
template<class Work>Json canonical(Work& work,uint64_t tick,const std::string& snapshot,uint64_t seed,
    const std::array<uint64_t,4>& random,int32_t extra,const Json& cash,const Json& logged_in){
    auto developers=work.developers(false).at("comparable");
    auto& staff=developers.at("developers");
    std::sort(staff.begin(),staff.end(),[](const Json& a,const Json& b){return a.at("id").get<int32_t>()<b.at("id").get<int32_t>();});
    auto subscribers=work.subscribers().at("comparable");
    auto& rows=subscribers.at("rows");
    std::sort(rows.begin(),rows.end(),[](const Json& a,const Json& b){return a.at("saved_id").get<int32_t>()<b.at("saved_id").get<int32_t>();});
    auto clock=work.clock().at("comparable");
    clock.erase("panel_present");
    return {{"schema","mt2-step-checkpoint-v1"},{"completed_tick",tick},{"snapshot_sha256",snapshot},{"seed",seed},
        {"default_rng",random},{"default_rng_extra",extra},{"clock",clock},{"developers",developers},
        {"cash",cash},{"logged_in_subscribers",logged_in},{"subscribers",subscribers},
        {"finished_tasks",work.finished_tasks().at("comparable")}};
}
}
