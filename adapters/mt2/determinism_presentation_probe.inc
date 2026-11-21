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
    auto map=typed(read<uintptr_t>(image+0x13eb8c0),0x1748c00);
    auto rez=typed(read<uintptr_t>(map+0x2a0),0x1722b50);
    require(rez==read<uintptr_t>(image+0x13eb590),"Presentation probe WorldRez singleton differs");
    auto digital=typed(read<uintptr_t>(owner+0x58),0x170c4e0);
    auto view=typed(read<uintptr_t>(image+0x13eb650),0x174e9c0);
    auto system=read<uintptr_t>(image+0x1c55b40);require(system,"Presentation probe System absent");
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
    presentation_rng::verify(0x332920,5264,"55ca106d58de5c32fdc1fc661706a1ecf3662ef4144040141475375e8dbb43c4");
    presentation_rng::verify(0x326230,304,"ab4e68b58f78612098adb424b91be006e23b6c128c151bbfb54c61b77652c688");
    presentation_rng::verify(0x326370,32,"371cc002ea95f63763c4ffb315190eed21f17c525eb2429c60dad8dbb7df8663");
    presentation_rng::verify(0x170660,1680,"239d75e38efbe874ebfae1b99a88a14cdf57551b3986ecb0214a1a768574cded");
    presentation_rng::verify(0x170eb0,2608,"01960afef08d26d400395426e7a2a96690d57145f83ffec40edc1f82b7bc0db5");
    presentation_rng::verify(0x486d70,448,"720e1591a5983c671bb7798dee3dc3425ed5d2061dfe066147ac9c90ef3d6867");
    presentation_rng::verify(0x486f30,720,"4b1a782291893f161b214ddec124b56901c22a7fcd6d3bd50dda5f47df81bf07");
    presentation_rng::verify(0x9d3f60,80,"35b5eea29601158eebeb53137155f4b564a454e8231884781e246825df535a03");
    presentation_rng::verify(0x9d2c30,880,"67927fcbd1b078e2a29d402a4af4c6096d592c2dc3029d17bb2be45854a48ae0");
}
}
