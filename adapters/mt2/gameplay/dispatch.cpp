#include "dispatch.h"
#include "dungeons/dungeons.h"
#include "characters/characters.h"
#include "management/management.h"
#include "world/world.h"
#include "regions/regions.h"
#include "actors/actors.h"
#include "world_tools/world_tools.h"
#include "modular/modular.h"
#include "scenery/scenery.h"
#include "paths/paths.h"
#include "region_type/region_type.h"

namespace mt2fw::mt2::gameplay {
const Json& operations() {
    static const Json value=Json::parse(
#include "operations.inc"
    );
    return value;
}
void validate_operation(const std::string& operation,const char* kind,const Json& args) {
    if(!args.is_object()||args.dump().size()>65536)throw std::runtime_error("GAMEPLAY_ARGUMENTS: expected an object up to 65536 bytes");
    const auto& catalog=operations();
    auto found=std::find_if(catalog.begin(),catalog.end(),[&](const Json& row){return row.at("name")==operation;});
    if(found==catalog.end()||found->at("kind")!=kind)throw std::runtime_error("GAMEPLAY_OPERATION: unsupported "+std::string(kind)+" "+operation);
    std::set<std::string> allowed;
    for(const auto& parameter:found->at("parameters")){
        const auto name=parameter.at("name").get<std::string>();allowed.insert(name);
        if(!parameter.value("optional",false)&&!args.contains(name))throw std::runtime_error("GAMEPLAY_ARGUMENTS: missing "+name);
    }
    for(const auto& [name,value]:args.items())if(!allowed.contains(name))throw std::runtime_error("GAMEPLAY_ARGUMENTS: unknown field "+name);
}
Json query(Context& context,const std::string& operation,const Json& args) {
    validate_operation(operation,"query",args);context.require_world();
    for(auto handler:{dungeons::query,characters::query,management::query,world::query,regions::query,actors::query,world_tools::query,modular::query,scenery::query,paths::query})if(auto value=handler(context,operation,args))return *value;
    throw std::runtime_error("GAMEPLAY_OPERATION: no native query handler for "+operation);
}
bool action(Context& context,const std::string& operation,const Json& args,bool& attempted,Json& result,std::string& cause) {
    validate_operation(operation,"action",args);context.require_mutation();
    const auto group=operation.substr(0,operation.find('.'));
    if(group=="dungeons")return dungeons::action(context,operation,args,attempted,result,cause);
    if(group=="characters")return characters::action(context,operation,args,attempted,result,cause);
    if(group=="management"||group=="economy")return management::action(context,operation,args,attempted,result,cause);
    if(operation=="regions.set_type")return region_type::action(context,operation,args,attempted,result,cause);
    if(group=="regions"||group=="terrain")return regions::action(context,operation,args,attempted,result,cause);
    if(group=="actors"||group=="npcs"||group=="monsters"||group=="quests")return actors::action(context,operation,args,attempted,result,cause);
    if(group=="network"||group=="development"||group=="projects")return world_tools::action(context,operation,args,attempted,result,cause);
    if(group=="modular")return modular::action(context,operation,args,attempted,result,cause);
    if(group=="scenery")return scenery::action(context,operation,args,attempted,result,cause);
    if(group=="paths"||group=="walls")return paths::action(context,operation,args,attempted,result,cause);
    throw std::runtime_error("GAMEPLAY_OPERATION: no native action handler for "+operation);
}
}
