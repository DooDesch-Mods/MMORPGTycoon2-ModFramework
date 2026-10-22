levels::Census level_members(Context& c,uintptr_t region,const std::array<int32_t,2>& range){
    levels::Census out;std::string cause;
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string&)->bool{value=c.read<std::remove_reference_t<decltype(value)>>(object+offset);return true;};
    need(levels::census(c.image,region,range,out,read,cause),"Region level actor census failed: "+cause);return out;
}
DWORD find_level_window(uintptr_t image,uintptr_t& result){
    const Text name{"level_select",12};
    __try{result=uintptr_t(reinterpret_cast<void*(*)(const Text*)>(image+0x81aa90)(&name));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
struct LevelCall {uintptr_t brush=0,graph=0;bool constructed=false,returned=false,destroyed=false;DWORD restore=0,cleanup=0;};
DWORD change_levels(uintptr_t image,uintptr_t cursor,uintptr_t collision,uintptr_t window,uintptr_t widget,uintptr_t region,const int32_t* range,const float* hit,LevelCall& result){
    uintptr_t prior_region=0;std::array<int32_t,3> prior{};bool borrowed=false;DWORD code=0;
    __try{
        result.brush=uintptr_t(reinterpret_cast<void*(*)(size_t)>(image+0xa02670)(0x178));
        if(!result.brush)return ERROR_NOT_ENOUGH_MEMORY;
        reinterpret_cast<void(*)(void*)>(image+0x5d82f0)(reinterpret_cast<void*>(result.brush));result.constructed=true;
        *reinterpret_cast<uintptr_t*>(result.brush+0x148)=window;
        prior_region=*reinterpret_cast<uintptr_t*>(collision+0x1d8);std::memcpy(prior.data(),reinterpret_cast<const void*>(widget+0x348),12);borrowed=true;
        *reinterpret_cast<uintptr_t*>(collision+0x1d8)=region;
        reinterpret_cast<void(*)(void*,int32_t,int32_t)>(image+0x729ae0)(reinterpret_cast<void*>(widget),range[0],range[1]);
        reinterpret_cast<void(*)(void*,void*,const float*)>(image+0x5d7cc0)(reinterpret_cast<void*>(result.brush),reinterpret_cast<void*>(cursor),hit);
        result.graph=*reinterpret_cast<uintptr_t*>(result.brush+0x170);
        reinterpret_cast<void(*)(void*,void*,const float*)>(image+0x5d83a0)(reinterpret_cast<void*>(result.brush),reinterpret_cast<void*>(cursor),hit);result.returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(borrowed){__try{
        *reinterpret_cast<uintptr_t*>(collision+0x1d8)=prior_region;std::memcpy(reinterpret_cast<void*>(widget+0x348),prior.data(),12);
        reinterpret_cast<void(*)(void*)>(image+0x7299c0)(reinterpret_cast<void*>(widget));
        if(*reinterpret_cast<uintptr_t*>(collision+0x1d8)!=prior_region||std::memcmp(reinterpret_cast<const void*>(widget+0x348),prior.data(),12))result.restore=ERROR_INVALID_DATA;
    }__except(EXCEPTION_EXECUTE_HANDLER){result.restore=GetExceptionCode();}}
    // An uncertain graph can still own the private receiver. Retain it until process exit.
    if(!code&&!result.restore&&result.returned&&result.constructed){__try{
        reinterpret_cast<void(*)(void*)>(image+0x5d7dc0)(reinterpret_cast<void*>(result.brush));
        reinterpret_cast<void(*)(void*,size_t)>(image+0x1264b60)(reinterpret_cast<void*>(result.brush),0x178);result.destroyed=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){result.cleanup=GetExceptionCode();}}
    return code;
}
void level_inverse(Context& c,const Json& before,const Json& after,uintptr_t graph,uintptr_t region,const Json& previous){
    need(before.at("manager")==after.at("manager")&&after.at("redo").empty()&&!after.at("undo").empty()&&after.at("undo").back()==graph,
        "Region level original Undo owner or committed graph differs");
    auto expected=before.at("undo");expected.push_back(graph);if(expected.size()>20)expected.erase(expected.begin());
    need(expected==after.at("undo"),"Region level native Undo stack differs");ap(c,graph,0x16e32d0);ap(c,graph+0x18,0x16e2dc0);ap(c,graph+0x48,0x1748c50);
    need(c.read<int32_t>(graph+0x10)==0&&c.read<int32_t>(graph+0x58)==0&&c.read<uint8_t>(graph+0x10c)==1&&c.read<int32_t>(graph+0x28)==1,
        "Region level Undo graph must be drained, locked and contain one inverse");
    auto data=c.read<uintptr_t>(graph+0x20),operation=c.read<uintptr_t>(data);ap(c,operation,0x1723230);
    need(c.read<uintptr_t>(operation+0xb8)==region&&c.read<int32_t>(operation+0xc0)==previous.at("type").get<int32_t>()&&
        c.read<std::array<int32_t,2>>(operation+0xc4)==std::array<int32_t,2>{previous.at("level").get<int32_t>(),previous.at("max_level").get<int32_t>()},
        "Region level inverse owner, type or previous range differs");
}
Json edit_levels(Context& c,const Json& args,bool& attempted){
    c.require_mutation();require_root_view(c);auto region=select(c,args.at("handle"));auto before=state(c,region),expected=before;
    auto low=integer(args.at("minimum"),"minimum level"),high=integer(args.at("maximum"),"maximum level");
    need(low>=1&&high>=low&&high<=INT32_MAX/10-10,"Region level range exceeds the native domain");
    need(before.at("owned").get<bool>()&&(low!=before.at("level").get<int32_t>()||high!=before.at("max_level").get<int32_t>()),"Region levels require owned land and a changed range");
    auto cursor=c.singleton(0x13df9d0);ap(c,cursor,0x1786dd0);auto collision=c.read<uintptr_t>(cursor+0x230);need(collision!=0,"Region level cursor collision owner is absent");
    c.verify_original(0x81aa90);uintptr_t window=0;auto find=find_level_window(c.image,window);need(!find&&window,"Native Region level window lookup failed SEH="+std::to_string(find));
    ap(c,window,0x1724450);auto widget=c.read<uintptr_t>(window+0x4f0);ap(c,widget,0x17241f0);
    auto prior=c.read<std::array<int32_t,3>>(widget+0x348),domain=c.read<std::array<int32_t,3>>(widget+0x354);
    need((domain[2]&255)==1&&domain[0]>=1&&domain[1]>=domain[0]&&domain[1]<=INT32_MAX/10-10&&low>=domain[0]&&high<=domain[1],
        "Region level range required="+std::to_string(domain[0])+".."+std::to_string(domain[1])+" requested="+std::to_string(low)+".."+std::to_string(high));
    std::array<int32_t,2> range{int32_t(low),int32_t(high)};auto actors=level_members(c,region.object,range);
    auto values=input(args.value("position",before.at("position")),3);std::array<float,3> hit{};std::copy(values.begin(),values.end(),hit.begin());
    for(auto value:hit)need(std::abs(value)<=1000000,"Region level position exceeds world bounds");
    auto map=c.singleton(0x13df8c0);ap(c,map,0x1742860);c.verify_original(0x32a990);uintptr_t found=0;auto lookup=height_region(c.image,map,hit.data(),found);
    need(!lookup&&found==region.object,"Region level hit resolves another Region SEH="+std::to_string(lookup));
    auto history=height_history(c),money=height_ledger();auto prior_region=c.read<uintptr_t>(collision+0x1d8);auto depth=c.read<int32_t>(c.image+0x13beda0);
    for(auto entry:{0xa02670,0x5d82f0,0x729ae0,0x5d7cc0,0x5d8480,0x5d83a0,0x7299c0,0x5d7dc0,0x1264b60})c.verify_original(entry);
    static std::vector<uintptr_t> uncertain;need(uncertain.size()<64,"Region level private receiver retention budget exhausted after uncertain native operations");
    LevelCall receipt;attempted=true;auto code=change_levels(c.image,cursor,collision,window,widget,region.object,range.data(),hit.data(),receipt);
    if(receipt.brush&&!receipt.destroyed)uncertain.push_back(receipt.brush);
    need(!code&&!receipt.restore&&!receipt.cleanup&&receipt.returned&&receipt.destroyed&&receipt.graph,"Region original level transaction failed SEH="+std::to_string(code)+" restore="+std::to_string(receipt.restore)+" cleanup="+std::to_string(receipt.cleanup));
    need(c.singleton(0x13df9d0)==cursor&&c.read<uintptr_t>(cursor+0x230)==collision&&c.read<uintptr_t>(collision+0x1d8)==prior_region&&c.read<std::array<int32_t,3>>(widget+0x348)==prior&&c.read<int32_t>(c.image+0x13beda0)==depth,
        "Region level original input or edit scope restoration differs");
    need(select(c,args.at("handle")).object==region.object,"Region lifetime changed during level edit");auto after=state(c,region);expected["level"]=low;expected["max_level"]=high;
    need(after==expected,"Region complete state differs after original level transaction");auto current=level_members(c,region.object,range);
    need(current.roster==actors.roster&&current.counts==actors.counts&&current.levels==actors.expected&&current.expected==current.levels,"Region original complete actor level clamp or roster differs");
    level_inverse(c,history,height_history(c),receipt.graph,region.object,before);need(height_ledger()==money,"Region level transaction changed the native ledger");
    refresh_region(c,region.object);
    return after;
}
