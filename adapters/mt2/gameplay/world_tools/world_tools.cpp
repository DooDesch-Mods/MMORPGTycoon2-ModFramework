#include "world_tools.h"
#include "common.h"
namespace mt2fw::mt2::gameplay::world_tools {
namespace {
using namespace detail;
#include "development.inc"
#include "network.inc"
}
std::optional<Json> query(Context& c,const std::string& op,const Json& args){
    if(op.starts_with("development.")||op.starts_with("projects."))return development_query(c,op,args);
    if(op.starts_with("network."))return network_query(c,op,args);
    return std::nullopt;
}
bool action(Context& c,const std::string& op,const Json& args,bool& attempted,Json& result,std::string& cause){
    c.require_world();if(op.starts_with("development.")||op.starts_with("projects."))return development_action(c,op,args,attempted,result,cause);
    if(op.starts_with("network."))return network_action(c,op,args,attempted,result,cause);
    throw std::runtime_error("Unsupported world tools action: "+op);
}
}
