#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::npc_quest_records {

struct DailyCount {
    std::vector<int32_t> history;
    int32_t today=0;
    int32_t total=0;
    bool operator==(const DailyCount&) const = default;
};

struct Rating {
    float beauty=0;
    float convenience=0;
    float danger=0;
    bool set=false;
    bool operator==(const Rating&) const = default;
};

struct Record {
    int32_t sequence_id=-1;
    int32_t type=0;
    std::string name;
    std::string description;
    int32_t action=0;
    int32_t monsters_to_kill=0;
    int32_t monster_type=0;
    int32_t xp_reward=0;
    int32_t gold_reward=0;
    std::string destination_key;
    DailyCount accepted;
    DailyCount completed;
    Rating rating;
    bool operator==(const Record&) const = default;
};

} // namespace mt2mp::npc_quest_records
