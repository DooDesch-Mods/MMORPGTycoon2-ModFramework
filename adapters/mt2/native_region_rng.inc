// Included inside step_runtime. Restore only the frozen selected native load.
namespace region_rng_load {
using Value=mt2::region_rng::Value;
using Row=mt2::region_rng::Row;
using Constructor=uint64_t(*)(void*);
Constructor original_constructor=nullptr;
Go original_finish=nullptr;
uint64_t constructor(void*);
void finish(void*);
std::mutex gate;
mt2::region_rng::Load receipts;
bool armed=false;
uint32_t mode=0;
uint64_t agreed_seed=0;
std::vector<Row> sealed;
std::string first_error;
void record_failure(const std::string& cause){
    std::lock_guard lock(gate);if(armed&&first_error.empty()){first_error=cause;runtime->log("steps","error",cause);}
}
Value value(uintptr_t region){return {read<std::array<uint64_t,4>>(region+0x268),read<int32_t>(region+0x288)};}
Value initial(int32_t id){return {mt2::determinism::Gate::seed_words(mt2::region_rng::initial_seed(agreed_seed,id)),0};}
Json rows_json(const std::vector<Row>& rows){
    Json out=Json::array();for(const auto& row:rows)out.push_back(Json::array({row.id,row.value.words,row.value.extra}));return out;
}
void arm(const mt2::StepPreloadIdentity& identity,const std::vector<Row>& rows){
    std::lock_guard lock(gate);require(!armed,"Region RNG load already armed");
    std::string cause;
    bool proven=native_routes->prove_framework_hook("framework.step.region-rng-constructor",reinterpret_cast<void*>(&constructor),reinterpret_cast<void*>(original_constructor),cause);
    require(proven,"Region RNG constructor route: "+cause);
    proven=native_routes->prove_framework_hook("framework.step.region-finish",reinterpret_cast<void*>(&finish),reinterpret_cast<void*>(original_finish),cause);
    require(proven,"Region RNG Finish route: "+cause);
    const auto hash=digest(Json::array({identity.region_rng_mode,rows_json(rows)}).dump());std::string expected;
    for(auto byte:identity.region_rng_sha256){expected+="0123456789abcdef"[byte>>4];expected+="0123456789abcdef"[byte&15];}
    require(hash==expected,"Region RNG immutable prepared rows commitment differs");
    receipts.arm(identity.region_rng_mode,rows);mode=identity.region_rng_mode;agreed_seed=*identity.agreed_seed;armed=true;
}
std::map<int32_t,uintptr_t> census(){
    const auto map=read<uintptr_t>(image+0x13df8c0);
    mt2fw::game::region_saved_identity::Index index;std::string cause;
    auto at=[](uintptr_t object,size_t offset,auto& out,std::string&){out=read<std::remove_reference_t<decltype(out)>>(object+offset);return true;};
    const bool captured=mt2fw::game::region_saved_identity::capture(image,map,map+0x1c0,index,at,cause);
    require(captured,"Region RNG root census: "+cause);
    return {index.saved.begin(),index.saved.end()};
}
uint64_t constructor_call(void* rng,uintptr_t caller)try{
    const auto result=original_constructor(rng);
    std::lock_guard lock(gate);
    if(armed&&caller==image+0x357d91){
        const auto receiver=reinterpret_cast<uintptr_t>(rng);require(receiver>0x268,"Region RNG constructor receiver invalid");
        receipts.constructed(receiver-0x268,value(receiver-0x268));
    }
    return result;
}catch(const std::exception& error){record_failure(std::string("Region RNG constructor failed; no replay: ")+error.what());throw;}
catch(...){record_failure("Region RNG constructor raised an unknown exception; no replay");throw;}
__declspec(noinline) uint64_t constructor(void* rng){return constructor_call(rng,reinterpret_cast<uintptr_t>(_ReturnAddress()));}
void finish_call(void* object,uintptr_t caller)try{
    const auto region=reinterpret_cast<uintptr_t>(object);bool observed=false;int32_t id=-1;
    {std::lock_guard lock(gate);if(armed&&caller==image+0x333a03){
        require(first_error.empty(),first_error);
        const auto owners=census();receipts.census(owners,initial);
        id=read<int32_t>(region+0x834);const auto expected=receipts.attempt(id,region,value(region));
        if(mode==1)original_seed(reinterpret_cast<void*>(region+0x268),mt2::region_rng::initial_seed(agreed_seed,id));
        else {write(region+0x268,expected.words);write(region+0x288,expected.extra);}
        receipts.confirm(id,region,value(region));observed=true;
    }}
    original_finish(object);
    if(observed){std::lock_guard lock(gate);require(read<uintptr_t>(region+0x160)==region+0x268,"Region RNG grid receiver differs after root Finish");receipts.unchanged(id,region,value(region));}
}catch(const std::exception& error){record_failure(std::string("Region RNG root Finish failed; no replay: ")+error.what());throw;}
catch(...){record_failure("Region RNG root Finish raised an unknown exception; no replay");throw;}
__declspec(noinline) void finish(void* object){finish_call(object,reinterpret_cast<uintptr_t>(_ReturnAddress()));}
Json seal(){
    std::lock_guard lock(gate);require(first_error.empty(),first_error);require(armed,"Region RNG bootstrap lacks armed restore");
    sealed=receipts.seal(census(),[](uintptr_t region){require(read<uintptr_t>(region+0x160)==region+0x268,"Region RNG grid receiver differs at bootstrap");return value(region);});armed=false;
    return {{"mode",mode},{"rows",rows_json(sealed)}};
}
void cancel(){std::lock_guard lock(gate);armed=false;}
void reset(){std::lock_guard lock(gate);require(!armed,"Region RNG reset retains an armed load");receipts={};sealed.clear();mode=0;agreed_seed=0;first_error.clear();}
void install(){
    native_routes->catalog("framework.step.region-rng-constructor",0x9fbfa0,true);
    native_routes->catalog("framework.step.region-finish",0x353bd0,true);
    native_services->reserve("framework.step.region-rng-constructor");
    native_services->reserve("framework.step.region-finish");
    for(const auto& [offset,size,sha]:std::array<std::tuple<uintptr_t,size_t,const char*>,4>{{
        {0x9fbfa0,224,"d47d25c1590b15ecd338ab68eba277eb47523426a9eb2252af5e761e5c4a56a9"},
        {0x353bd0,1824,"ef149471e855888b7bd8321f347b6292bf81c89144e996d78fde5949ca224f28"},
        {0x357ce0,1952,"b8c9bc9b510fba38bf161bbf51ab4778692d5c9aad2ef826b9b023229990e58c"},
        {0x332610,9072,"5c06a00af14840825b6381b1c4f0cffb4f9245475f113fd5b6fc33a46b7aa51a"}}}){
        std::string body(size,'\0');SIZE_T copied=0;
        require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+offset),body.data(),size,&copied)&&copied==size&&digest(body)==sha,
            "Region RNG original load/Finish/constructor/Map body differs");
    }
    hook(reinterpret_cast<void*>(image+0x9fbfa0),reinterpret_cast<void*>(&constructor),reinterpret_cast<void**>(&original_constructor));
    hook(reinterpret_cast<void*>(image+0x353bd0),reinterpret_cast<void*>(&finish),reinterpret_cast<void**>(&original_finish));
}
}
