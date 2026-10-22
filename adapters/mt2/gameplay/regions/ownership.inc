DWORD ownership_price(uintptr_t image,int32_t& price){
    __try{price=reinterpret_cast<int32_t(*)()>(image+0x5954f0)();return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
struct OwnershipCall {uintptr_t receiver=0;bool constructed=false,returned=false,destroyed=false;DWORD cleanup=0;};
DWORD change_ownership(uintptr_t image,uintptr_t region,const float* hit,bool sale,OwnershipCall& result){
    DWORD code=0;
    __try{
        result.receiver=uintptr_t(reinterpret_cast<void*(*)(size_t)>(image+0xa02670)(0x160));if(!result.receiver)return ERROR_NOT_ENOUGH_MEMORY;
        reinterpret_cast<void(*)(void*)>(image+0x595490)(reinterpret_cast<void*>(result.receiver));result.constructed=true;
        *reinterpret_cast<uint8_t*>(result.receiver+0x148)=sale?1:0;
        reinterpret_cast<void(*)(void*,void*,const float*)>(image+0x595630)(reinterpret_cast<void*>(result.receiver),nullptr,hit);
        if(!*reinterpret_cast<uint8_t*>(result.receiver+0x158)||*reinterpret_cast<uintptr_t*>(result.receiver+0x150)!=region)code=ERROR_INVALID_DATA;
        else{reinterpret_cast<void(*)(void*,void*,const float*)>(image+0x5956f0)(reinterpret_cast<void*>(result.receiver),nullptr,hit);result.returned=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(result.constructed){__try{
        reinterpret_cast<void(*)(void*)>(image+0xc1e510)(reinterpret_cast<void*>(result.receiver));
        reinterpret_cast<void(*)(void*,size_t)>(image+0x1264b60)(reinterpret_cast<void*>(result.receiver),0x160);result.destroyed=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){result.cleanup=GetExceptionCode();}}
    return code;
}
void ownership_inverse(Context& c,const Json& before,const Json& after,uintptr_t region,const Json& previous,int32_t amount,const std::array<float,3>& hit){
    need(before.at("manager")==after.at("manager")&&!after.at("undo").empty()&&after.at("redo").empty(),"Region ownership Undo manager or redo differs");
    auto graph=after.at("undo").back().get<uintptr_t>();auto expected=before.at("undo");expected.push_back(graph);if(expected.size()>20)expected.erase(expected.begin());
    need(expected==after.at("undo")&&std::find(before.at("undo").begin(),before.at("undo").end(),Json(graph))==before.at("undo").end(),"Region ownership did not append one new native Undo graph");
    ap(c,graph,0x16e32d0);ap(c,graph+0x18,0x16e2dc0);ap(c,graph+0x48,0x1748c50);
    need(c.read<int32_t>(graph+0x58)==0&&c.read<uint8_t>(graph+0x10c)==1&&c.read<int32_t>(graph+0x28)==(amount?2:1),"Region ownership Undo graph count, lock or deletion state differs");
    auto data=c.read<uintptr_t>(graph+0x20),operation=c.read<uintptr_t>(data);ap(c,operation,0x1723230);
    need(c.read<uintptr_t>(operation+0xb8)==region&&c.read<int32_t>(operation+0xc0)==previous.at("type").get<int32_t>()&&
        c.read<std::array<int32_t,2>>(operation+0xc4)==std::array<int32_t,2>{previous.at("level").get<int32_t>(),previous.at("max_level").get<int32_t>()}&&
        c.read<uint8_t>(operation+0x160)==uint8_t(previous.at("owned").get<bool>())&&vector<4>(c,operation+0x150)==previous.at("tint"),
        "Region ownership inverse owner, type, range, tint or ownership differs");
    for(size_t i=0;i<4;++i)need(c.text(operation+0xd0+i*0x20)==previous.at("styles")[i].get<std::string>(),"Region ownership inverse styles differ");
    if(amount){auto cash=c.read<uintptr_t>(data+8);ap(c,cash,0x1786fb0);
        need(c.read<int32_t>(cash+0xb8)==amount&&c.read<std::array<float,3>>(cash+0xbc)==hit,"Region ownership signed cash inverse or position differs");}
}
Json edit_ownership(Context& c,const Json& args,bool sale,bool& attempted){
    c.require_mutation();require_root_view(c);auto region=select(c,args.at("handle"));auto before=state(c,region),expected=before;
    need(before.at("type").get<int32_t>()>0&&before.at("owned").get<bool>()==sale,sale?"Region sale requires owned land":"Region purchase requires unowned land");
    auto values=input(args.value("position",before.at("position")),3);std::array<float,3> hit{};std::copy(values.begin(),values.end(),hit.begin());
    for(auto value:hit)need(std::abs(value)<=1000000,"Region ownership position exceeds world bounds");
    c.verify_original(0x32a990);auto map=c.singleton(0x13df8c0);ap(c,map,0x1742860);uintptr_t found=0;auto lookup=height_region(c.image,map,hit.data(),found);
    need(!lookup&&found==region.object,"Region ownership hit resolves another Region SEH="+std::to_string(lookup));
    c.verify_original(0x5954f0);int32_t price=-1;auto pricing=ownership_price(c.image,price);need(!pricing&&price>=0,"Native Region price failed SEH="+std::to_string(pricing));
    auto money=height_ledger(),expected_money=money;need(money.at("present").get<bool>()&&(sale||money.at("cash").get<int32_t>()>=price),"Region ownership requires a current financial record and sufficient cash");
    const auto delta=sale?int64_t(price):-int64_t(price);height_add(expected_money["cash"],delta);height_add(expected_money["record_cash"],delta);
    height_add(expected_money[sale?"income":"expense"],price);height_add(expected_money["line_counts"][7],1);height_add(expected_money["line_income"][7],delta);
    const std::array<int32_t,2> range=sale?std::array<int32_t,2>{0,0}:std::array<int32_t,2>{1,1};auto actors=level_members(c,region.object,range);auto history=height_history(c);
    for(auto entry:{0xa02670,0x595490,0x595630,0x5956f0,0xc1e510,0x1264b60})c.verify_original(entry);
    static std::vector<uintptr_t> uncertain;need(uncertain.size()<64,"Region ownership native receiver retention budget exhausted");OwnershipCall receipt;attempted=true;
    auto code=change_ownership(c.image,region.object,hit.data(),sale,receipt);if(receipt.receiver&&!receipt.destroyed)uncertain.push_back(receipt.receiver);
    need(!code&&!receipt.cleanup&&receipt.returned&&receipt.destroyed,"Native Region ownership transaction failed SEH="+std::to_string(code)+" cleanup="+std::to_string(receipt.cleanup));
    need(select(c,args.at("handle")).object==region.object,"Region lifetime changed during ownership edit");auto after=state(c,region);
    expected["owned"]=!sale;expected["status"]=sale?0:1;expected["level"]=range[0];expected["max_level"]=range[1];
    need(after==expected,"Region ownership complete authored state differs");auto current=level_members(c,region.object,range);
    need(current.roster==actors.roster&&current.counts==actors.counts&&current.levels==actors.expected&&current.expected==current.levels,"Region ownership complete actor clamp or roster differs");
    need(height_ledger()==expected_money,"Region ownership complete financial result differs");ownership_inverse(c,history,height_history(c),region.object,before,sale?-price:price,hit);
    refresh_region(c,region.object);return after;
}
