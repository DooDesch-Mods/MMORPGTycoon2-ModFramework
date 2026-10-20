#pragma once
#include "mt2mp/game/native_engine.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace mt2fw::mt2::public_reads {
using Json = nlohmann::json;
namespace native = mt2mp::game::native;

inline float coordinate(const Json& args, const char* field) {
    if (!args.contains(field) || !args.at(field).is_number())
        throw std::runtime_error(std::string("Region query requires finite ") + field);
    const double value = args.at(field).get<double>();
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
        throw std::runtime_error(std::string("Region query requires finite float ") + field);
    return static_cast<float>(value);
}
inline Json building(const native::BuildingState& state, uint64_t epoch) {
    Json result{{"handle",{{"epoch",epoch},{"uid",std::to_string(state.prop.uid)}}},
        {"x",state.prop.x},{"y",state.prop.y},{"z",state.prop.z},
        {"type",state.type},{"variant",state.variant},
        {"rotation",state.rotation},{"scale",state.scale},
        {"in_dungeon",state.dungeon_membership},{"in_dungeon_instance",state.dungeon_instance_membership}};
    if (state.exterior_uid) result["exterior"] = {{"epoch",epoch},{"uid",std::to_string(state.exterior_uid)}};
    return result;
}
inline Json region(const native::RegionState& state, float x, float z, uint64_t epoch) {
    return {{"epoch",epoch},{"x",x},{"z",z},{"type",state.type},{"owned",state.owned},
        {"purchase_price",state.purchase_price},{"cash",state.cash}};
}
inline Json simulation(const native::SimulationState& state, uint64_t epoch) {
    Json record = nullptr;
    if (state.record_present) {
        Json lines = Json::array();
        for (size_t index = 0; index < state.line_counts.size(); ++index)
            lines.push_back({{"category",index},{"count",state.line_counts[index]},{"income",state.line_income[index]}});
        record = {{"day",state.record_day},{"cash",state.record_cash},
            {"income",state.record_income},{"expense",state.record_expense},{"lines",lines}};
    }
    return {{"epoch",epoch},{"cash",state.cash},{"logged_in_subscribers",state.logged_in_subscribers},
        {"clock",{{"paused",state.clock.paused},{"speed",state.clock.time_scale},
            {"day",state.clock.flat_day},{"seconds",state.clock.seconds}}},{"ledger",record}};
}
inline Json software(const native::SoftwareState& state, uint64_t epoch) {
    return {{"epoch",epoch},{"rating",state.rating},{"fun",state.fun}};
}
}
