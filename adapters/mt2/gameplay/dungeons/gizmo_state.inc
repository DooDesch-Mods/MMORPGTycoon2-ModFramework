// Original MIT adapter. Native evidence: dungeon_gizmo_runtime.inc,
// dungeon_gizmo_record.h and typed_action/dungeon_gizmo_action.h.
uint32_t gizmo_state_door(uintptr_t image,uintptr_t target,bool& door) {
    __try{door=reinterpret_cast<bool(*)(uintptr_t)>(image+0x1469c0)(target);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t gizmo_state_prepare(uintptr_t image,uintptr_t receiver,uintptr_t target) {
    __try{reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x6fc880)(receiver,target);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
void gizmo_state_window(Context& c,uintptr_t receiver,uintptr_t target) {
    const auto proxy=c.read<uintptr_t>(receiver+0x4f8);
    need(c.read<uintptr_t>(receiver)==c.image+0x1720020 && c.read<uintptr_t>(receiver+0x4e8)==c.image+0x1700280 &&
        c.read<uintptr_t>(receiver+0x4f0)==target && proxy && c.read<uintptr_t>(target+0x28)==proxy && c.read<uintptr_t>(proxy)==target+0x20 &&
        c.resolve(handle(c,target),0x174fe50)==target,"Dungeon Gizmo native Window weak/full UID reciprocity differs");
}
Json set_gizmo_state(Context& c,const Owner& o,const Json& args,bool& attempted) {
    const auto members=gizmos(c,o,args);const auto target=object(c,args.at("gizmo"),0x174fe50);need(std::find(members.begin(),members.end(),target)!=members.end(),"Dungeon State Gizmo is absent from the requested scene");
    uintptr_t shard=0;scene_region(c,o,args,shard);const auto shard_manager=c.singleton(0x13eb440);need(shard_manager && c.read<uintptr_t>(shard_manager)==c.image+0x1708ea0 && c.read<uintptr_t>(shard_manager+0x60)==shard,"Dungeon Gizmo State requires the requested scene to be the actual native active scene");
    const auto state=integer(args.at("state"),1,3,"native Gizmo State choice");
    const Pin pins[]{
        {0x6fbdc0,2071,"4e9daac31bcc21baeccdaa86d22024d99e36a4b19739d99d7424ec55e5c601ba"},
        {0x6fc880,930,"a8a44d16ebf40fe7bb1db28c93e1d70a0b299c32e32e76262c544f5980b9e4a6"},
        {0x6fac90,2041,"f16bf84505d322516d4e92a0ac455516e2fdd626769a9ee17361efa5984d650a"},
        {0x1469c0,32,"d4c6711e4238bda8f0c8cb383ba044d2f78378a2d24fdc9894964a6db301b596"},
        {0x146a00,48,"eb163910e21a65c0082f80b572ebf352c9b2ef33e1259ac74acfd8108d0d76d3"},
        {0x147ea0,448,"b4d74c51e9e488278e887271acf957c11526329b21a7cf718d706dd6a4c98045"},
        {0x148060,384,"e3585d2ae44aa134d700b25abb3dc5ed9baaef4adc09109f2c7911c38f6badba"},
        {0x148230,560,"9d26b462e3f6e6074a7e168dba851033a1335ad206550404a1c7d08e064a9bb8"},
        {0x148530,96,"e1c8320b86071dd29142b0c1a53d66ed6b89490ad9762a507b8526ff58c557ff"},
        {0x148590,272,"4269aaf9fcba88c8d242c2270f000b68f26b7d15c79578cf0141451ece874ffb"},
    };for(auto value:pins)pin(c,value);
    const auto receiver=window(c,"info_gizmo",0x1720020);need(c.read<uintptr_t>(c.image+0x1720020+0x168)==c.image+0x6fbdc0,"Dungeon Gizmo State UICommand virtual method differs");
    const auto before=gizmo_json(c,target);const auto unlocked=c.read<uint32_t>(shard+0x44);bool door=false;seh(gizmo_state_door(c.image,target,door),"Gizmo IsDoor");need(door || state!=2,"Dungeon non-Door Gizmo State permits only trigger=1 or reset=3");
    attempted=true;seh(gizmo_state_prepare(c.image,receiver,target),"Gizmo State Window preparation");gizmo_state_window(c,receiver,target);
    need(gizmo_json(c,target)==before && c.read<uint32_t>(shard+0x44)==unlocked,"Dungeon Gizmo native Window preparation changed gameplay before State command; do not replay");
    auto expected=before;uint32_t wanted_unlocked=unlocked;
    if(door){expected["state"]=state-1;if(state==1){expected["triggered"]=false;wanted_unlocked&=~(uint32_t(1)<<(uint32_t(before.at("lock_id").get<int32_t>())&31));}
        if(state==3 && before.at("immediate")==0){expected["triggered"]=true;wanted_unlocked|=uint32_t(1)<<(uint32_t(before.at("lock_id").get<int32_t>())&31);}}
    else expected["triggered"]=state==1;
    command(c,0x6fbdc0,receiver,"SetState "+std::to_string(state),attempted);
    gizmo_state_window(c,receiver,target);need(c.read<uintptr_t>(shard_manager+0x60)==shard && gizmos(c,owner(c,args),args)==members,"Dungeon State command changed active scene or Gizmo roster");
    const auto after=gizmo_json(c,object(c,args.at("gizmo"),0x174fe50));need(after==expected && c.read<uint32_t>(shard+0x44)==wanted_unlocked,"Dungeon native State trigger/state/unlocked readback differs; inspect current native state before retry");return after;
}
