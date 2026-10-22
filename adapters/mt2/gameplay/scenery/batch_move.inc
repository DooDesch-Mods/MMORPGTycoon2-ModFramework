// Copyright (c) 2026 DooDesch. Original standalone adapter under the MIT license.
// Native transaction evidence: Multiplayer/src/game/selection_move_native.h.
struct BatchTarget {uintptr_t pointer=0,region=0;std::array<float,10> before{},wanted{};};
struct BatchReceipt {MoveReceipt scope;size_t constructed=0,moved=0,destroyed=0;};
uint32_t batch_native(uintptr_t image,const BatchTarget* targets,size_t count,BatchReceipt& r){
    alignas(16) unsigned char transform[0x6c]{},label[0x38]{},operation[0x130]{};uint32_t code=0;bool live=false;
    __try{
        reinterpret_cast<void(*)(void*,const char*)>(image+0xa16f10)(label,"Move");r.scope.label=true;
        reinterpret_cast<void(*)(const void*)>(image+0x265300)(label);r.scope.begun=true;
        r.scope.graph=reinterpret_cast<uintptr_t(*)(const void*)>(image+0x539990)(label);r.scope.opened=true;
        if(!r.scope.graph)code=ERROR_INVALID_HANDLE;
        else for(size_t n=0;n<count;++n){
            std::memset(transform,0,sizeof(transform));std::memcpy(transform,targets[n].wanted.data(),40);transform[0x28]=1;
            reinterpret_cast<void(*)(void*,uintptr_t,const void*)>(image+0x55e8e0)(operation,targets[n].pointer,transform);live=true;++r.constructed;
            reinterpret_cast<void(*)(void*,uintptr_t)>(image+0x55ed80)(operation,r.scope.graph);++r.moved;
            live=false;reinterpret_cast<void(*)(void*)>(image+0xcf1600)(operation);++r.destroyed;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(live){__try{reinterpret_cast<void(*)(void*)>(image+0xcf1600)(operation);++r.destroyed;}__except(EXCEPTION_EXECUTE_HANDLER){r.scope.move_cleanup=GetExceptionCode();}}
    if(r.scope.opened && r.scope.graph){__try{reinterpret_cast<void(*)()>(image+0x5379c0)();}__except(EXCEPTION_EXECUTE_HANDLER){r.scope.close_cleanup=GetExceptionCode();}}
    if(r.scope.begun){__try{reinterpret_cast<void(*)()>(image+0x265650)();}__except(EXCEPTION_EXECUTE_HANDLER){r.scope.end_cleanup=GetExceptionCode();}}
    if(r.scope.label){__try{reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(label+0x20);}__except(EXCEPTION_EXECUTE_HANDLER){r.scope.args_cleanup=GetExceptionCode();}
        __try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(label);}__except(EXCEPTION_EXECUTE_HANDLER){r.scope.label_cleanup=GetExceptionCode();}}
    return code;
}
uint32_t batch_destination(uintptr_t image,uintptr_t root,const float* position,uintptr_t& region){
    __try{region=reinterpret_cast<uintptr_t(*)(uintptr_t,const void*,void*)>(image+0x5089b0)(root,position,nullptr);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t batch_can_move(uintptr_t image,uintptr_t target,bool& allowed){
    __try{allowed=reinterpret_cast<bool(*)(uintptr_t)>(image+0xcf3130)(target);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
Json transform_batch(Context& c,const World& w,const Json& args,bool& attempted){
    const auto& input=args.at("targets");need(input.is_array() && !input.empty() && input.size()<=4096,"Scenery transform requires 1..4096 targets");
    const auto before=scenery_census(c,w);const auto groups_before=groups(c,w);const auto history=array(c,w.undo+8,20);std::vector<BatchTarget> targets;std::set<uintptr_t> unique;bool changed=false;
    for(uintptr_t rva:{0xcf3130,0x446020,0x5089b0,0xa16f10,0x265300,0x539990,0x55e8e0,0x55ed80,0xcf1600,0x5379c0,0x265650,0x11b2b00,0x11bd1e0})c.verify_original(rva);
    need(c.read<uintptr_t>(c.image+0x16c1390+0xd0)==c.image+0xcf3130,"Scenery transform original CanMove virtual method differs");
    for(const auto& row:input){BatchTarget t;t.pointer=c.resolve(row.at("handle"),0x16c1390);need(unique.insert(t.pointer).second && before.contains(t.pointer),"Scenery transform repeats a target or target is absent from current root");
        need(!c.read<uintptr_t>(t.pointer+0x180),"Scenery transform requires free Scenery; transform its whole Group instead");
        bool allowed=false;seh(batch_can_move(c.image,t.pointer,allowed),"CanMove");need(allowed,"Scenery native CanMove rejected target");
        t.before=before.at(t.pointer).at("pose").get<std::array<float,10>>();t.wanted=requested_pose(row,t.before);changed|=t.wanted!=t.before;
        seh(batch_destination(c.image,w.root,t.wanted.data()+4,t.region),"transform destination Region");need(std::find(w.regions.begin(),w.regions.end(),t.region)!=w.regions.end() && c.read<uint8_t>(t.region+0x850)==1,"Scenery transform destination is not an owned root-world Region");
        const auto manager=c.read<uintptr_t>(t.region+0x960);need(manager && c.read<uintptr_t>(manager)==c.image+0x17152d0 && c.read<uintptr_t>(manager+0x900)==t.region && c.read<uintptr_t>(manager+0x908)==w.root,"Scenery transform destination manager reciprocity differs");targets.push_back(t);
    }
    need(changed,"Scenery transform has no changed pose");BatchReceipt r;attempted=true;const auto code=batch_native(c.image,targets.data(),targets.size(),r);
    need(!code && r.constructed==targets.size() && r.moved==targets.size() && r.destroyed==targets.size() && !r.scope.move_cleanup && !r.scope.close_cleanup && !r.scope.end_cleanup && !r.scope.args_cleanup && !r.scope.label_cleanup,
        "Scenery transform failed SEH="+std::to_string(code)+" constructed="+std::to_string(r.constructed)+" moved="+std::to_string(r.moved)+" destroyed="+std::to_string(r.destroyed)+" move_cleanup="+std::to_string(r.scope.move_cleanup)+" close_cleanup="+std::to_string(r.scope.close_cleanup)+" end_cleanup="+std::to_string(r.scope.end_cleanup)+" args_cleanup="+std::to_string(r.scope.args_cleanup)+" label_cleanup="+std::to_string(r.scope.label_cleanup)+"; do not replay");
    const auto graph=committed(c,w,history,targets.size());need(graph==r.scope.graph,"Scenery transform committed another native graph");const auto ops=array(c,graph+0x18);const auto after=scenery_census(c,world(c,true));need(after.size()==before.size(),"Scenery transform changed the native roster");
    Json result=Json::array();for(size_t n=0;n<targets.size();++n){const auto& t=targets[n];const auto op=ops[n];need(c.read<uintptr_t>(op)==c.image+0x1786ff0 && c.read<uint64_t>(op+0xb8)==c.read<uint64_t>(t.pointer+0x18) && pose(c,op,0xc0)==t.before,"Scenery transform inverse identity/prior pose/order differs");
        need(after.contains(t.pointer) && after.at(t.pointer).at("pose")==Json(t.wanted) && c.read<uintptr_t>(t.pointer+0x178)==t.region && !c.read<uintptr_t>(t.pointer+0x180),"Scenery transform destination pose/Region/parent readback differs");result.push_back(after.at(t.pointer));}
    for(const auto& [p,row]:before)if(!unique.contains(p))need(after.contains(p) && after.at(p)==row,"Scenery transform changed an unrelated member");
    const auto groups_after=groups(c,w);need(group_pointers(groups_after)==group_pointers(groups_before),"Scenery transform changed the Group roster");for(size_t n=0;n<groups_before.size();++n)need(groups_after[n].value==groups_before[n].value,"Scenery transform changed an unrelated Group");return result;
}
