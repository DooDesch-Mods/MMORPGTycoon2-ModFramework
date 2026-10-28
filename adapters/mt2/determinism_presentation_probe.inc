// Private read-only presentation evidence at an existing quiescent witness.
namespace presentation_probe {
bool enabled=false;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
uintptr_t typed(uintptr_t p,uintptr_t ap){require(p&&read<uintptr_t>(p)==image+ap,"Presentation probe native identity differs");return p;}
uint8_t flag(uintptr_t p){auto value=read<uint8_t>(p);require(value<=1,"Presentation probe native flag differs");return value;}
Json words(uintptr_t p,std::initializer_list<uintptr_t> offsets){
    Json out=Json::object();for(auto offset:offsets)out[std::to_string(offset)]=read<uint32_t>(p+offset);return out;
}
Json snapshot(){
    if(!enabled)return nullptr;
    require(thread==GetCurrentThreadId()&&held_tasks,"Presentation probe requires quiescent game-thread witness");
    auto map=typed(read<uintptr_t>(image+0x13df8c0),0x1742860);
    auto rez=typed(read<uintptr_t>(map+0x2a0),0x171c7b0);
    require(rez==read<uintptr_t>(image+0x13df590),"Presentation probe WorldRez singleton differs");
    auto digital=typed(read<uintptr_t>(owner+0x58),0x1706140);
    auto view=typed(read<uintptr_t>(image+0x13df650),0x1748620);
    auto system=read<uintptr_t>(image+0x1c4eb40);require(system,"Presentation probe System absent");
    auto screen=read<uintptr_t>(system+0x70);require(screen,"Presentation probe Screen absent");
    auto count=read<int32_t>(screen+0x30);require(count>=26&&count<=64,"Presentation probe scene count differs");
    auto list=read<uintptr_t>(screen+0x10);require(list,"Presentation probe scene list absent");
    Json scenes=Json::array();
    for(int i=0;i<26;++i){auto scene=read<uintptr_t>(list+sizeof(uintptr_t)*i);require(scene,"Presentation probe Scene absent");
        scenes.push_back({{"index",i},{"b0",flag(scene+0xb0)},{"b1",flag(scene+0xb1)},{"b2",flag(scene+0xb2)}});}
    return {{"map",{{"state",read<int32_t>(map+0x3d4)},{"ready",flag(map+0x47f)},{"transition_enabled",flag(map+0x480)},
        {"rez_elapsed_bits",read<uint32_t>(map+0x484)},{"rez_active",flag(map+0x488)},
        {"background_elapsed_bits",read<uint32_t>(map+0x48c)},{"background_pending",flag(map+0x490)},
        {"presentation_words",words(map,{0x494,0x498,0x49c})}}},
        {"world_rez",{{"time_bits",read<uint32_t>(rez+8)},{"active",flag(rez+0xc)}}},
        {"digital",{{"visible",flag(digital+0x51)},{"state",read<int32_t>(digital+0x510)},
            {"requested_state",read<int32_t>(digital+0x514)},{"transition_started",flag(digital+0x518)},
            {"logo_shown",flag(digital+0x519)},{"timer_words",words(digital,{0x51c,0x520,0x524})}}},
        {"view",{{"mode",read<int32_t>(view+0x78)},{"fraction_bits",words(view,{0x90,0x94,0x98,0x9c,0xa0})}}},
        {"scene_flags",scenes}};
}
struct Target{float x,y,z,distance;int32_t region;std::string saved_key;};
Target npc_target(const Json& before,const Json& args,float minimum,float maximum){
    require(enabled,"Grounded camera target requires private presentation_probe marker");
    Target target{};target.saved_key=args.at("presentation_npc_key").get<std::string>();
    target.distance=args.at("distance").get<float>();
    require(std::isfinite(target.distance)&&target.distance>0&&target.distance>=minimum&&target.distance<=maximum,"Grounded camera distance outside native bounds");
    size_t matches=0;
    for(const auto& npc:before.at("comparable").at("npcs")){
        if(npc.at("saved_key")!=target.saved_key)continue;
        ++matches;const auto& pose=npc.at("pose_bits");require(pose.is_array()&&pose.size()==10,"Grounded camera NPC pose differs");
        auto value=[&](size_t i){require(pose.at(i).is_number_unsigned()&&pose.at(i).get<uint64_t>()<=UINT32_MAX,"Grounded camera NPC pose bits invalid");return std::bit_cast<float>(pose.at(i).get<uint32_t>());};
        target.x=value(4);target.y=value(5);target.z=value(6);target.region=npc.at("region").get<int32_t>();
    }
    require(matches==1&&target.region>0&&std::isfinite(target.x)&&std::isfinite(target.y)&&std::isfinite(target.z),"Grounded camera requires one current saved NPC with finite pose and Region");
    return target;
}
void install(const Json& config){
    enabled=config.value("presentation_probe",false);if(!enabled)return;
    require(config.value("bootstrap_boundary",false),"Presentation probe requires bootstrap boundary");
    // Exact bodies establish the field layouts; this observer installs no hooks.
    presentation_rng::verify(0x32e890,5264,"5e6f092c80fc48bbb0fedc56b386af43f47040ccf3829989c25d9e97a2852db7");
    presentation_rng::verify(0x322490,288,"7f05128d33541dc1d03ae9ea27c4a6e3da40bb02cdb5c002d06e0da067ddfd1d");
    presentation_rng::verify(0x3225c0,32,"8f361392a5393725d5922aabe260e3e250732ca8cddb3295c5cda719c4939e88");
    presentation_rng::verify(0x16ec90,1680,"643ac19c973724ac97c29a001a686188811f86638284668deb49d986c6dd126b");
    presentation_rng::verify(0x16f4e0,2608,"e2ae82831a421c13459f20498381209beaf2b0620c356110c12efb2fbbf71a07");
    presentation_rng::verify(0x481c10,416,"7fee0c0355ebf9137a279ce78f2bd4821d7b7c90c8cb26e9969aed6fc5839249");
    presentation_rng::verify(0x481db0,656,"265e384e2af25a903cfe58183e33a484e8c6a8aeb9aa3dc239ff3875af5e588f");
    presentation_rng::verify(0x9ca3a0,80,"a18e9111c7c1ea1143c06644859585a09386cc712d80995b295c66f11e559ced");
    presentation_rng::verify(0x9c9070,880,"4cf61560e5223f1a9193b9f1af4685d40caa1bce41ea7511690b529052c1823e");
}
}
