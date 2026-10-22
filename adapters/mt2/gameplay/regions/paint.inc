using PaintPoint=paint_model::Point;
struct PaintView {uintptr_t begin=0,end=0,capacity=0;};
static_assert(sizeof(PaintPoint)==16&&sizeof(PaintView)==24);
struct PaintShape {alignas(16) std::array<unsigned char,0x70> bytes{};bool started=false,constructed=false;};
struct PaintReceiver {
    alignas(16) std::array<unsigned char,0x680> bytes{};bool started=false,constructed=false,finished=false;
    std::vector<std::unique_ptr<PaintShape>> shapes;
};
std::vector<std::unique_ptr<PaintReceiver>> uncertain_paint;
std::vector<std::unique_ptr<PaintShape>> uncertain_shapes;
DWORD paint_seh(void(*function)(void*),void* context){
    __try{function(context);return 0;}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
template<class Function>void paint_native(Function&& function){
    using F=std::remove_reference_t<Function>;auto code=paint_seh([](void* value){(*static_cast<F*>(value))();},&function);
    need(!code,"Native terrain operation failed SEH="+std::to_string(code));
}
template<class Result,class... Args>Result paint_call(Context& c,uint64_t rva,Args... args){return reinterpret_cast<Result(*)(Args...)>(c.image+rva)(args...);}
size_t paint_span(const PaintView& value,size_t stride,size_t limit){
    if(!value.begin){need(!value.end&&!value.capacity,"Terrain native vector has an absent start with a nonempty span");return 0;}
    need(value.end>=value.begin&&value.capacity>=value.end&&(value.end-value.begin)%stride==0&&(value.capacity-value.begin)%stride==0&&
        (value.end-value.begin)/stride<=limit,"Terrain native vector span exceeds its alignment or budget");return (value.end-value.begin)/stride;
}
PaintView paint_paths(Context& c,uintptr_t shape){auto data=c.read<uintptr_t>(shape+0x18);need(data!=0,"Terrain native polygon data is absent");return c.read<PaintView>(data);}
bool same_paint_shape(Context& c,uintptr_t left,uintptr_t right){
    auto a=paint_paths(c,left),b=paint_paths(c,right);auto count=paint_span(a,24,1000000);if(count!=paint_span(b,24,1000000))return false;
    size_t remaining=1000000;
    for(size_t path=0;path<count;++path){auto av=c.read<PaintView>(a.begin+path*24),bv=c.read<PaintView>(b.begin+path*24);auto size=paint_span(av,16,remaining);
        if(size!=paint_span(bv,16,remaining))return false;remaining-=size;
        for(size_t offset=0;offset<size;){auto n=std::min<size_t>(1024,size-offset);std::array<PaintPoint,1024> aa{},bb{};
            c.read_bytes(av.begin+offset*16,aa.data(),n*16);c.read_bytes(bv.begin+offset*16,bb.data(),n*16);
            if(!std::equal(aa.begin(),aa.begin()+n,bb.begin()))return false;offset+=n;}}
    return true;
}
void destroy_paint_shape(Context& c,std::unique_ptr<PaintShape>& shape){
    if(!shape)return;
    try{if(shape->constructed){paint_native([&]{paint_call<void>(c,0xf15a0,shape->bytes.data());});shape->constructed=false;}
        else need(!shape->started,"Terrain polygon constructor has an uncertain native lifetime");}
    catch(const std::exception& error){uncertain_shapes.push_back(std::move(shape));throw std::runtime_error(std::string(error.what())+"; native polygon storage retained");}
}
std::unique_ptr<PaintShape> region_paint_shape(Context& c,uintptr_t region,int32_t style){
    need(uncertain_shapes.size()<16,"Terrain native polygon retention budget exhausted");auto shape=std::make_unique<PaintShape>();shape->started=true;
    try{paint_native([&]{paint_call<void*>(c,0x362450,shape->bytes.data(),reinterpret_cast<void*>(region+0x78),style);shape->constructed=true;});return shape;}
    catch(const std::exception& error){uncertain_shapes.push_back(std::move(shape));throw std::runtime_error(std::string(error.what())+"; native contour storage retained");}
}
Json terrain_contour_page(Context& c,const Json& args){
    auto region=select(c,args.at("handle"));auto style=integer(args.at("style"),"terrain style"),path=integer(args.value("path",Json(0)),"polygon path"),
        offset=integer(args.value("offset",Json(0)),"vertex offset"),limit=integer(args.value("limit",Json(256)),"vertex limit");
    need(style>=0&&style<=3&&path>=0&&path<=1000000&&offset>=0&&offset<=1000000&&limit>0&&limit<=4096,"Terrain contour page requires style 0..3, valid indices and limit 1..4096");
#include "contour_pins.inc"
    auto shape=region_paint_shape(c,region.object,int32_t(style));Json result;std::string failure;
    try{auto paths=paint_paths(c,uintptr_t(shape->bytes.data()));auto total=paint_span(paths,24,1000000);need(size_t(path)<total||(!total&&path==0),"Terrain polygon path is absent");
        PaintView vertices{};if(total)vertices=c.read<PaintView>(paths.begin+size_t(path)*24);auto count=paint_span(vertices,16,1000000);need(size_t(offset)<=count,"Terrain vertex offset exceeds its polygon path");
        auto size=std::min<size_t>(size_t(limit),count-size_t(offset));std::vector<PaintPoint> values(size);if(size)c.read_bytes(vertices.begin+size_t(offset)*16,values.data(),size*16);Json points=Json::array();
        for(auto value:values)points.push_back({{"x",value.x},{"z",value.z}});
        result={{"handle",args.at("handle")},{"style",style},{"path",path},{"total_paths",total},{"total_vertices",count},{"offset",offset},{"points",points},{"next_offset",size_t(offset)+size},{"complete",size_t(offset)+size==count}};
    }catch(const std::exception& error){failure=error.what();}
    try{destroy_paint_shape(c,shape);}catch(const std::exception& error){if(!failure.empty())failure+="; ";failure+=error.what();}
    need(failure.empty(),failure);return result;
}
using PaintStamp=paint_model::Stamp;
std::unique_ptr<PaintShape> make_paint_shape(Context& c,const PaintStamp& stamp){
    need(uncertain_shapes.size()<16,"Terrain native polygon retention budget exhausted");auto shape=std::make_unique<PaintShape>();std::vector<PaintView> paths;
    for(const auto& points:stamp.paths){auto begin=uintptr_t(points.data());paths.push_back({begin,begin+points.size()*16,begin+points.size()*16});}
    auto begin=uintptr_t(paths.data());PaintView view{begin,begin+paths.size()*24,begin+paths.size()*24};shape->started=true;
    try{paint_native([&]{paint_call<void>(c,0xf1810,shape->bytes.data());shape->constructed=true;auto data=c.read<uintptr_t>(uintptr_t(shape->bytes.data())+0x18);
            need(data!=0,"Native terrain polygon constructor returned no data");paint_call<void>(c,0xf7ed0,reinterpret_cast<void*>(data),&view);paint_call<void>(c,0xf3b60,shape->bytes.data());});
        auto native_paths=paint_paths(c,uintptr_t(shape->bytes.data()));need(paint_span(native_paths,24,128)==stamp.paths.size(),"Native terrain polygon path count differs after assignment");
        for(size_t i=0;i<stamp.paths.size();++i){auto vertices=c.read<PaintView>(native_paths.begin+i*24);need(paint_span(vertices,16,16384)==stamp.paths[i].size(),"Native terrain polygon vertex count differs");
            std::vector<PaintPoint> points(stamp.paths[i].size());c.read_bytes(vertices.begin,points.data(),points.size()*16);need(points==stamp.paths[i],"Native terrain polygon assignment changed requested coordinates");}
        return shape;
    }catch(const std::exception& error){auto cause=std::string(error.what());try{destroy_paint_shape(c,shape);}catch(const std::exception& cleanup){cause+="; ";cause+=cleanup.what();}throw std::runtime_error(cause);}
}
Json paint_terrain(Context& c,const Json& args,bool& attempted){
    c.require_mutation();require_root_view(c);need(uncertain_paint.size()<16,"Terrain native brush retention budget exhausted");auto region=select(c,args.at("handle"));
    auto style=integer(args.at("style"),"terrain style");need(style>=0&&style<=3,"Terrain style must be 0..3");auto stamps=paint_model::parse(args.at("stamps"));
    auto label=args.value("label",std::string("Paint terrain"));need(!label.empty()&&label.size()<=256&&label.find('\0')==std::string::npos,"Terrain edit label requires 1..256 bytes without NUL");
#include "paint_pins.inc"
    auto history=height_history(c),money=height_ledger(),previous=state(c,region);std::array<std::unique_ptr<PaintShape>,4> before,after;
    auto receiver=std::make_unique<PaintReceiver>();auto brush=uintptr_t(receiver->bytes.data());uintptr_t graph=0;int32_t cost=-1;bool changed=false;std::string failure;
    try{
        for(int32_t i=0;i<4;++i)before[size_t(i)]=region_paint_shape(c,region.object,i);
        for(const auto& stamp:stamps)receiver->shapes.push_back(make_paint_shape(c,stamp));
        receiver->started=true;attempted=true;paint_native([&]{paint_call<void>(c,0x5e0ed0,receiver->bytes.data());receiver->constructed=true;
            *reinterpret_cast<uintptr_t*>(brush+0x218)=region.object;*reinterpret_cast<int32_t*>(brush+0x150)=int32_t(style);*reinterpret_cast<float*>(brush+0x664)=0;
            Text text{label.data(),label.size()};paint_call<void>(c,0x5e03b0,receiver->bytes.data(),&text);graph=c.read<uintptr_t>(brush+0x658);cost=paint_call<int32_t>(c,0x5dfdf0,receiver->bytes.data());});
        need(graph!=0&&cost==0,"Native terrain StartEdit graph or zero-cost contract differs");
        for(size_t i=0;i<stamps.size();++i){paint_native([&]{std::memcpy(reinterpret_cast<void*>(brush+0x208),stamps[i].position.data(),12);
                *reinterpret_cast<float*>(brush+0x660)=stamps[i].radius;paint_call<void>(c,0x5dfe70,receiver->bytes.data(),receiver->shapes[i]->bytes.data());});}
        paint_native([&]{paint_call<void>(c,0x5e0c20,receiver->bytes.data(),static_cast<void*>(nullptr));receiver->finished=true;});
        need(!c.read<uintptr_t>(brush+0x658)&&!c.read<uintptr_t>(brush+0x670),"Native terrain completion retained its edit or graph");
        auto now=height_history(c);need(now.at("manager")==history.at("manager")&&!now.at("undo").empty()&&now.at("undo").back()==graph&&now.at("redo").empty(),"Terrain native Undo owner or graph differs");
        auto expected=history.at("undo");expected.push_back(graph);if(expected.size()>20)expected.erase(expected.begin());need(now.at("undo")==expected,"Terrain native Undo append or trim differs");
        ap(c,graph,0x16e32d0);ap(c,graph+0x18,0x16e2dc0);ap(c,graph+0x48,0x1748c50);
        auto count=c.read<int32_t>(graph+0x28),capacity=c.read<int32_t>(graph+0x2c);auto data=c.read<uintptr_t>(graph+0x20);
        need(count>0&&count<=1000000&&capacity>=count&&data&&c.read<uint8_t>(graph+0x10c)==1&&!c.read<int32_t>(graph+0x58),"Terrain native inverse graph is invalid, unlocked or pending deletion");
        size_t found=0;for(int32_t i=0;i<count;++i){auto operation=c.read<uintptr_t>(data+size_t(i)*8);if(c.read<uintptr_t>(operation)!=c.image+0x1723330)continue;
            need(c.read<uintptr_t>(operation+0xb8)==region.object,"Terrain inverse belongs to another Region");
            for(size_t n=0;n<4;++n)need(same_paint_shape(c,uintptr_t(before[n]->bytes.data()),operation+0xc0+n*0x70),"Terrain native inverse does not retain the original contours");++found;}
        need(found==1,"Terrain native Undo requires exactly one contour inverse");
        need(select(c,args.at("handle")).object==region.object&&state(c,region)==previous&&height_ledger()==money,"Terrain stroke changed Region identity, authored configuration or the native ledger");
        for(int32_t i=0;i<4;++i){after[size_t(i)]=region_paint_shape(c,region.object,i);if(!same_paint_shape(c,uintptr_t(before[size_t(i)]->bytes.data()),uintptr_t(after[size_t(i)]->bytes.data())))changed=true;}
    }catch(const std::exception& error){failure=error.what();}
    auto cleanup=[&](const char* operation,auto&& function){try{function();}catch(const std::exception& error){if(!failure.empty())failure+="; ";failure+=std::string(operation)+": "+error.what();}};
    if(receiver->constructed&&receiver->finished)cleanup("Terrain brush destructor",[&]{paint_native([&]{paint_call<void>(c,0x5e0fd0,receiver->bytes.data());});receiver->constructed=false;});
    if(receiver->constructed||receiver->started&&!receiver->finished){if(!failure.empty())failure+="; ";failure+="uncertain native terrain edit receiver retained";uncertain_paint.push_back(std::move(receiver));}
    if(receiver)for(auto& shape:receiver->shapes)cleanup("Terrain stamp cleanup",[&]{destroy_paint_shape(c,shape);});
    for(auto& shape:before)cleanup("Terrain prior contour cleanup",[&]{destroy_paint_shape(c,shape);});for(auto& shape:after)cleanup("Terrain resulting contour cleanup",[&]{destroy_paint_shape(c,shape);});
    need(failure.empty(),failure);return {{"handle",args.at("handle")},{"style",style},{"changed",changed},{"stamps",stamps.size()},{"undo_confirmed",true}};
}
