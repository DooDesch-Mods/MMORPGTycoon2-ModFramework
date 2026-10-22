struct Heights {
    int32_t width=0,height=0;float cell=0;std::array<float,3> origin{};std::vector<float> values;
};
Heights heights(Context& c,uintptr_t region,size_t offset=0,size_t limit=1000000){
    Heights out;out.width=c.read<int32_t>(region+0xc0);out.height=c.read<int32_t>(region+0xc4);
    out.cell=c.read<float>(region+0xc8);out.origin=c.read<std::array<float,3>>(region+0x138);
    ap(c,region+0xe8,0x1749340);auto data=c.read<uintptr_t>(region+0xf0);
    auto count=c.read<int32_t>(region+0xf8),capacity=c.read<int32_t>(region+0xfc);
    need(out.width>0&&out.height>0&&int64_t(out.width)*out.height<=1000000&&count==int64_t(out.width)*out.height&&capacity>=count&&data,
        "Terrain height array or dimensions differ");
    need(std::isfinite(out.cell)&&out.cell>0,"Terrain height cell size must be positive and finite");
    for(auto value:out.origin)need(std::isfinite(value),"Terrain height origin is not finite");
    need(offset<=size_t(count),"Terrain height offset exceeds the native grid");out.values.resize(std::min(limit,size_t(count)-offset));
    for(size_t read=0;read<out.values.size();){auto size=std::min<size_t>(262144,out.values.size()-read);
        c.read_bytes(data+(offset+read)*sizeof(float),out.values.data()+read,size*sizeof(float));read+=size;}
    for(auto value:out.values)need(std::isfinite(value),"Terrain height sample is not finite");
    need(c.read<uintptr_t>(region+0xf0)==data&&c.read<int32_t>(region+0xf8)==count,"Terrain height array changed during read");return out;
}
int32_t height_index(const Heights& grid,const std::array<float,3>& point){
    auto x=std::floor((point[0]-grid.origin[0])/grid.cell),z=std::floor((point[2]-grid.origin[2])/grid.cell);
    need(std::isfinite(x)&&std::isfinite(z),"Terrain height point is outside the finite native grid");
    auto clamp=[](float value,int32_t count){return value<0?0:value>=float(count)?count-1:int32_t(value);};
    return clamp(z,grid.height)*grid.width+clamp(x,grid.width);
}
Json height_page(Context& c,const Json& args){
    auto offset=integer(args.value("offset",Json(0)),"height offset"),limit=integer(args.value("limit",Json(256)),"height limit");
    need(offset>=0&&offset<=1000000&&limit>0&&limit<=4096,"Terrain height page requires a valid offset and limit 1..4096");
    auto region=select(c,args.at("handle"));auto grid=heights(c,region.object,size_t(offset),size_t(limit));Json values=grid.values;
    return {{"handle",args.at("handle")},{"width",grid.width},{"height",grid.height},{"cell_size",grid.cell},{"origin",grid.origin},
        {"offset",offset},{"values",values},{"next_offset",size_t(offset)+values.size()},{"complete",size_t(offset)+values.size()==size_t(grid.width)*size_t(grid.height)}};
}
std::vector<uintptr_t> history_array(Context& c,uintptr_t object){
    ap(c,object,0x16e1320);auto data=c.read<uintptr_t>(object+8);auto count=c.read<int32_t>(object+0x10),capacity=c.read<int32_t>(object+0x14);
    need(count>=0&&count<=20&&capacity>=count&&(!count||data),"Terrain Undo history array differs");
    std::vector<uintptr_t> out(size_t(count),0);if(count)c.read_bytes(data,out.data(),out.size()*8);return out;
}
Json height_history(Context& c){
    need(!c.read<uintptr_t>(c.image+0x1a3aa80),"Terrain height requires no active native Undo graph");auto manager=c.singleton(0x13df380);ap(c,manager,0x16fe070);
    return {{"manager",manager},{"undo",history_array(c,manager+8)},{"redo",history_array(c,manager+0x20)}};
}
Json height_ledger(){
    mt2mp::game::native::SimulationState value;std::string error;
    need(mt2mp::game::native::read_simulation_state(value,error),"Terrain native ledger read failed: "+error);
    return {{"cash",value.cash},{"day",value.clock.flat_day},{"present",value.record_present},{"record_day",value.record_day},
        {"record_cash",value.record_cash},{"income",value.record_income},{"expense",value.record_expense},
        {"line_counts",value.line_counts},{"line_income",value.line_income}};
}
void height_add(Json& value,int64_t delta){auto next=integer(value,"financial value")+delta;
    need(next>=INT32_MIN&&next<=INT32_MAX,"Terrain height financial value would overflow");value=int32_t(next);}
