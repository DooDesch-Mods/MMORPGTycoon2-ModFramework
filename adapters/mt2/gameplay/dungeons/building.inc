// Original MIT adapter. Native evidence: placement_kiss/native_build.cpp,
// native_family.cpp, effect_cleanup.h and dungeon_build_tasks.inc.
struct BuildingDraft {uintptr_t receiver=0;uint32_t stage=0,type_cleanup=0,variant_cleanup=0;bool ready=false;};
uint32_t building_definition_native(uintptr_t image,uintptr_t manager,const Text* type,uintptr_t& definition) {
    __try{definition=reinterpret_cast<uintptr_t(*)(uintptr_t,const Text&)>(image+0x251500)(manager,*type);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t building_prepare_native(uintptr_t image,uintptr_t shard,const Text* type,const Text* variant,const std::vector<Text>* variants,const Pose* pose,BuildingDraft& out) {
    alignas(8) unsigned char type_owned[32]{},variant_owned[32]{};bool type_ready=false,variant_ready=false;uintptr_t blueprint=0,prop=0;int32_t selected=-1;uint32_t code=0;
    __try {
        out.stage=1;reinterpret_cast<void(*)(void*,const char*)>(image+0x49e090)(type_owned,type->data);type_ready=true;
        out.stage=2;reinterpret_cast<void(*)(void*,const char*)>(image+0x49e090)(variant_owned,variant->data);variant_ready=true;
        out.stage=3;out.receiver=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x770);
        if(!out.receiver)code=ERROR_NOT_ENOUGH_MEMORY;
        else {
            out.stage=4;reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0x5949f0)(out.receiver,type_owned);
            out.stage=5;reinterpret_cast<void(*)(uintptr_t)>(image+0xc4b230)(out.receiver+0x188);
            for(size_t i=0;i<variants->size();++i){const Text row[2]{*type,(*variants)[i]};reinterpret_cast<void(*)(uintptr_t,const void*)>(image+0xc4b240)(out.receiver+0x188,row);}
            out.stage=19;reinterpret_cast<void(*)(uintptr_t)>(image+0x592450)(out.receiver);
            out.stage=20;const auto data=*reinterpret_cast<const uintptr_t*>(out.receiver+0x1a8);const auto count=*reinterpret_cast<const int32_t*>(out.receiver+0x1b0);
            if(!data || count<1 || count>100000)code=ERROR_INVALID_DATA;
            else for(int32_t i=0;i<count;++i){const auto* row=reinterpret_cast<const Text*>(data+size_t(i)*64);
                if(row[0].size==type->size && row[1].size==variant->size && !std::memcmp(row[0].data,type->data,size_t(type->size)) && !std::memcmp(row[1].data,variant->data,size_t(variant->size))){if(selected!=-1){code=ERROR_DUP_NAME;break;}selected=i;}}
            if(selected<0 && !code)code=ERROR_NOT_FOUND;
            if(!code){
                out.stage=6;blueprint=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x260);
                if(!blueprint)code=ERROR_NOT_ENOUGH_MEMORY;
                else {
                    *reinterpret_cast<uintptr_t*>(out.receiver+0x6c0)=blueprint;
                    out.stage=7;reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x400200)(blueprint,shard);
                    out.stage=8;reinterpret_cast<void(*)(uintptr_t,const void*,const void*,bool,bool)>(image+0x400ad0)(blueprint,type_owned,variant_owned,false,true);
                    out.stage=9;prop=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x220);
                    if(!prop)code=ERROR_NOT_ENOUGH_MEMORY;
                    else {
                        *reinterpret_cast<uintptr_t*>(out.receiver+0x6b8)=prop;
                        out.stage=10;reinterpret_cast<void(*)(uintptr_t)>(image+0x4cacc0)(prop);
                        out.stage=11;reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x4cad10)(prop,blueprint);
                        out.stage=12;reinterpret_cast<void(*)(uintptr_t,const void*,const void*,const void*)>(image+0x9ff370)(prop+0x100,pose->rotation.data(),pose->position.data(),pose->scale.data());
                        if(*reinterpret_cast<const uintptr_t*>(out.receiver+0x3a8) || *reinterpret_cast<const uintptr_t*>(out.receiver+0x3b0))code=ERROR_INVALID_STATE;
                        else {
                            *reinterpret_cast<int32_t*>(out.receiver+0x6cc)=selected;*reinterpret_cast<uint8_t*>(out.receiver+0x6d0)=selected?1:0;
                            *reinterpret_cast<uint8_t*>(out.receiver+0x660)=0;*reinterpret_cast<uint8_t*>(out.receiver+0x661)=1;*reinterpret_cast<uint8_t*>(out.receiver+0x662)=0;
                            out.stage=15;out.ready=true;
                        }
                    }
                }
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(variant_ready){__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(variant_owned);}__except(EXCEPTION_EXECUTE_HANDLER){out.variant_cleanup=GetExceptionCode();}}
    if(type_ready){__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(type_owned);}__except(EXCEPTION_EXECUTE_HANDLER){out.type_cleanup=GetExceptionCode();}}return code;
}
uint32_t building_update_native(uintptr_t image,BuildingDraft& draft) {
    __try{draft.stage=16;reinterpret_cast<void(*)(uintptr_t,float)>(image+0x593cc0)(draft.receiver,0.0f);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t building_unary(uintptr_t image,uintptr_t rva,uintptr_t receiver) {
    __try{reinterpret_cast<void(*)(uintptr_t)>(image+rva)(receiver);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t effect_manager_native(uintptr_t image,uintptr_t shard,uintptr_t& manager) {
    __try{manager=reinterpret_cast<uintptr_t(*)(uintptr_t)>(image+0x5086a0)(shard);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t effect_remove_native(uintptr_t image,uintptr_t manager,uintptr_t instance) {
    __try{reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x188230)(manager,instance);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t effect_clear_native(uintptr_t receiver,uintptr_t expected,bool& exchanged) {
    __try{exchanged=uintptr_t(_InterlockedCompareExchange64(reinterpret_cast<volatile long long*>(receiver+0x6e0),0,static_cast<long long>(expected)))==expected;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
std::vector<uintptr_t> effect_rows(Context& c,uintptr_t manager) {
    const auto data=c.read<uintptr_t>(manager+0x68);const auto count=c.read<int32_t>(manager+0x70),capacity=c.read<int32_t>(manager+0x74);need(count>=0 && count<=1048576 && capacity>=count && (!count || data),"Dungeon Build preview effect manager bounds differ");
    std::vector<uintptr_t> out;for(int32_t i=0;i<count;++i)out.push_back(c.read<uintptr_t>(data+size_t(i)*8));return out;
}
void building_dispose(Context& c,uintptr_t shard,BuildingDraft& draft) {
    need(draft.ready && (draft.stage==15 || draft.stage==16),"Dungeon Build partial native draft retained without destructor replay");
    const auto effect=c.read<uintptr_t>(draft.receiver+0x6e0);
    if(effect){need(c.read<uintptr_t>(effect+0x10)==shard,"Dungeon Build preview effect belongs to another Shard");uintptr_t manager=0;seh(effect_manager_native(c.image,shard,manager),"Build effect manager");need(manager && c.read<uintptr_t>(effect+8)==manager,"Dungeon Build preview effect has a foreign manager");
        auto expected=effect_rows(c,manager);need(std::count(expected.begin(),expected.end(),effect)==1,"Dungeon Build preview effect membership is not unique");const auto index=size_t(std::find(expected.begin(),expected.end(),effect)-expected.begin());expected[index]=expected.back();expected.pop_back();
        seh(effect_remove_native(c.image,manager,effect),"Build preview effect removal");need(effect_rows(c,manager)==expected,"Dungeon Build preview effect removal readback differs");bool exchanged=false;seh(effect_clear_native(draft.receiver,effect,exchanged),"Build preview effect slot clear");need(exchanged && !c.read<uintptr_t>(draft.receiver+0x6e0),"Dungeon Build preview effect slot changed before clear");
    }
    draft.stage=17;seh(building_unary(c.image,0x5950c0,draft.receiver),"private Build destructor");draft.receiver=0;draft.stage=18;
}
uint32_t building_cast_native(uintptr_t image,uintptr_t input,uintptr_t& result) {
    __try{result=reinterpret_cast<uintptr_t(*)(uintptr_t,uintptr_t,uintptr_t,ptrdiff_t)>(image+0x12654f0)(input,image+0x1673900,image+0x1655090,0);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
Json place_building(Context& c,const Owner& owner,const Json& args,bool& attempted) {
    need(!args.contains("instance") && instances(c,owner).empty(),"Dungeon Building placement requires the editable template with zero Instances");
    const auto shard_manager=c.singleton(0x13df440);need(shard_manager && c.read<uintptr_t>(shard_manager)==c.image+0x1702b00 && c.read<uintptr_t>(shard_manager+0x60)==owner.shard,"Dungeon Building placement requires the requested template to be the actual native active scene");
    const auto type=args.at("type").get<std::string>(),variant=args.at("variant").get<std::string>();need(!type.empty() && !variant.empty() && type.size()<=256 && variant.size()<=256 && type.find('\0')==std::string::npos && variant.find('\0')==std::string::npos,"Dungeon Building type/variant keys are invalid");
    Pose pose;pose.position=numbers<3>(args.at("position"),"Building position");if(args.contains("rotation"))pose.rotation=numbers<4>(args.at("rotation"),"Building rotation");if(args.contains("scale"))pose.scale=numbers<3>(args.at("scale"),"Building scale");float norm=0;for(float x:pose.rotation)norm+=x*x;need(std::abs(norm-1)<0.002f,"Dungeon Building quaternion must be normalized");for(float x:pose.scale)need(x>0,"Dungeon Building scale must be positive");
    for(uintptr_t rva:{0x251500,0x49e090,0xa02670,0x5949f0,0xc4b230,0xc4b240,0x592450,0x400200,0x400ad0,0x4cacc0,0x4cad10,0x9ff370,0x11bd1e0,0x593cc0,0x5086a0,0x188230,0x5950c0,0x526d50,0x12654f0,0x5089b0})c.verify_original(rva);
    const Text type_key{type.data(),uint64_t(type.size())},variant_key{variant.data(),uint64_t(variant.size())};uintptr_t definition=0;seh(building_definition_native(c.image,c.singleton(0x13df780),&type_key,definition),"Building definition lookup");
    need(definition && c.read<uintptr_t>(definition)==c.image+0x172ae50 && c.text(definition+0x18)==type,"Dungeon Building definition class/type differs");const auto flags=c.read<uint32_t>(definition+0x16c);
    need(type!="Scenery" && type!="DungeonExterior" && !(flags&(1u<<13)) && !(flags&(1u<<16)) && bool(flags&(1u<<17))==(type=="DungeonPortal"),"Dungeon Building definition uses an incompatible native feature family");
    need(bool(flags&(1u<<2))==(type=="StartingPoint"),"Dungeon Building StartingPoint native feature differs");std::vector<std::string> names;std::set<std::string> unique;
    for(auto p:array(c,definition+0x170)){need(c.read<uintptr_t>(p)==c.image+0x1718550,"Dungeon Building variant class differs");auto name=c.text(p+0x18);need(unique.insert(name).second,"Dungeon Building variant identifier is ambiguous");names.push_back(std::move(name));}
    need(unique.contains(variant),"Dungeon Building requested variant is absent from the native definition");std::vector<Text> variants;for(const auto& name:names)variants.push_back({name.data(),uint64_t(name.size())});
    uintptr_t destination=0;seh(region_native(c.image,owner.shard,&pose,destination),"Building destination RegionAt");need(destination==owner.region,"Dungeon Building position leaves the active template Region");need(!c.read<uintptr_t>(c.image+0x1a3aa80),"Dungeon Building placement requires idle native Undo");
    const auto manager=c.read<uintptr_t>(owner.region+0x950),undo=c.singleton(0x13df380),tasks=c.singleton(0x13df340);need(manager && c.read<uintptr_t>(manager)==c.image+0x17183c0 && c.read<uintptr_t>(manager+0x450)==owner.shard && c.read<uintptr_t>(manager+0x458)==owner.region,"Dungeon Building manager reciprocity differs");need(tasks && c.read<uintptr_t>(tasks)==c.image+0x16fdaf0,"Dungeon Building TaskManager class differs");
    const auto before=array(c,manager+0x408),undo_before=undo_stack(c,undo);BuildingDraft draft;std::string cause;Json result;
    attempted=true;const auto prepared=building_prepare_native(c.image,owner.shard,&type_key,&variant_key,&variants,&pose,draft);
    try {
        need(!prepared && draft.ready && !draft.type_cleanup && !draft.variant_cleanup,"Dungeon native Build preparation failed SEH="+std::to_string(prepared)+" stage="+std::to_string(draft.stage)+" type_cleanup="+std::to_string(draft.type_cleanup)+" variant_cleanup="+std::to_string(draft.variant_cleanup));
        need(c.read<uintptr_t>(shard_manager+0x60)==owner.shard,"Dungeon active template changed before Build Update");seh(building_update_native(c.image,draft),"private Build Update");
        seh(building_unary(c.image,0x526d50,tasks),"Building maker FinishAllTasks");need(c.singleton(0x13df340)==tasks,"Dungeon Build TaskManager owner changed");for(size_t offset:{0x30,0x48,0x60,0x78,0x90,0xa8})need(!c.read<int32_t>(tasks+offset),"Dungeon Build native task drain incomplete offset="+std::to_string(offset));
        const auto after=array(c,manager+0x408);std::vector<uintptr_t> added;for(auto p:after)if(std::find(before.begin(),before.end(),p)==before.end())added.push_back(p);need(added.size()==1 && after.size()==before.size()+1,"Dungeon original Build did not add exactly one native Building");
        const auto p=added[0];uintptr_t typed=0;seh(building_cast_native(c.image,p,typed),"new Building RTTI");need(typed==p && c.resolve(handle(c,p))==p && c.read<uintptr_t>(p+0x170)==owner.shard && c.read<uintptr_t>(p+0x178)==owner.region,"Dungeon Build created a foreign class/full UID/scene member");
        need(c.text(p+0x768)==type && c.text(p+0x788)==variant && c.read<std::array<float,3>>(p+0x110)==pose.position && c.read<std::array<float,4>>(p+0x100)==pose.rotation && c.read<std::array<float,3>>(p+0x11c)==pose.scale,"Dungeon Build created authored type/variant/pose readback differs");committed_graph(c,undo,undo_before);
        result={{"handle",handle(c,p)},{"type",type},{"variant",variant},{"position",pose.position},{"rotation",pose.rotation},{"scale",pose.scale}};
    }catch(const std::exception& e){cause=e.what();}
    if(draft.ready){try{need(c.read<uintptr_t>(shard_manager+0x60)==owner.shard,"Dungeon active scene changed before private Build cleanup");building_dispose(c,owner.shard,draft);}catch(const std::exception& e){cause+=(cause.empty()?"":"; ")+std::string("cleanup: ")+e.what();}}
    else if(draft.receiver)cause+="; partial native Build owner retained without destructor replay";
    need(cause.empty(),cause);return result;
}

Json building_json(Context& c,uintptr_t p) {
    return {{"handle",handle(c,p)},{"type",c.text(p+0x768)},{"variant",c.text(p+0x788)},
        {"position",c.read<std::array<float,3>>(p+0x110)},{"rotation",c.read<std::array<float,4>>(p+0x100)},{"scale",c.read<std::array<float,3>>(p+0x11c)}};
}
std::vector<uintptr_t> building_rows(Context& c,uintptr_t shard,uintptr_t region) {
    const auto manager=c.read<uintptr_t>(region+0x950);
    need(manager && c.read<uintptr_t>(manager)==c.image+0x17183c0 && c.read<uintptr_t>(manager+0x450)==shard && c.read<uintptr_t>(manager+0x458)==region,"Dungeon Building manager reciprocal scene differs");
    auto rows=array(c,manager+0x408);for(auto p:rows)need(object(c,handle(c,p),0x16d93c0)==p && c.read<uintptr_t>(p+0x170)==shard && c.read<uintptr_t>(p+0x178)==region,"Dungeon Building full UID/class/scene differs");return rows;
}
uint32_t building_capability(uintptr_t image,uintptr_t target,bool duplicate,bool& allowed,int32_t& cost) {
    __try{allowed=reinterpret_cast<bool(*)(uintptr_t)>(image+(duplicate?0xd314b0:0xd31530))(target);
        if(duplicate)cost=reinterpret_cast<int32_t(*)(uintptr_t)>(image+0xd314f0)(target);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t building_afford(uintptr_t image,uintptr_t business,int32_t price,bool& allowed) {
    __try{allowed=reinterpret_cast<bool(*)(uintptr_t,int32_t)>(image+0x54d00)(business,price);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct BuildingDuplicate : Placement {uint32_t key_cleanup=0;};
uint32_t building_duplicate_native(uintptr_t image,uintptr_t business,uintptr_t undo,uintptr_t source,uintptr_t selection,uintptr_t release,const Pose* pose,int32_t price,BuildingDuplicate& r) {
    alignas(16) unsigned char transform[0x70]{},name[0x20]{},label_key[0x20]{},label[0x38]{};uintptr_t inverse=0;bool name_ready=false,key_ready=false,label_ready=false;uint32_t code=0;
    __try {
        r.purchased=price<=0 || reinterpret_cast<bool(*)(uintptr_t,int32_t,int32_t)>(image+0x54d80)(business,price,6);
        if(!r.purchased)code=ERROR_ACCESS_DENIED;
        else {
            if(reinterpret_cast<bool(*)()>(image+0x264c30)()){
                reinterpret_cast<void(*)(void*,const char*)>(image+0x5c04b0)(name,"Duplicating collection");name_ready=true;
                reinterpret_cast<void(*)(const void*)>(image+0x265300)(name);r.edit_open=true;
            }
            r.graph=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x110);
            if(!r.graph)code=ERROR_NOT_ENOUGH_MEMORY;
            else {
                reinterpret_cast<void(*)(uintptr_t)>(image+0x5400f0)(r.graph);inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xc8);
                if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
                else {
                    reinterpret_cast<void(*)(uintptr_t,int32_t,const void*)>(image+0x53ccb0)(inverse,price,pose->position.data());reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x540760)(r.graph,inverse);
                    reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(image+0x9ff370)(transform,pose->rotation.data(),pose->position.data(),pose->scale.data());
                    r.object=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*)>(image+0x411340)(source,transform);
                    if(!r.object)code=ERROR_INVALID_HANDLE;
                    else {inverse=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xc0);if(!inverse)code=ERROR_NOT_ENOUGH_MEMORY;
                        else {reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x53dc30)(inverse,r.object);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x540760)(r.graph,inverse);
                            const NativeArray rows{image+0x1748c50,&r.object,1,1};
                            reinterpret_cast<void(*)(uintptr_t,const NativeArray*)>(image+0x2d5910)(selection,&rows);
                            reinterpret_cast<void(*)(uintptr_t,int32_t,int32_t)>(image+0x4ee5c0)(release+0x108,1,1);
                            reinterpret_cast<void(*)(void*,const char*)>(image+0x5c04b0)(label_key,"{undo_msg_duplicate}");key_ready=true;
                            reinterpret_cast<void(*)(void*,const void*,const NativeArray*)>(image+0x539230)(label,label_key,&rows);label_ready=true;
                            reinterpret_cast<void(*)(uintptr_t,const void*,uintptr_t)>(image+0x53b260)(undo,label,r.graph);r.committed=true;}}
                }
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(label_ready){
        __try{reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}
    }
    if(key_ready){__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label_key);}__except(EXCEPTION_EXECUTE_HANDLER){r.key_cleanup=GetExceptionCode();}}
    if(name_ready){__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(name);}__except(EXCEPTION_EXECUTE_HANDLER){r.begin_cleanup=GetExceptionCode();}}
    if(r.edit_open){__try{reinterpret_cast<void(*)()>(image+0x265650)();}__except(EXCEPTION_EXECUTE_HANDLER){r.end_cleanup=GetExceptionCode();}}return code;
}
Json control_building(Context& c,const Owner& o,const std::string& operation,const Json& args,bool& attempted) {
    need(!args.contains("instance") && instances(c,o).empty(),"Dungeon Building control requires the editable template with zero Instances");
    const auto shard_manager=c.singleton(0x13df440);need(shard_manager && c.read<uintptr_t>(shard_manager)==c.image+0x1702b00 && c.read<uintptr_t>(shard_manager+0x60)==o.shard,"Dungeon Building control requires the requested template to be the actual native active scene");
    const auto rows=building_rows(c,o.shard,o.region);const auto target=object(c,args.at("building"),0x16d93c0);need(std::find(rows.begin(),rows.end(),target)!=rows.end(),"Dungeon Building is absent from the requested template");
    const auto before=building_json(c,target);const auto definition=c.read<uintptr_t>(target+0x390);need(definition && c.read<uintptr_t>(definition)==c.image+0x172ae50 && c.text(definition+0x18)==before.at("type").get<std::string>(),"Dungeon Building definition class/type differs");
    const auto flags=c.read<uint32_t>(definition+0x16c);need(!(flags&((1u<<13)|(1u<<16))) && bool(flags&(1u<<17))==(before.at("type")=="DungeonPortal"),"Dungeon Building contains an incompatible native feature family");
    need(!c.read<uintptr_t>(c.image+0x1a3aa80),"Dungeon Building control requires idle native Undo");const auto undo=c.singleton(0x13df380),selection_manager=c.singleton(0x13df630);const auto undo_before=undo_stack(c,undo);selection(c,selection_manager);
    Pose pose;pose.position=before.at("position").get<std::array<float,3>>();pose.rotation=before.at("rotation").get<std::array<float,4>>();pose.scale=before.at("scale").get<std::array<float,3>>();
    const bool destroying=operation=="dungeons.destroy_building",duplicating=operation=="dungeons.duplicate_building";
    if(!destroying){pose.position=numbers<3>(args.at("position"),"Building position");if(args.contains("rotation"))pose.rotation=numbers<4>(args.at("rotation"),"Building rotation");if(args.contains("scale"))pose.scale=numbers<3>(args.at("scale"),"Building scale");float norm=0;for(float x:pose.rotation)norm+=x*x;need(std::abs(norm-1)<0.002f,"Dungeon Building quaternion must be normalized");for(float x:pose.scale)need(x>0,"Dungeon Building scale must be positive");
        c.verify_original(0x5089b0);uintptr_t region=0;seh(region_native(c.image,o.shard,&pose,region),"Building destination RegionAt");need(region==o.region,"Dungeon Building destination leaves the requested template Region");}
    uintptr_t graph=0,created=0;
    if(destroying){
        pin(c,{0x2d42c0,2768,"c526304f6ce4e9014154daf39a936bae914f448e7788f80d816b52615fb68e8f"});const NativeArray members{c.image+0x1748c50,&target,1,1};attempted=true;seh(selection_native(c.image,selection_manager,&members,true),"Building DestroyProps");
    }else if(duplicating){
        need(c.read<uintptr_t>(c.image+0x16d93c0+0xd8)==c.image+0xd314b0 && c.read<uintptr_t>(c.image+0x16d93c0+0xe8)==c.image+0x411340 && c.read<uintptr_t>(c.image+0x16d93c0+0x278)==c.image+0xd314f0,"Dungeon Building Duplicate virtual methods differ");
        for(uintptr_t rva:{0xd314b0,0xd314f0,0x411340,0x54d00,0x54d80,0x264c30,0x5c04b0,0x265300,0xa02670,0x5400f0,0x53ccb0,0x540760,0x9ff370,0x53dc30,0x2d5910,0x4ee5c0,0x539230,0x53b260,0x11b2b00,0x11bd1e0,0x265650})c.verify_original(rva);
        const auto business=c.singleton(0x13df220),release_manager=c.singleton(0x13df4f0);need(business && release_manager && c.read<uintptr_t>(release_manager)==c.image+0x1714ed0,"Dungeon Building Duplicate Business/ReleaseManager is absent");const auto release=c.read<uintptr_t>(release_manager+0x38);need(release && c.read<uintptr_t>(release)==c.image+0x16c12b0,"Dungeon Building Duplicate current Release class differs");const auto edits=c.read<int32_t>(release+0x124);need(edits>=0 && edits<INT32_MAX,"Dungeon Building Duplicate edit counter overflows");
        bool allowed=false,affordable=false;int32_t price=-1;seh(building_capability(c.image,target,true,allowed,price),"Building CanDuplicate/Cost");need(allowed && price>=0,"Dungeon Building native CanDuplicate/Cost rejected target");seh(building_afford(c.image,business,price,affordable),"Building duplicate CanAfford");need(affordable,"Dungeon Building Duplicate cannot afford native price");
        BuildingDuplicate receipt;attempted=true;const auto code=building_duplicate_native(c.image,business,undo,target,selection_manager,release,&pose,price,receipt);need(!code && receipt.committed && !receipt.begin_cleanup && !receipt.key_cleanup && !receipt.end_cleanup && !receipt.args_cleanup && !receipt.label_cleanup,"Dungeon Building Duplicate failed SEH="+std::to_string(code)+" begin_cleanup="+std::to_string(receipt.begin_cleanup)+" key_cleanup="+std::to_string(receipt.key_cleanup)+" end_cleanup="+std::to_string(receipt.end_cleanup)+" args_cleanup="+std::to_string(receipt.args_cleanup)+" label_cleanup="+std::to_string(receipt.label_cleanup)+"; do not replay");graph=receipt.graph;created=receipt.object;
        need(c.read<int32_t>(release+0x124)==edits+1 && selection(c,selection_manager)==std::vector<uintptr_t>{created},"Dungeon Building Duplicate native release edit/selection readback differs");
    }else{
        need(c.read<uintptr_t>(c.image+0x16d93c0+0xd0)==c.image+0xd31530,"Dungeon Building CanMove virtual method differs");
        for(uintptr_t rva:{0xd31530,0x9ff370,0xa16f10,0x265300,0x539990,0x55e8e0,0x55ed80,0xcf1600,0x5379c0,0x265650,0x11b2b00,0x11bd1e0})c.verify_original(rva);
        bool allowed=false;int32_t ignored=0;seh(building_capability(c.image,target,false,allowed,ignored),"Building CanMove");need(allowed,"Dungeon native Building CanMove rejected target");MoveReceipt receipt;attempted=true;const auto code=move_native(c.image,target,&pose,receipt);need(!code && receipt.moved && !receipt.move_cleanup && !receipt.close_cleanup && !receipt.end_cleanup && !receipt.args_cleanup && !receipt.label_cleanup,"Dungeon Building Move failed SEH="+std::to_string(code)+" move_cleanup="+std::to_string(receipt.move_cleanup)+" close_cleanup="+std::to_string(receipt.close_cleanup)+" end_cleanup="+std::to_string(receipt.end_cleanup)+" args_cleanup="+std::to_string(receipt.args_cleanup)+" label_cleanup="+std::to_string(receipt.label_cleanup)+"; do not replay");graph=receipt.graph;
    }
    need(c.read<uintptr_t>(shard_manager+0x60)==o.shard,"Dungeon active template changed during Building control");committed_graph(c,undo,undo_before,graph);const auto after=building_rows(c,o.shard,o.region);
    if(destroying){auto expected=rows;std::erase(expected,target);need(after==expected,"Dungeon Building DestroyProps surviving scene roster differs");return {{"destroyed",true}};}
    if(duplicating){need(created && after.size()==rows.size()+1 && after.back()==created && std::equal(rows.begin(),rows.end(),after.begin()),"Dungeon Building Duplicate surviving scene roster differs");need(building_json(c,target)==before,"Dungeon Building Duplicate changed its source");}
    else need(after==rows,"Dungeon Building Move changed scene roster");
    auto result=building_json(c,duplicating?created:object(c,args.at("building"),0x16d93c0)),expected=before;expected["handle"]=result.at("handle");expected["position"]=pose.position;expected["rotation"]=pose.rotation;expected["scale"]=pose.scale;need(result==expected,"Dungeon Building native type/variant/pose readback differs");return result;
}
