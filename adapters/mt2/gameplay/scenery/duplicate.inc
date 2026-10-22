// Original MIT adapter. Native evidence: duplicate_calls.h, duplicate_port.inc,
// duplicate_native.h and duplicate_codeguards.h.
struct DuplicateReceipt {
    uintptr_t object=0,graph=0;bool begun=false,committed=false;
    uint32_t end_cleanup=0,name_cleanup=0,key_cleanup=0,args_cleanup=0,label_cleanup=0;
};
uint32_t duplicate_probe(uintptr_t image,uintptr_t business,uintptr_t source,uintptr_t can,uintptr_t cost,int32_t& price,bool& allowed,bool& affordable){
    __try{allowed=reinterpret_cast<bool(*)(uintptr_t)>(image+can)(source);price=reinterpret_cast<int32_t(*)(uintptr_t)>(image+cost)(source);affordable=price>=0 && reinterpret_cast<bool(*)(uintptr_t,int32_t)>(image+0x54d00)(business,price);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t duplicate_destination(uintptr_t image,uintptr_t root,const std::array<float,10>* pose,bool group,uintptr_t& region){
    __try{region=group?reinterpret_cast<uintptr_t(*)(uintptr_t,const void*,uintptr_t)>(image+0x5089b0)(root,pose->data()+4,0):
        reinterpret_cast<uintptr_t(*)(uintptr_t,const void*)>(image+0x508a40)(root,pose->data()+4);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t duplicate_native(uintptr_t image,const World* world,uintptr_t business,uintptr_t release,uintptr_t source,bool group,const std::array<float,10>* pose,int32_t price,DuplicateReceipt& r){
    alignas(16) unsigned char transform[0x6c]{},name[0x20]{},key[0x20]{},label[0x38]{};bool name_ready=false,key_ready=false,label_ready=false;uintptr_t operation=0;uint32_t code=0;
    __try{
        if(price>0 && !reinterpret_cast<bool(*)(uintptr_t,int32_t,int32_t)>(image+0x54d80)(business,price,6))code=ERROR_ACCESS_DENIED;
        else{
            if(reinterpret_cast<bool(*)()>(image+0x264c30)()){reinterpret_cast<void(*)(void*,const char*)>(image+0x5c04b0)(name,"Duplicating collection");name_ready=true;reinterpret_cast<void(*)(const void*)>(image+0x265300)(name);r.begun=true;}
            r.graph=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x110);if(!r.graph)code=ERROR_NOT_ENOUGH_MEMORY;
            else{
                reinterpret_cast<void(*)(uintptr_t)>(image+0x5400f0)(r.graph);operation=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xc8);
                if(!operation)code=ERROR_NOT_ENOUGH_MEMORY;
                else{
                    reinterpret_cast<void(*)(uintptr_t,int32_t,const void*)>(image+0x53ccb0)(operation,price,pose->data()+4);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x540760)(r.graph,operation);
                    std::memcpy(transform,pose->data(),sizeof(*pose));transform[0x28]=1;r.object=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*)>(image+(group?0x4cec20:0x445f60))(source,transform);
                    if(!r.object)code=ERROR_INVALID_HANDLE;
                    else{
                        operation=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xc0);if(!operation)code=ERROR_NOT_ENOUGH_MEMORY;
                        else{
                            reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x53dc30)(operation,r.object);reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x540760)(r.graph,operation);
                            const NativeArray targets{image+0x1748c50,&r.object,1,1};reinterpret_cast<void(*)(uintptr_t,const NativeArray*)>(image+0x2d5910)(world->selection,&targets);
                            if(!group)reinterpret_cast<void(*)(uintptr_t,int32_t,int32_t)>(image+0x4ee5c0)(release+0x108,2,1);
                            reinterpret_cast<void(*)(void*,const char*)>(image+0x5c04b0)(key,"{undo_msg_duplicate}");key_ready=true;
                            reinterpret_cast<void(*)(void*,const void*,const NativeArray*)>(image+0x539230)(label,key,&targets);label_ready=true;
                            reinterpret_cast<void(*)(uintptr_t,const void*,uintptr_t)>(image+0x53b260)(world->undo,label,r.graph);r.committed=true;
                        }
                    }
                }
            }
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(label_ready){__try{reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.label_cleanup=GetExceptionCode();}}
    if(key_ready){__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(key);}__except(EXCEPTION_EXECUTE_HANDLER){r.key_cleanup=GetExceptionCode();}}
    if(name_ready){__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(name);}__except(EXCEPTION_EXECUTE_HANDLER){r.name_cleanup=GetExceptionCode();}}
    if(r.begun){__try{reinterpret_cast<void(*)()>(image+0x265650)();}__except(EXCEPTION_EXECUTE_HANDLER){r.end_cleanup=GetExceptionCode();}}return code;
}
Json duplicate(Context& c,const World& w,const Json& args,bool& attempted){
    const auto source=c.resolve(args.at("handle"));const auto ap=c.read<uintptr_t>(source);const bool group=ap==c.image+0x1749de0;need(group || ap==c.image+0x16c1390,"Scenery Duplicate requires a native Group or free Scenery");
    const auto before=groups(c,w);const auto props=scenery_census(c,w);const auto selected=std::find_if(before.begin(),before.end(),[&](const auto& row){return row.pointer==source;});
    if(group)need(selected!=before.end() && !selected->members.empty(),"Scenery Duplicate Group is absent or empty");else need(props.contains(source) && !c.read<uintptr_t>(source+0x180),"Scenery Duplicate requires a free current root member");
    const auto wanted=requested_pose(args,pose(c,source));const auto undo_before=array(c,w.undo+8,20);const auto business=c.singleton(0x13df220),release_manager=c.singleton(0x13df4f0);
    need(business && release_manager && c.read<uintptr_t>(release_manager)==c.image+0x1714ed0,"Scenery Duplicate Business/ReleaseManager is absent");const auto release=c.read<uintptr_t>(release_manager+0x38);need(release && c.read<uintptr_t>(release)==c.image+0x16c12b0,"Scenery Duplicate current Release class differs");auto expected_edits=c.read<std::array<int32_t,8>>(release+0x120);if(!group){need(expected_edits[2]>=0 && expected_edits[2]<INT32_MAX,"Scenery Duplicate release edit counter overflows");++expected_edits[2];}
    const uintptr_t can=group?0xe470e0:0xcf30a0,method=group?0x4cec20:0x445f60,cost=group?0x4ceb00:0xcf30e0;
    need(c.read<uintptr_t>(ap+0xd8)==c.image+can && c.read<uintptr_t>(ap+0xe8)==c.image+method && c.read<uintptr_t>(ap+0x278)==c.image+cost,"Scenery Duplicate original virtual methods differ");
    for(uintptr_t rva:std::initializer_list<uintptr_t>{can,method,cost,0x54d00,0x54d80,0x264c30,0x5c04b0,0x265300,0xa02670,0x5400f0,0x53ccb0,0x540760,0x53dc30,0x2d5910,0x4ee5c0,0x539230,0x53b260,0x11b2b00,0x11bd1e0,0x265650,0x508a40,0x5089b0})c.verify_original(rva);
    bool allowed=false,affordable=false;int32_t price=-1;seh(duplicate_probe(c.image,business,source,can,cost,price,allowed,affordable),"CanDuplicate/Cost/CanAfford");need(allowed && affordable && price>=0,"Scenery native Duplicate capability or affordability rejected target");uintptr_t region=0;seh(duplicate_destination(c.image,w.root,&wanted,group,region),"Duplicate destination Region");need(std::find(w.regions.begin(),w.regions.end(),region)!=w.regions.end(),"Scenery Duplicate destination has no current root Region");
    DuplicateReceipt r;attempted=true;const auto code=duplicate_native(c.image,&w,business,release,source,group,&wanted,price,r);need(!code && r.committed && !r.end_cleanup && !r.name_cleanup && !r.key_cleanup && !r.args_cleanup && !r.label_cleanup,"Scenery Duplicate failed SEH="+std::to_string(code)+" end_cleanup="+std::to_string(r.end_cleanup)+" name_cleanup="+std::to_string(r.name_cleanup)+" key_cleanup="+std::to_string(r.key_cleanup)+" args_cleanup="+std::to_string(r.args_cleanup)+" label_cleanup="+std::to_string(r.label_cleanup)+"; do not replay");
    const auto current=world(c,true);const auto after=groups(c,current);const auto props_after=scenery_census(c,current);need(c.read<std::array<int32_t,8>>(release+0x120)==expected_edits && array(c,w.selection+0x20)==std::vector<uintptr_t>{r.object},"Scenery Duplicate native release edits/selection readback differs");
    need(r.object && c.resolve(handle(c,r.object),group?0x1749de0:0x16c1390)==r.object && pose(c,r.object)==wanted && !c.read<uintptr_t>(r.object+0x180) && c.read<uintptr_t>(r.object+0x178)==region,"Scenery Duplicate created full UID/class/pose/Region differs");
    for(const auto& [p,row]:props){const auto found=props_after.find(p);need(found!=props_after.end() && found->second==row,"Scenery Duplicate changed an existing Scenery member");}
    for(const auto& row:before){const auto found=std::find_if(after.begin(),after.end(),[&](const auto& value){return value.pointer==row.pointer;});need(found!=after.end() && found->value==row.value,"Scenery Duplicate changed an existing Group");}
    Json result;
    if(group){const auto created=std::find_if(after.begin(),after.end(),[&](const auto& row){return row.pointer==r.object;});need(created!=after.end() && after.size()==before.size()+1 && created->members.size()==selected->members.size() && props_after.size()==props.size()+selected->members.size(),"Scenery Duplicate whole Group/member roster count differs");
        for(size_t i=0;i<created->members.size();++i){const auto p=created->members[i];need(!props.contains(p) && c.read<int32_t>(p+0x268)==c.read<int32_t>(selected->members[i]+0x268) && created->value.at("members").at(i).at("relative_pose")==selected->value.at("members").at(i).at("relative_pose"),"Scenery Duplicate Group child category/order/relative pose differs");}result=created->value;
    }else{need(group_pointers(before)==group_pointers(after) && props_after.size()==props.size()+1 && props_after.contains(r.object) && !props.contains(r.object) && c.read<int32_t>(r.object+0x268)==c.read<int32_t>(source+0x268),"Scenery Duplicate native member/category roster differs");result=props_after.at(r.object);}
    const auto graph=committed(c,w,undo_before,2);need(graph==r.graph,"Scenery Duplicate committed another native graph");const auto operations=array(c,graph+0x18);const auto cash=operations[0],destroy=operations[1];need(c.read<uintptr_t>(cash)==c.image+0x1786fb0 && c.read<int32_t>(cash+0xb8)==price && c.read<std::array<float,3>>(cash+0xbc)==std::array<float,3>{wanted[4],wanted[5],wanted[6]} && c.read<uintptr_t>(destroy)==c.image+0x16de860 && c.read<uint64_t>(destroy+0xb8)==c.read<uint64_t>(r.object+0x18),"Scenery Duplicate original cash/destroy inverse amount/position/full UID differs");return result;
}