DWORD height_region(uintptr_t image,uintptr_t map,const float* point,uintptr_t& region){
    __try{region=uintptr_t(reinterpret_cast<void*(*)(void*,const void*,void*)>(image+0x32a990)(reinterpret_cast<void*>(map),point,nullptr));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
struct HeightCall {bool constructed=false,returned=false,borrowed=false,destroyed=false;DWORD restore=0,cleanup=0;};
DWORD change_height(uintptr_t image,unsigned char* brush,uintptr_t cursor,uintptr_t model,const float* point,float radius,bool up,HeightCall& result){
    std::array<float,3> prior{};float prior_radius=0;DWORD code=0;
    __try{
        reinterpret_cast<void(*)(void*)>(image+0x5e16d0)(brush);result.constructed=true;
        brush[0x148]=up?1:0;std::memcpy(brush+0x14c,point,12);
        // A distinct cached cell lets the original Drag perform this explicit request once.
        *reinterpret_cast<float*>(brush+0x14c)=point[0]+1.0f;
        std::memcpy(prior.data(),reinterpret_cast<const void*>(model+0xe0),12);prior_radius=*reinterpret_cast<float*>(cursor+0x170);result.borrowed=true;
        std::memcpy(reinterpret_cast<void*>(model+0xe0),point,12);*reinterpret_cast<float*>(cursor+0x170)=radius;
        reinterpret_cast<void(*)(void*,void*,const void*,float)>(image+0x5e1860)(brush,reinterpret_cast<void*>(cursor),point,0.0f);result.returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(result.borrowed){__try{std::memcpy(reinterpret_cast<void*>(model+0xe0),prior.data(),12);*reinterpret_cast<float*>(cursor+0x170)=prior_radius;}
        __except(EXCEPTION_EXECUTE_HANDLER){result.restore=GetExceptionCode();}}
    if(result.constructed){__try{reinterpret_cast<void(*)(void*)>(image+0x5e12a0)(brush);result.destroyed=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){result.cleanup=GetExceptionCode();}}
    return code;
}
Json edit_height(Context& c,const Json& args,bool& attempted){
    c.require_mutation();require_root_view(c);auto region=select(c,args.at("handle"));need(c.read<uint8_t>(region.object+0x850)==1,"Terrain height requires an owned Region");
    auto input_point=input(args.at("position"),3);std::array<float,3> point{};std::copy(input_point.begin(),input_point.end(),point.begin());
    for(auto value:point)need(std::abs(value)<=1000000,"Terrain height position exceeds native world bounds");
    need(args.at("up").is_boolean(),"Terrain height up requires a Boolean");auto up=args.at("up").get<bool>();
    auto radius=args.value("radius",1.0);need(std::isfinite(radius)&&radius>0&&radius<=10000,"Terrain height radius must be in 0..10000");
    need(float(radius)>0,"Terrain height radius is not representable as a positive native float");
    auto grid=heights(c,region.object);auto cell=height_index(grid,point);auto expected=grid.values;expected[size_t(cell)]+=up?20.0f:-20.0f;
    need(std::isfinite(expected[size_t(cell)])&&std::bit_cast<uint32_t>(expected[size_t(cell)])!=std::bit_cast<uint32_t>(grid.values[size_t(cell)]),"Terrain height result would not be finite or represent a change");
    auto map=c.singleton(0x13df8c0);ap(c,map,0x1742860);c.verify_original(0x32a990);uintptr_t found=0;
    auto lookup=height_region(c.image,map,point.data(),found);need(!lookup&&found==region.object,"Terrain height position resolves another native Region SEH="+std::to_string(lookup));
    auto cursor=c.singleton(0x13df9d0);ap(c,cursor,0x1786dd0);auto model=c.read<uintptr_t>(cursor+0x160);ap(c,model,0x1749490);
    auto prior=c.read<std::array<float,3>>(model+0xe0);auto prior_radius=c.read<float>(cursor+0x170);
    auto history=height_history(c),money=height_ledger(),expected_money=money;need(money.at("present").get<bool>()&&money.at("cash").get<int32_t>()>=100,"Terrain height requires a current financial record and 100 cash");
    height_add(expected_money["cash"],-100);height_add(expected_money["record_cash"],-100);height_add(expected_money["expense"],100);
    height_add(expected_money["line_counts"][6],1);height_add(expected_money["line_income"][6],-100);
#include "height_pins.inc"
    need(c.read<std::array<float,2>>(c.image+0x14f4aa0)==std::array<float,2>{20.0f,-20.0f},"Terrain native height step constants differ");
    alignas(16) unsigned char brush[0x680]{};HeightCall receipt;attempted=true;
    auto code=change_height(c.image,brush,cursor,model,point.data(),float(radius),up,receipt);
    need(!code&&!receipt.restore&&!receipt.cleanup&&receipt.returned&&receipt.destroyed,"Terrain native height failed SEH="+std::to_string(code)+" restore="+std::to_string(receipt.restore)+" cleanup="+std::to_string(receipt.cleanup));
    need(c.singleton(0x13df9d0)==cursor&&c.read<uintptr_t>(cursor+0x160)==model&&c.read<std::array<float,3>>(model+0xe0)==prior&&c.read<float>(cursor+0x170)==prior_radius,"Terrain native cursor input restoration differs");
    need(select(c,args.at("handle")).object==region.object,"Terrain Region lifetime changed during mutation");auto after=heights(c,region.object);
    need(after.width==grid.width&&after.height==grid.height&&after.cell==grid.cell&&after.origin==grid.origin&&after.values==expected,"Terrain original complete grid consequence differs");
    for(size_t i=0;i<expected.size();++i)need(std::bit_cast<uint32_t>(after.values[i])==std::bit_cast<uint32_t>(expected[i]),"Terrain native height bits differ at cell "+std::to_string(i));
    need(height_history(c)==history,"Terrain height unexpectedly changed native Undo history");need(height_ledger()==expected_money,"Terrain height complete financial result differs");
    return {{"handle",args.at("handle")},{"cell",cell},{"before",grid.values[size_t(cell)]},{"height",after.values[size_t(cell)]},{"cost",100},{"undo_unchanged",true}};
}
