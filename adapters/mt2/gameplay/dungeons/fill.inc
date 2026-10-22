// Original MIT adapter. ABI evidence: dungeon_fill_native.cpp and
// subscriber_lifecycle/{record_decode.cpp,record_graph.cpp,capture.cpp}.
using detail::FillToken;
using detail::FillRecord;
using detail::fill_records;
enum class RecordOp {Allocate,Construct,Destroy,Free,Count,Attach,Label,String,Type,Integer,Float};
struct RecordCall {RecordOp op;uintptr_t target=0,input=0,result=0;int32_t number=0;float value=0;};
uint32_t record_native(uintptr_t image,RecordCall& c) {
    __try {
        switch(c.op){
        case RecordOp::Allocate:c.result=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0x78);break;
        case RecordOp::Construct:reinterpret_cast<void(*)(uintptr_t)>(image+0x9667c0)(c.target);break;
        case RecordOp::Destroy:reinterpret_cast<void(*)(uintptr_t)>(image+0x9676a0)(c.target);break;
        case RecordOp::Free:reinterpret_cast<void(*)(uintptr_t)>(image+0xa01260)(c.target);break;
        case RecordOp::Count:reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0x964b50)(c.target,c.number);break;
        case RecordOp::Attach:reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x964920)(c.target,c.input);break;
        case RecordOp::Label:reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x96d370)(c.target,c.input);break;
        case RecordOp::String:reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x96d360)(c.target,c.input);break;
        case RecordOp::Type:reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0x96d3a0)(c.target,c.number);break;
        case RecordOp::Integer:reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0x96d380)(c.target,c.number);break;
        case RecordOp::Float:reinterpret_cast<void(*)(uintptr_t,float)>(image+0x96d390)(c.target,c.value);break;
        }return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
