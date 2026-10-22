// Original MIT adapter. Native evidence: dungeon_gizmo_runtime.inc,
// dungeon_gizmo_record.h and typed_action/dungeon_gizmo_action.h.
uint32_t gizmo_state_door(uintptr_t image,uintptr_t target,bool& door) {
    __try{door=reinterpret_cast<bool(*)(uintptr_t)>(image+0x145990)(target);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t gizmo_state_prepare(uintptr_t image,uintptr_t receiver,uintptr_t target) {
    __try{reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x6f53f0)(receiver,target);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
void gizmo_state_window(Context& c,uintptr_t receiver,uintptr_t target) {
    const auto proxy=c.read<uintptr_t>(receiver+0x4f8);
    need(c.read<uintptr_t>(receiver)==c.image+0x1719c80 && c.read<uintptr_t>(receiver+0x4e8)==c.image+0x16f9ee0 &&
        c.read<uintptr_t>(receiver+0x4f0)==target && proxy && c.read<uintptr_t>(target+0x28)==proxy && c.read<uintptr_t>(proxy)==target+0x20 &&
        c.resolve(handle(c,target),0x1749ab0)==target,"Dungeon Gizmo native Window weak/full UID reciprocity differs");
}
Json set_gizmo_state(Context& c,const Owner& o,const Json& args,bool& attempted) {
    const auto members=gizmos(c,o,args);const auto target=object(c,args.at("gizmo"),0x1749ab0);need(std::find(members.begin(),members.end(),target)!=members.end(),"Dungeon State Gizmo is absent from the requested scene");
    uintptr_t shard=0;scene_region(c,o,args,shard);const auto shard_manager=c.singleton(0x13df440);need(shard_manager && c.read<uintptr_t>(shard_manager)==c.image+0x1702b00 && c.read<uintptr_t>(shard_manager+0x60)==shard,"Dungeon Gizmo State requires the requested scene to be the actual native active scene");
    const auto state=integer(args.at("state"),1,3,"native Gizmo State choice");
    const Pin pins[]{
        {0x6f4930,2071,"06943e49c74a354ab951ca86dd1939140ee6677cbb436b660a524954cd557e08"},
        {0x6f53f0,930,"7fb0b30f3d0f041d54bbd13f62a26111ff2afe89dfb3b9730647c955bdbf64de"},
        {0x6f3800,2041,"3401ec064831a83e6e09af76061de2ba6e8b3e2fc89337858886f673baae889d"},
        {0x145990,32,"d4c6711e4238bda8f0c8cb383ba044d2f78378a2d24fdc9894964a6db301b596"},
        {0x1459d0,48,"eb163910e21a65c0082f80b572ebf352c9b2ef33e1259ac74acfd8108d0d76d3"},
        {0x146e70,448,"99d313f2e7e68fd77a3f5fdfdaaa3f1f8b16055c60e2259a4d23358e2a226c68"},
        {0x147030,384,"67c3fba8419dca8070cc7d90854b6ffe51eaaa11de6eb3303aa7d6c5d1dbf053"},
        {0x147200,560,"fd2cfd40d4d3abbbee74452fb298a6ae17d75dff5f8930a37b6d4c6c11928527"},
        {0x147500,96,"fe72e009f4feb5354857af7135d987d70aee08fa2998343b2ca302702c4526ee"},
        {0x147560,272,"08239506163a119c2194e8f1bfe4491eac5eecea80cbd07a33f05ad1c84ee614"},
    };for(auto value:pins)pin(c,value);
    const auto receiver=window(c,"info_gizmo",0x1719c80);need(c.read<uintptr_t>(c.image+0x1719c80+0x168)==c.image+0x6f4930,"Dungeon Gizmo State UICommand virtual method differs");
    const auto before=gizmo_json(c,target);const auto unlocked=c.read<uint32_t>(shard+0x44);bool door=false;seh(gizmo_state_door(c.image,target,door),"Gizmo IsDoor");need(door || state!=2,"Dungeon non-Door Gizmo State permits only trigger=1 or reset=3");
    attempted=true;seh(gizmo_state_prepare(c.image,receiver,target),"Gizmo State Window preparation");gizmo_state_window(c,receiver,target);
    need(gizmo_json(c,target)==before && c.read<uint32_t>(shard+0x44)==unlocked,"Dungeon Gizmo native Window preparation changed gameplay before State command; do not replay");
    auto expected=before;uint32_t wanted_unlocked=unlocked;
    if(door){expected["state"]=state-1;if(state==1){expected["triggered"]=false;wanted_unlocked&=~(uint32_t(1)<<(uint32_t(before.at("lock_id").get<int32_t>())&31));}
        if(state==3 && before.at("immediate")==0){expected["triggered"]=true;wanted_unlocked|=uint32_t(1)<<(uint32_t(before.at("lock_id").get<int32_t>())&31);}}
    else expected["triggered"]=state==1;
    command(c,0x6f4930,receiver,"SetState "+std::to_string(state),attempted);
    gizmo_state_window(c,receiver,target);need(c.read<uintptr_t>(shard_manager+0x60)==shard && gizmos(c,owner(c,args),args)==members,"Dungeon State command changed active scene or Gizmo roster");
    const auto after=gizmo_json(c,object(c,args.at("gizmo"),0x1749ab0));need(after==expected && c.read<uint32_t>(shard+0x44)==wanted_unlocked,"Dungeon native State trigger/state/unlocked readback differs; inspect current native state before retry");return after;
}
