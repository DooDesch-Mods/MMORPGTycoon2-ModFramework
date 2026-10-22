#include "world.h"
#include "mt2mp/game/native_engine.h"
#include <limits>

namespace mt2fw::mt2::gameplay::world {
namespace native=mt2mp::game::native;
std::optional<Json> query(Context& context,const std::string& operation,const Json& args) {
    if(operation=="world.entities"){
        auto limit=args.value("limit",100);
        if(limit<1||limit>128)throw std::runtime_error("World entity query limit must be 1..128");
        std::vector<native::WorldPropState> states;std::string cause;
        if(!native::list_world_props(size_t(limit),states,cause))throw std::runtime_error(cause);
        Json rows=Json::array();
        for(const auto& state:states)rows.push_back({{"handle",{{"epoch",context.epoch},{"uid",std::to_string(state.prop.uid)}}},
            {"x",state.prop.x},{"y",state.prop.y},{"z",state.prop.z},{"region_active",state.region_active},
            {"building",state.building},{"type",state.type},{"variant",state.variant}});
        return rows;
    }
    if(operation=="subscribers.candidates"){
        native::SubscriberCandidates state;std::string cause;
        if(!native::read_subscriber_candidates(state,cause))throw std::runtime_error(cause);
        Json handles=Json::array();for(auto id:state.serialized_ids)handles.push_back({{"epoch",context.epoch},{"id",id}});
        return Json{{"epoch",context.epoch},{"existing_count",state.existing_count},{"scanned_count",state.scanned_count},
            {"eligible_count",state.eligible_count},{"scan_complete",state.scan_complete},{"handles",handles}};
    }
    if(operation=="subscribers.get"){
        const auto& handle=args.at("handle");
        if(!handle.is_object()||!handle.at("epoch").is_number_integer()||handle.at("epoch").get<int64_t>()<0||
           handle.at("epoch").get<uint64_t>()!=context.epoch||!handle.at("id").is_number_integer())
            throw std::runtime_error("Subscriber handle has invalid identity or belongs to another world");
        auto id=handle.at("id").get<int64_t>();
        if(id<0||id>std::numeric_limits<int32_t>::max())throw std::runtime_error("Subscriber ID is outside the native integer range");
        native::SubscriberNeedsState state;std::string cause;
        if(!native::read_subscriber_needs(int32_t(id),state,cause))throw std::runtime_error(cause);
        return Json{{"handle",{{"epoch",context.epoch},{"id",state.serialized_id}}},{"subscriber_count",state.subscriber_count},
            {"satisfaction",state.satisfaction},{"addiction",state.addiction},{"logged_in_seconds",state.logged_in_seconds},
            {"logged_in",state.logged_in},{"needs",state.needs}};
    }
    return std::nullopt;
}
}
