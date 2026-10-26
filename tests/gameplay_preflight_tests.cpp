#include "gameplay/dispatch.h"
#include "gameplay/characters/characters.h"
#include "gameplay/dungeons/dungeons.h"
#include "gameplay/management/management.h"
#include "gameplay/regions/regions.h"
#include "gameplay/actors/actors.h"
#include "gameplay/world_tools/world_tools.h"
#include <iostream>

using namespace mt2fw;
using namespace mt2fw::mt2::gameplay;
int main(){try{
    size_t checks=0;
    auto require=[&](bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);};
    const auto& catalog=operations();std::set<std::string> names;
    require(!catalog.empty(),"Gameplay catalog is empty");
    for(const auto& operation:catalog){
        auto name=operation.at("name").get<std::string>();
        require(names.insert(name).second,"Gameplay operation is duplicated");
        Json args=Json::object();
        for(const auto& p:operation.at("parameters"))if(!p.value("optional",false))args[p.at("name").get<std::string>()]=Json();
        validate_operation(name,operation.at("kind").get_ref<const std::string&>().c_str(),args);++checks;
        args["forged_native_address"]=1234;bool rejected=false;
        try{validate_operation(name,operation.at("kind").get_ref<const std::string&>().c_str(),args);}catch(const std::exception& error){rejected=std::string(error.what()).find("unknown field")!=std::string::npos;}
        require(rejected,"Gameplay accepts undeclared argument fields");
        if(operation.at("kind")=="query"){
            Context absent;args.erase("forged_native_address");bool refused=false;
            try{query(absent,name,args);}catch(const std::exception& error){refused=std::string(error.what()).find("GAMEPLAY_THREAD")!=std::string::npos;}
            require(refused,"Gameplay query bypassed its native world/thread boundary");
        }
        if(operation.at("kind")=="action"){
            Context absent;bool attempted=false,confirmed=false;Json result;std::string cause;
            args.erase("forged_native_address");
            try{confirmed=action(absent,name,args,attempted,result,cause);}catch(const std::exception& error){cause=error.what();}
            require(!confirmed&&!attempted&&!cause.empty(),"Gameplay confirms or attempts a mutation without an owned world");
        }
    }
    auto test_boundary=[&](auto handler,const char* operation){
        Context absent;bool attempted=false,confirmed=false;Json result;std::string cause;
        try{confirmed=handler(absent,operation,Json::object(),attempted,result,cause);}catch(const std::exception& error){cause=error.what();}
        require(!confirmed&&!attempted&&!cause.empty(),"Native domain turns its failed precondition into success");
    };
    test_boundary(characters::action,"characters.rename");
    test_boundary(dungeons::action,"dungeons.make_instance");
    test_boundary(management::action,"management.set_purchase_fee");
    test_boundary(regions::action,"regions.set_styles");
    test_boundary(actors::action,"npcs.rename");
    test_boundary(world_tools::action,"network.place_node");
    std::cout<<checks<<" gameplay catalog and native preflight checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