void record_call(Context& c,RecordCall call) {seh(record_native(c.image,call),"Fill Record operation");}
uint32_t fill_string_native(uintptr_t image,uintptr_t target,const char* data,size_t size,bool label,uint32_t& cleanup) {
    alignas(8) unsigned char text[32]{};bool constructed=false;uint32_t code=0;
    __try{reinterpret_cast<void(*)(void*,const char*,size_t,uintptr_t)>(image+0x11c0260)(text,data,size,0);constructed=true;
        reinterpret_cast<void(*)(uintptr_t,const void*)>(image+(label?0x96d370:0x96d360))(target,text);
    }__except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(constructed){__try{reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(text);}__except(EXCEPTION_EXECUTE_HANDLER){cleanup=GetExceptionCode();}}return code;
}
void set_fill_token(Context& c,uintptr_t target,const FillToken& token) {
    if(token.type==1 || token.type==2){uint32_t cleanup=0;const auto code=fill_string_native(c.image,target,token.text.data(),token.text.size(),token.type==1,cleanup);need(!code && !cleanup,"Dungeon Fill native text copy failed SEH="+std::to_string(code)+" cleanup="+std::to_string(cleanup));}
    else if(token.type==3){RecordCall op{RecordOp::Float,target};op.value=std::bit_cast<float>(token.bits);record_call(c,op);}
    else {RecordCall op{token.type==4?RecordOp::Integer:RecordOp::Type,target};op.number=token.type==4?std::bit_cast<int32_t>(token.bits):token.type;record_call(c,op);}
}
struct FillOwner {uintptr_t pointer=0;bool constructed=false,unknown=false;};
struct FillLoad {uintptr_t root=0;bool context=false;uint32_t cleanup=0;};
uint32_t fill_load_native(uintptr_t image,FillOwner& owner,FillLoad& receipt) {
    alignas(8) uintptr_t context=0;uint32_t code=0;
    __try {
        owner.pointer=reinterpret_cast<uintptr_t(*)()>(image+0xd96100)();owner.constructed=owner.pointer!=0;
        if(!owner.pointer)return ERROR_NOT_ENOUGH_MEMORY;
        reinterpret_cast<void(*)(void*)>(image+0x89e940)(&context);receipt.context=true;
        reinterpret_cast<void(*)(uintptr_t,uintptr_t,void*)>(image+0xce7af0)(owner.pointer,receipt.root,&context);
    }__except(EXCEPTION_EXECUTE_HANDLER){owner.unknown=true;code=GetExceptionCode();}
    if(receipt.context && !owner.unknown){__try{reinterpret_cast<void(*)(void*)>(image+0x89e480)(&context);}__except(EXCEPTION_EXECUTE_HANDLER){receipt.cleanup=GetExceptionCode();owner.unknown=true;}}return code;
}
uint32_t fill_dispose_native(uintptr_t image,uintptr_t fill) {
    __try{reinterpret_cast<void(*)(uintptr_t)>(image+0x157f70)(fill);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct FillCapture {alignas(8) unsigned char record[0x78]{};alignas(8) unsigned char context[0x10]{};bool record_ready=false,context_ready=false;};
uint32_t fill_save_native(uintptr_t image,uintptr_t fill,FillCapture& capture) {
    __try{reinterpret_cast<void(*)(void*)>(image+0x9667c0)(capture.record);capture.record_ready=true;
        reinterpret_cast<void(*)(void*)>(image+0x89e580)(capture.context);capture.context_ready=true;
        reinterpret_cast<void(*)(uintptr_t,void*)>(image+0x10ee6f0)(fill,capture.context);
        reinterpret_cast<void(*)(uintptr_t,void*,void*)>(image+0x10ee750)(fill,capture.record,capture.context);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t fill_context_dispose(uintptr_t image,FillCapture& capture) {
    __try{reinterpret_cast<void(*)(void*)>(image+0x89e5f0)(capture.context);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
Json fill_saved(Context& c,uintptr_t fill) {
    need(c.read<uintptr_t>(fill)==c.image+0x1743680,"Dungeon detached Fill class differs");FillCapture capture;const auto code=fill_save_native(c.image,fill,capture);std::string cause;Json output=Json::array();
    try {
        seh(code,"Fill Save");const auto table=c.read<uintptr_t>(uintptr_t(capture.context));need(table && !c.read<uint64_t>(table+0x18),"Dungeon Fill detached save contains native object references");
        struct Pending {uintptr_t p;int64_t parent;size_t index;};std::vector<Pending> pending{{uintptr_t(capture.record),-1,0}};std::set<uintptr_t> seen;size_t total_tokens=0,text_bytes=0;
        const auto token=[&](uintptr_t p){const auto type=c.read<int32_t>(p);need(type>=0 && type<=4,"Dungeon Fill saved token is not a scalar");auto text=(type==1 || type==2)?c.text(p+8,8192):std::string{};text_bytes+=text.size();need(text_bytes<=65536,"Dungeon Fill saved text exceeds budget");return Json::array({type,(type==3 || type==4)?uint64_t(c.read<uint32_t>(p+0x28)):uint64_t(0),text});};
        for(size_t i=0;i<pending.size();++i){need(pending.size()<=2048 && seen.insert(pending[i].p).second,"Dungeon Fill saved Record repeats/exceeds budget");const auto row=pending[i];need(!c.read<uint8_t>(row.p+0x70),"Dungeon Fill saved Record is streaming");
            const auto data=c.read<uintptr_t>(row.p+0x38);const auto count=c.read<int32_t>(row.p+0x40),capacity=c.read<int32_t>(row.p+0x44);need(count>=0 && count<=16384 && capacity>=count && (!count || data),"Dungeon Fill saved token array differs");total_tokens+=size_t(count);need(total_tokens<=16384,"Dungeon Fill saved tokens exceed budget");Json tokens=Json::array();for(int32_t n=0;n<count;++n)tokens.push_back(token(data+size_t(n)*0x30));
            output.push_back(Json::array({row.parent,row.index,token(row.p),tokens}));const auto children=array(c,row.p+0x48,2048);for(size_t n=0;n<children.size();++n)pending.push_back({children[n],int64_t(i),n});
        }
        fill_records(output);
    }catch(const std::exception& e){cause=e.what();}
    if(capture.record_ready){RecordCall op{RecordOp::Destroy,uintptr_t(capture.record)};const auto cleanup=record_native(c.image,op);if(cleanup)cause+=(cause.empty()?"":"; ")+std::string("Fill saved Record cleanup SEH=")+std::to_string(cleanup);}
    if(capture.context_ready){const auto cleanup=fill_context_dispose(c.image,capture);if(cleanup)cause+=(cause.empty()?"":"; ")+std::string("Fill SaveContext cleanup SEH=")+std::to_string(cleanup);}
    need(cause.empty(),cause);return output;
}
void verify_fill_code(Context& c) {
    for(uintptr_t rva:{0xa02670,0xa01260,0x9667c0,0x9676a0,0x11c0260,0x11bd1e0,0x964920,0x96d370,0x96d360,0x964b50,0x96d380,0x96d3a0,0x96d390,0xd96100,0x89e940,0xce7af0,0x89e480,0x157f70,0x89e580,0x89e5f0,0x10ee6f0,0x10ee750})c.verify_original(rva);
}
FillOwner create_fill(Context& c,const Json& source) {
    const auto rows=fill_records(source);std::vector<uintptr_t> records,detached;bool decoding_unknown=false;FillOwner owner;std::string cause;
    try {
        for(size_t i=0;i<rows.size();++i){RecordCall alloc{RecordOp::Allocate};decoding_unknown=true;seh(record_native(c.image,alloc),"Fill Record allocate");need(alloc.result,"Dungeon Fill Record allocation returned null");detached.push_back(alloc.result);record_call(c,{RecordOp::Construct,alloc.result});records.push_back(alloc.result);
            set_fill_token(c,alloc.result,rows[i].label);RecordCall resize{RecordOp::Count,alloc.result};resize.number=int32_t(rows[i].tokens.size());record_call(c,resize);
            const auto data=c.read<uintptr_t>(alloc.result+0x38);need(c.read<int32_t>(alloc.result+0x40)==int32_t(rows[i].tokens.size()) && (data || rows[i].tokens.empty()),"Dungeon Fill allocated token count differs");
            for(size_t n=0;n<rows[i].tokens.size();++n)set_fill_token(c,data+n*0x30,rows[i].tokens[n]);
            if(i){record_call(c,{RecordOp::Attach,records[rows[i].parent],alloc.result});std::erase(detached,alloc.result);}decoding_unknown=false;
        }
        FillLoad load{records[0]};const auto code=fill_load_native(c.image,owner,load);seh(code,"detached Fill Load");seh(load.cleanup,"Fill LoadContext cleanup");need(owner.constructed,"Dungeon detached Fill allocation is absent");need(fill_saved(c,owner.pointer)==source,"Dungeon detached Fill load/save readback differs");
    }catch(const std::exception& e){cause=e.what();}
    if(!decoding_unknown){for(auto p:detached){RecordCall op{RecordOp::Destroy,p};const auto destroy=record_native(c.image,op);if(destroy){cause+=(cause.empty()?"":"; ")+std::string("Fill Record destruction SEH=")+std::to_string(destroy);break;}op.op=RecordOp::Free;const auto release=record_native(c.image,op);if(release){cause+=(cause.empty()?"":"; ")+std::string("Fill Record free SEH=")+std::to_string(release);break;}}}
    else cause+="; partially decoded native Record owners retained without replay";
    if(!cause.empty() && owner.pointer && owner.constructed && !owner.unknown){const auto cleanup=fill_dispose_native(c.image,owner.pointer);if(cleanup)cause+="; detached Fill cleanup SEH="+std::to_string(cleanup);else owner.pointer=0;}
    need(cause.empty(),cause);return owner;
}
