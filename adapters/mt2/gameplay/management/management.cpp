#include "management.h"
#include "model.h"
#include <Windows.h>
#include <intrin.h>
#include <set>
#include <numeric>
#include <cstring>

// Original Framework wrappers. Native ABI/layout facts are documented in
// management/provenance.md; no multiplayer session or transport code is used.
namespace mt2fw::mt2::gameplay::management {
namespace {
void require(bool condition,const std::string& cause){if(!condition)throw std::runtime_error(cause);}
void typed(Context& c,uintptr_t object,uintptr_t table){require(object && c.read<uintptr_t>(object)==c.image+table,"Management native class differs");}
uintptr_t owner(Context& c,uint64_t global,uintptr_t table){auto object=c.singleton(global);typed(c,object,table);return object;}
uintptr_t business(Context& c){return owner(c,0x13df220,0x16d97e0);}
uintptr_t software(Context& c){return owner(c,0x13df500,0x1715450);}
uintptr_t manager(Context& c){return owner(c,0x13df4f0,0x1714ed0);}
uintptr_t release(Context& c){auto p=c.read<uintptr_t>(manager(c)+0x38);typed(c,p,0x16c12b0);return p;}
Json handle(Context& c,uintptr_t object){auto uid=c.read<uint64_t>(object+0x18);require(uid!=0,"Management native UID is zero");return {{"epoch",c.epoch},{"uid",std::to_string(uid)}};}
std::vector<uintptr_t> array(Context& c,uintptr_t object,size_t offset,size_t maximum=4096){
    auto data=c.read<uintptr_t>(object+offset);auto count=c.read<int32_t>(object+offset+8),capacity=c.read<int32_t>(object+offset+12);
    require(count>=0 && size_t(count)<=maximum && capacity>=count && capacity<=1000000 && (!capacity || data) && data<=UINTPTR_MAX-size_t(capacity)*8,"Management native array extent invalid");
    std::vector<uintptr_t> out;std::set<uintptr_t> unique;
    for(int32_t n=0;n<count;++n){auto p=c.read<uintptr_t>(data+size_t(n)*8);require(p && unique.insert(p).second,"Management native array null or duplicate owner");out.push_back(p);}return out;
}
Json fees(Context& c){auto p=business(c);auto a=c.read<int32_t>(p+0x40),b=c.read<int32_t>(p+0x44);require(a>=0&&b>=0,"Management native fees invalid");return {{"purchase",a},{"subscription",b}};}
Json loans(Context& c){
    Json out={{"loans",Json::array()},{"offers",Json::array()}};auto p=business(c);auto analytics=owner(c,0x13df280,0x16dd3b0);auto level=c.read<int32_t>(analytics+0x44);
    for(bool offer:{false,true}){std::set<std::string> keys;for(auto row:array(c,p,offer?0xc8:0x98,1024)){
        typed(c,row,offer?0x16df370:0x17438e0);auto key=c.text(row+0x18,256);require(keys.insert(key).second,"Management loan key duplicated");
        Json value={{"key",key},{"name",c.text(row+0x38,2048)},{"description",c.text(row+0x58,2048)},{"amount",c.read<int32_t>(row+0x78)}};
        if(offer){auto eligibility=c.read<int32_t>(row+0x84);value["repayment"]=c.read<int32_t>(row+0x7c);value["days"]=c.read<int32_t>(row+0x80);value["eligibility"]=eligibility;value["available"]=level>=eligibility;}
        else {value["daily_principal"]=c.read<int32_t>(row+0x7c);value["daily_interest"]=c.read<int32_t>(row+0x80);value["days"]=c.read<int32_t>(row+0x84);}
        out[offer?"offers":"loans"].push_back(std::move(value));
    }}return out;
}
Json staff(Context& c){auto p=business(c),lounge=owner(c,0x13df5e0,0x171e600);auto count=c.read<int32_t>(p+0x50),employed=c.read<int32_t>(lounge+0xd0);
    require(count>=0&&count<=50&&employed>=0&&employed<=50,"Management GM count outside native bounds");
    Json rows=Json::array();auto objects=array(c,lounge,0x40,50);require(objects.empty()||objects.size()==50,"Management GM pool must contain fifty fixed slots");
    for(size_t n=0;n<objects.size();++n){auto gm=objects[n];typed(c,gm,0x16e5070);auto slot=c.read<int32_t>(gm+0x4ac);auto active=c.read<uint8_t>(gm+0x4c4),busy=c.read<uint8_t>(gm+0x4c5);
        require(slot==int32_t(n)&&active==uint8_t(int32_t(n)<employed)&&busy<=1,"Management GM slot or employment flags differ");
        auto h=handle(c,gm);require(c.resolve(h,0x16e5070)==gm,"Management GM registry owner changed");rows.push_back({{"handle",h},{"slot",slot},{"employed",bool(active)},{"busy",bool(busy)}});}
    return {{"count",count},{"employed",employed},{"staff",rows}};
}
Json statistics(Context& c){auto p=software(c);typed(c,p+0x98,0x1786ef0);typed(c,p+0xb8,0x173a6f0);auto data=c.read<uintptr_t>(p+0x1f8);auto count=c.read<int32_t>(p+0x200),capacity=c.read<int32_t>(p+0x204);require(data&&count>=3&&count<=4096&&capacity>=count&&capacity<=1000000,"Management character slot array extent invalid");return {{"values",c.read<std::array<int32_t,10>>(p+0x31c)},{"spent",c.read<std::array<int32_t,10>>(p+0x350)},{"purchased",c.read<std::array<uint8_t,10>>(p+0x344)},{"free_points",c.read<int32_t>(p+0x37c)},{"purchased_points",c.read<int32_t>(p+0x378)},{"character_slots",c.read<std::array<int32_t,3>>(data)}};}
Json releases(Context& c){auto s=software(c),m=manager(c);auto r=c.read<uintptr_t>(m+0x38);Json out={{"version",c.read<std::array<int32_t,3>>(s+0x88)},{"funds",c.read<int32_t>(m+0xb8)},{"present",bool(r)}};
    if(r){typed(c,r,0x16c12b0);auto custom=c.read<uint8_t>(r+0x100);require(custom<=1,"Management release custom flag invalid");out["planned_version"]=c.read<std::array<int32_t,3>>(r+0x50);out["type"]=c.read<int32_t>(r+0x60);out["state"]=c.read<int32_t>(r+0x64);out["custom"]=bool(custom);Json text=Json::array();for(size_t n=0;n<4;++n)text.push_back(c.text(r+0x80+n*0x20,50000));out["text"]=text;}return out;
}
Json policies(Context& c){auto q=owner(c,0x13df360,0x16fdc60);Json out=Json::array();
    for(size_t n=0;n<2;++n){auto p=q+0x48+n*40;typed(c,p,0x16dab90);Json row=Json::object();for(size_t f=0;f<7;++f){auto value=model::policy_integer(f)?c.read<int32_t>(p+model::policy_offsets[f]):int32_t(c.read<uint8_t>(p+model::policy_offsets[f]));require(value>=0&&value<=model::policy_max(f),"Management GM policy value outside native UI domain");if(model::policy_integer(f))row[model::policy_names[f]]=value;else row[model::policy_names[f]]=bool(value);}out.push_back(row);}return out;
}
Json rules(Context& c){auto s=software(c),d=owner(c,0x13df9e0,0x1786ef0),f=owner(c,0x13df3c0,0x16ffdd0);require(d==s+0x98,"Management design owner is not embedded in current software");typed(c,d+0x170,0x16d9f40);typed(c,d+0x1c0,0x173cd30);auto delay=c.read<uint8_t>(d+0x1dc);auto features=c.read<std::array<uint8_t,2>>(f+0x18);require(delay<=1&&features[0]<=1&&features[1]<=1,"Management rule Boolean invalid");return {{"xp",c.read<std::array<uint32_t,14>>(d+0x188)},{"respawn_seconds",c.read<int32_t>(d+0x1d8)},{"respawn_delay",bool(delay)},{"features",features}};}
bool feature_available(Context& c,int32_t index){auto tree=owner(c,0x13df250,0x16dc400);auto techs=array(c,tree,0x10),records=array(c,software(c),0x280);auto key=index?"parties":"pvpduels";uintptr_t found=0;
    for(auto p:techs)if(c.text(p+0x20,200)==key){require(!found,"Management feature technology key duplicated");found=p;}
    if(!found)return false;auto table=c.read<uintptr_t>(found);require(table==c.image+0x1744c40||table==c.image+0x16dc330||table==c.image+0x16e0ac0||table==c.image+0x16fdb10,"Management feature technology class differs");auto record=c.read<uintptr_t>(found+0x1e8);require(std::count(records.begin(),records.end(),record)==1,"Management feature record outside current software");typed(c,record,0x16e79d0);require(c.read<uintptr_t>(record+0x60)==found&&c.text(record+0x38,200)==key,"Management feature technology reciprocal owner differs");auto flags=c.read<std::array<uint8_t,3>>(record+0x58);for(auto flag:flags)require(flag<=1,"Management feature technology flag invalid");return flags[1]||flags[2];}
Json release_cycle(Context& c){auto r=release(c),m=manager(c);auto celebration=c.read<uintptr_t>(m+0xc0);require(celebration&&c.singleton(0x13df6d0)==celebration&&c.singleton(0x1a38700)==r,"Management release cycle native owners differ");auto phase=c.read<int32_t>(celebration);require(phase>=0&&phase<=2,"Management release cycle phase outside native bounds");return {{"release",releases(c)},{"phase",phase},{"history_count",array(c,m,0x98).size()},{"free_points",c.read<int32_t>(software(c)+0x37c)}};}
Json campaigns(Context& c){auto m=owner(c,0x13df2c0,0x16dfa60);Json rows=Json::array(),costs=Json::array(),awards=Json::array();for(auto p:array(c,m,0x40,512)){typed(c,p,0x1749880);auto good=c.read<uint8_t>(p+0xa4);require(good<=1,"Management award Boolean invalid");awards.push_back({{"name",c.text(p+0x28,256)},{"source",c.text(p+0x60,256)},{"day",c.read<int32_t>(p+0x98)},{"impressions",c.read<int32_t>(p+0x9c)},{"conversions",c.read<int32_t>(p+0xa0)},{"good",bool(good)}});}for(auto p:array(c,m,0x70,4)){
    typed(c,p,0x16e3d50);auto values=c.read<std::array<int32_t,6>>(p+0x50);auto active=c.read<uint8_t>(p+0x68);require(values[0]>=0&&values[0]<=8&&active<=1,"Management campaign type or active flag invalid");rows.push_back({{"name",c.text(p+0x18,256)},{"type",values[0]},{"started_on",values[1]},{"duration",values[2]},{"impressions",values[3]},{"conversions",values[4]},{"hype",values[5]},{"active",bool(active)}});}
    for(size_t n=0;n<9;++n)costs.push_back(c.read<int32_t>(c.image+0x1a39bc0+n*0xd0));return {{"campaigns",rows},{"awards",awards},{"daily_costs",costs},{"positive_buzz",c.read<int32_t>(m+0x98)},{"negative_buzz",c.read<int32_t>(m+0x9c)}};
}
struct Service {uintptr_t building,object;Json row;};
std::vector<Service> services(Context& c,const Json& h){auto b=c.resolve(h,0x16d93c0);std::vector<Service> result;auto objects=array(c,b,0x9d8,1024);
    for(size_t n=0;n<objects.size();++n){auto p=objects[n];typed(c,p,0x1718480);require(c.read<uintptr_t>(p+0x148)==b,"Management service owning building changed");typed(c,p+0x28,0x174ab50);auto price=c.read<std::array<int32_t,2>>(p+0x40);auto sync=c.read<uint8_t>(p+0x48);auto prototype=c.read<uintptr_t>(p+0x178);uint8_t allowed=1;if(prototype){typed(c,prototype,0x173be20);allowed=c.read<uint8_t>(prototype+0x41);}require(price[0]>=0&&price[0]<=1&&price[1]>=0&&sync<=1&&allowed<=1,"Management service price or sync flags invalid");
        auto sh=handle(c,p);require(c.resolve(sh,0x1718480)==p,"Management service registry owner changed");result.push_back({b,p,{{"handle",sh},{"ordinal",n},{"type",c.read<int32_t>(p+0x20)},{"config_key",c.text(p+0x158,256)},{"currency",price[0]},{"price",price[1]},{"synced",bool(sync)},{"allow_sync",bool(allowed)}}});}return result;
}
Json service_rows(Context& c,const Json& args){
    Json configs=Json::object();std::map<uintptr_t,std::string> owners;auto manager=c.read<uintptr_t>(c.image+0x13df840);if(manager){typed(c,manager,0x173fba0);for(auto p:array(c,manager,0x20)){typed(c,p,0x1736660);typed(c,p+0x38,0x174ab50);auto key=c.text(p+0x18,256);require(!configs.contains(key),"Management service configuration key duplicated");auto price=c.read<std::array<int32_t,2>>(p+0x50);require(price[0]>=0&&price[0]<=1&&price[1]>=0,"Management service configuration price outside native domain");configs[key]={{"currency",price[0]},{"price",price[1]}};owners[p]=key;}}
    Json out=Json::array();for(auto& row:services(c,args.at("building"))){auto key=row.row.at("config_key").get<std::string>();auto config=c.read<uintptr_t>(row.object+0x150);if(config)require(owners.contains(config)&&owners.at(config)==key,"Management service config pointer/key outside current manager");row.row["config_manager_present"]=bool(manager);row.row["config"]=configs.contains(key)?configs.at(key):Json();out.push_back(row.row);}return out;
}
struct Ledger {int32_t cash=0;std::array<int32_t,5> record{};std::array<std::array<int32_t,2>,20> lines{};bool operator==(const Ledger&)const=default;};
Ledger ledger(Context& c){auto b=business(c),r=c.read<uintptr_t>(b+0x100);typed(c,r,0x1712c00);require(c.read<int32_t>(r+0x18)==2,"Management current financial record version differs");Ledger out;out.cash=c.read<int32_t>(b+0x38);out.record=c.read<std::array<int32_t,5>>(r+0x1c);for(size_t n=0;n<20;++n){auto p=r+0x30+n*0x48;typed(c,p,0x171d0d0);require(c.read<int32_t>(p+0x20)==int32_t(n),"Management financial category index differs");out.lines[n]=c.read<std::array<int32_t,2>>(p+0x18);}return out;}
std::string ledger_difference(const Ledger& expected,const Ledger& actual){auto field=[](const std::string& name,int32_t a,int32_t b){return name+" required="+std::to_string(a)+" measured="+std::to_string(b);};if(expected.cash!=actual.cash)return field("cash",expected.cash,actual.cash);for(size_t n=0;n<5;++n)if(expected.record[n]!=actual.record[n])return field("record["+std::to_string(n)+"]",expected.record[n],actual.record[n]);for(size_t n=0;n<20;++n)for(size_t f=0;f<2;++f)if(expected.lines[n][f]!=actual.lines[n][f])return field("category["+std::to_string(n)+"]["+std::to_string(f)+"]",expected.lines[n][f],actual.lines[n][f]);return "unchanged";}
void add(int32_t& value,int64_t delta){auto n=int64_t(value)+delta;require(n>=INT32_MIN&&n<=INT32_MAX,"Management financial value overflows int32");value=int32_t(n);}
template<class F>Json stable(F&& read){auto before=read(),after=read();require(before==after,"Management native values changed during double read");return after;}
enum class Call {Void,IntVoid,IntBool,StringBool,ServicePrice,ServiceSync,Policy,RespawnSeconds,RespawnDelay,XP,SetType,StringAssign,Custom,CampaignStart,CampaignAbort,SpendStatistics,Feature,ReleaseShow,Buzz,Award};
struct Invocation {Call kind;uintptr_t image=0,target=0,rva=0;int32_t value=0,index=0,before=0;const char* text=nullptr;size_t length=0;const void* data=nullptr;bool accepted=false,constructed=false,cleaned=false;int64_t amount=0;int32_t category=-1;bool subtract=false;const char* second_text=nullptr;size_t second_length=0;uintptr_t allocated=0;};
uint32_t publish_script(uintptr_t image) noexcept {alignas(8) unsigned char record[0x78]{};bool constructed=false;uint32_t error=0;
    __try {reinterpret_cast<void(*)(void*,const char*)>(image+0x967370)(record,"MessageTo Mode LevelUp");constructed=true;reinterpret_cast<void(*)(void*)>(image+0x4fbff0)(record);}__except(EXCEPTION_EXECUTE_HANDLER){error=GetExceptionCode();}
    if(constructed){__try{reinterpret_cast<void(*)(void*)>(image+0x9676a0)(record);}__except(EXCEPTION_EXECUTE_HANDLER){if(!error)error=GetExceptionCode();}}return error;
}
uint32_t invoke(Invocation* v) noexcept {
    struct String {const char* data;size_t size;unsigned char rest[16];};
    const String text{v->text,v->length,{}};alignas(8) unsigned char value[40]{};uint32_t error=0;
    __try {auto target=reinterpret_cast<void*>(v->target);switch(v->kind){
    case Call::Void:reinterpret_cast<void(*)(void*)>(v->image+v->rva)(target);v->accepted=true;break;
    case Call::IntVoid:reinterpret_cast<void(*)(void*,int32_t)>(v->image+v->rva)(target,v->value);v->accepted=true;break;
    case Call::IntBool:v->accepted=reinterpret_cast<bool(*)(void*,int32_t)>(v->image+v->rva)(target,v->value);break;
    case Call::StringBool:v->accepted=reinterpret_cast<bool(*)(void*,const void*)>(v->image+v->rva)(target,&text);break;
    case Call::ServiceSync:reinterpret_cast<void(*)(void*,bool)>(v->image+0x245000)(target,v->value!=0);v->accepted=true;break;
    case Call::ServicePrice:reinterpret_cast<void(*)(void*,int32_t,int32_t)>(v->image+0x86d260)(value,v->index,v->value);v->constructed=true;reinterpret_cast<void(*)(void*,const void*)>(v->image+0x244f50)(target,value);v->accepted=true;break;
    case Call::Policy:{auto address=v->target+model::policy_offsets[size_t(v->index)];bool stored=model::policy_integer(size_t(v->index))?InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address),v->value,v->before)==v->before:uint8_t(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address),char(v->value),char(v->before)))==uint8_t(v->before);if(!stored){error=ERROR_INVALID_DATA;break;}reinterpret_cast<void(*)(void*,const void*)>(v->image+0x3af510)(target,&text);v->accepted=true;break;}
    case Call::RespawnSeconds:case Call::RespawnDelay:{bool stored=v->kind==Call::RespawnSeconds?InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(v->target+0x1d8),v->value,v->before)==v->before:uint8_t(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(v->target+0x1dc),char(v->value),char(v->before)))==uint8_t(v->before);if(!stored){error=ERROR_INVALID_DATA;break;}reinterpret_cast<void(*)(void*,const void*)>(v->image+0xb63030)(reinterpret_cast<void*>(v->target+0x1c0),&text);v->accepted=true;break;}
    case Call::XP:{auto words=static_cast<const uint32_t*>(v->data);for(size_t n=0;n<14;++n)if(uint32_t(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(v->target+0x188+n*4),LONG(words[n+14]),LONG(words[n])))!=words[n]){error=ERROR_INVALID_DATA;break;}v->accepted=!error;break;}
    case Call::SetType:{auto version=static_cast<const int32_t*>(v->data);reinterpret_cast<void(*)(void*,int32_t,int32_t,int32_t)>(v->image+0x525690)(value,version[0],version[1],version[2]);v->constructed=true;reinterpret_cast<void(*)(void*,const void*,int32_t)>(v->image+0x4de390)(target,value,v->value);v->accepted=true;break;}
    case Call::StringAssign:reinterpret_cast<void(*)(void*,const void*)>(v->image+0x11bfce0)(target,&text);v->accepted=true;break;
    case Call::Custom:v->accepted=uint8_t(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(v->target+0x100),char(v->value),char(v->before)))==uint8_t(v->before);break;
    case Call::CampaignStart:{auto daily=reinterpret_cast<int32_t(*)(int32_t)>(v->image+0x516510)(v->value);if(daily<0||daily>INT32_MAX/7||int64_t(daily)*7!=v->amount){error=ERROR_INVALID_DATA;break;}if(!reinterpret_cast<bool(*)(void*,int32_t)>(v->image+0x51c5e0)(target,v->value))break;if(!reinterpret_cast<bool(*)(void*,int32_t,int32_t)>(v->image+0x54d80)(reinterpret_cast<void*>(v->rva),daily*7,17))break;v->accepted=reinterpret_cast<bool(*)(void*,int32_t,int32_t)>(v->image+0x51c640)(target,v->value,7);break;}
    case Call::CampaignAbort:v->accepted=reinterpret_cast<bool(*)(void*,int32_t)>(v->image+0x51c510)(target,v->value);break;
    case Call::SpendStatistics:{auto allocations=static_cast<const int32_t*>(v->data);for(int32_t n=0;n<10;++n)reinterpret_cast<void(*)(void*,int32_t,int32_t)>(v->image+0x2dc900)(target,n,allocations[n]);v->accepted=true;break;}
    case Call::Feature:reinterpret_cast<void(*)(void*,int32_t,bool)>(v->image+0x46fb70)(target,v->index,v->value!=0);v->accepted=true;break;
    case Call::ReleaseShow:if(reinterpret_cast<bool(*)(const void*,void*)>(v->image+0x4def00)(reinterpret_cast<void*>(v->rva),nullptr)){if(v->index==4)error=publish_script(v->image);else reinterpret_cast<void(*)(void*)>(v->image+0x4eb360)(target);v->accepted=!error;}break;
    case Call::Buzz:{auto words=static_cast<const int32_t*>(v->data);for(size_t n=0;n<2;++n)if(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(v->target+0x98+n*4),words[n+2],words[n])!=words[n]){error=ERROR_INVALID_DATA;break;}v->accepted=!error;break;}
    case Call::Award:{auto object=reinterpret_cast<unsigned char*>(reinterpret_cast<void*(*)(size_t)>(v->image+0xa02670)(0xa8));v->allocated=uintptr_t(object);if(!object){error=ERROR_OUTOFMEMORY;break;}reinterpret_cast<void(*)(void*)>(v->image+0x51a9b0)(object);const String source{v->second_text,v->second_length,{}};reinterpret_cast<void(*)(void*,const void*)>(v->image+0x11bfce0)(object+0x28,&text);reinterpret_cast<void(*)(void*,const void*)>(v->image+0x11bfce0)(object+0x60,&source);auto fields=static_cast<const int32_t*>(v->data);for(size_t n=0;n<3;++n)*reinterpret_cast<int32_t*>(object+0x98+n*4)=fields[n];object[0xa4]=uint8_t(fields[3]);reinterpret_cast<void(*)(void*,void*)>(v->image+0xb5d8d0)(reinterpret_cast<void*>(v->target+0x38),object);v->accepted=true;break;}
    }} __except(EXCEPTION_EXECUTE_HANDLER){error=GetExceptionCode();}
    if(v->constructed){__try{reinterpret_cast<void(*)(void*)>(v->image+0x89da30)(value);v->cleaned=true;}__except(EXCEPTION_EXECUTE_HANDLER){if(!error)error=GetExceptionCode();}}
    return error;
}
void execute(Context& c,Invocation& call,bool& attempted){
    switch(call.kind){
    case Call::Void:case Call::IntVoid:case Call::IntBool:case Call::StringBool:c.verify_original(call.rva);break;
    case Call::ServicePrice:for(auto rva:{0x86d260,0x244f50,0x89da30})c.verify_original(rva);break;
    case Call::ServiceSync:c.verify_original(0x245000);break;
    case Call::Policy:c.verify_original(0x3af510);break;
    case Call::RespawnSeconds:case Call::RespawnDelay:c.verify_original(0xb63030,16);break;
    case Call::SetType:for(auto rva:{0x525690,0x4de390,0x89da30})c.verify_original(rva);break;
    case Call::StringAssign:c.verify_original(0x11bfce0);break;
    case Call::CampaignStart:for(auto rva:{0x516510,0x51c5e0,0x54d80,0x51c640})c.verify_original(rva);break;
    case Call::CampaignAbort:c.verify_original(0x51c510);break;
    case Call::SpendStatistics:c.verify_original(0x2dc900);break;
    case Call::Feature:c.verify_original(0x46fb70);break;
    case Call::ReleaseShow:c.verify_original(0x4def00);c.verify_original(0x4eb360);for(auto rva:{0x967370,0x4fbff0,0x9676a0})c.verify_original(rva);break;
    case Call::Award:for(auto rva:{0xa02670,0x51a9b0,0x11bfce0,0xb5d8d0})c.verify_original(rva);break;
    case Call::XP:case Call::Custom:case Call::Buzz:break;
    }
    auto first=ledger(c),expected=ledger(c);require(first==expected,"Management financial state changed before native mutation");
    if(call.category>=0){require(call.category<20&&call.amount>=0&&call.amount<=INT32_MAX&&(!call.subtract||expected.cash>=call.amount),"Management native payment cannot be afforded or exceeds int32");auto delta=call.subtract?-call.amount:call.amount;add(expected.cash,delta);add(expected.record[1],delta);add(expected.record[call.subtract?4:3],call.amount);add(expected.lines[size_t(call.category)][0],1);add(expected.lines[size_t(call.category)][1],delta);}
    call.image=c.image;attempted=true;auto exception=invoke(&call);require(!exception&&call.accepted&&(!call.constructed||call.cleaned),"Management native operation failed SEH="+std::to_string(exception)+" accepted="+std::to_string(call.accepted)+(call.allocated?"; native award ownership uncertain, allocation retained":"")+"; no replay");
    c.require_world();auto actual=ledger(c);require(actual==expected,"Management native full financial readback differs: "+ledger_difference(expected,actual)+"; no replay");
}
void pin(Context& c,uint64_t rva,size_t size,const char* hash){c.verify_hash(rva,size,hash);}
struct alignas(8) DefaultText {unsigned char localized[64]{};unsigned char text[32]{};};
uint32_t default_step(uintptr_t image,uintptr_t r,bool announcement,DefaultText* out,int step) noexcept {
    __try {switch(step){
    case 0:reinterpret_cast<void(*)(void*,const void*)>(image+(announcement?0x4e0b00:0x4e0d40))(out->localized,reinterpret_cast<void*>(r));break;
    case 1:reinterpret_cast<void(*)(void*,const void*)>(image+0xa18e60)(out->text,out->localized);break;
    case 2:reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(out->text);break;
    case 3:reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(out->localized+32);break;
    case 4:reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(out->localized);break;
    }return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
std::string default_text(Context& c,uintptr_t r,bool announcement,bool& attempted){
    pin(c,announcement?0x4e0b00:0x4e0d40,announcement?512:5536,announcement?"f028381f5825ae62add0b1e4f1c70d9ecdff0b0b6399b479d0a93ccc94c5cbce":"4968958f106f838382549c42d61d7e3684a00ea90bcdc6c5ac50532435706dee");
    pin(c,0xa18e60,176,"3ade962fa191a303d5b60871adef0a5ac791780c1eab4214b49761cc7865b5d8");pin(c,0x11bd1e0,32,"4a8cc79ce002b866698caded8eaea996814366638d34ce5cdf3ccb0394dc95e5");pin(c,0x11b2b00,176,"e6155e49c38d380ed513ce15ead8505762a5e50d6f1bde2e9985ec668fb95524");
    DefaultText out;uint32_t completed=0;std::string result,cause;attempted=true;
    for(int step=0;step<2;++step){auto code=default_step(c.image,r,announcement,&out,step);if(code){cause="Management release default native getter/formatter failed SEH="+std::to_string(code);break;}completed|=1u<<step;}
    if(cause.empty())try{result=c.text(uintptr_t(out.text),50000);}catch(const std::exception& error){cause=error.what();}
    for(int step=2;step<5;++step){if((step==2&&!(completed&2))||(step>=3&&!(completed&1)))continue;auto code=default_step(c.image,r,announcement,&out,step);if(code){if(!cause.empty())cause+="; ";cause+="Management release default cleanup failed step="+std::to_string(step)+" SEH="+std::to_string(code);}}
    require(cause.empty(),cause);return result;
}
bool complete(bool ok,Json before,Json after,Json& result,std::string& cause){result={{"before",std::move(before)},{"after",std::move(after)}};if(!ok){cause="Management native readback differs from intended effect; no replay";return false;}cause.clear();return true;}
}

std::optional<Json> query(Context& c,const std::string& op,const Json& args){
    if(op!="management.fees"&&op!="management.loans"&&op!="management.staff"&&op!="management.statistics"&&op!="management.release"&&op!="management.policies"&&op!="management.rules"&&op!="management.campaigns"&&op!="management.services"&&op!="management.release_cycle")return std::nullopt;
    c.require_world();
    if(op=="management.fees")return stable([&]{return fees(c);});
    if(op=="management.loans")return stable([&]{return loans(c);});
    if(op=="management.staff")return stable([&]{return staff(c);});
    if(op=="management.statistics")return stable([&]{return statistics(c);});
    if(op=="management.release")return stable([&]{return releases(c);});
    if(op=="management.policies")return stable([&]{return policies(c);});
    if(op=="management.rules")return stable([&]{return rules(c);});
    if(op=="management.campaigns")return stable([&]{return campaigns(c);});
    if(op=="management.release_cycle")return stable([&]{return release_cycle(c);});
    return stable([&]{return service_rows(c,args);});
}

bool action(Context& c,const std::string& op,const Json& args,bool& attempted,Json& result,std::string& cause){
    c.require_world();
    if(op=="management.set_purchase_fee"||op=="management.set_subscription_fee"){
        const bool purchase=op=="management.set_purchase_fee";auto value=model::integer(args.at("value"));auto before=*query(c,"management.fees",{});auto expected=before;expected[purchase?"purchase":"subscription"]=value;
        if(purchase)c.verify(0x54ce0,{0x89,0x51,0x40,0xc3});else c.verify(0x54cf0,{0x89,0x51,0x44,0xc3});
        Invocation call{Call::IntVoid};call.target=business(c);call.rva=purchase?0x54ce0:0x54cf0;call.value=value;execute(c,call,attempted);auto after=*query(c,"management.fees",{});return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.hire_gm"||op=="management.fire_gm"){
        bool hire=op=="management.hire_gm";auto before=*query(c,"management.staff",{});int32_t count=before.at("count"),employed=before.at("employed");require(hire?(count<50&&employed<50):(count>0&&employed>0),"Management GM employment prerequisite failed");
        auto rva=hire?0x56940:0x56a70;pin(c,rva,304,hire?"7c7d686227b2264218cc445425c6f390458d1aaebaba7312e71e42e32dce95c9":"dc180bfe9808d320f2c64b5e02d4a7aad5aaca55fb8768b150ebdae2c568e20f");
        Invocation call{Call::Void};call.target=business(c);call.rva=rva;execute(c,call,attempted);auto after=*query(c,"management.staff",{});auto delta=hire?1:-1;return complete(after.at("count")==count+delta&&after.at("employed")==employed+delta,before,after,result,cause);
    }
    if(op=="management.take_loan"||op=="management.repay_loan"){
        bool take=op=="management.take_loan";auto key=model::text(args.at("key"),256);require(!key.empty(),"Management loan key is empty");auto before=*query(c,"management.loans",{});auto rows=before.at(take?"offers":"loans");auto found=std::find_if(rows.begin(),rows.end(),[&](const Json& row){return row.at("key")==key;});require(found!=rows.end(),"Management loan key absent");if(take)require(found->at("available").get<bool>(),"Management loan offer is unavailable");
        auto rva=take?0x555a0:0x57620;
        // Full-body pins for these two functions are generated from the retained native facts.
        #include "loan_pins.inc"
        auto expected=before;Json retained=Json::array();for(const auto& row:rows)if(row["key"]!=key)retained.push_back(row);expected[take?"offers":"loans"]=retained;
        if(take){auto amount=found->at("amount").get<int32_t>(),repayment=found->at("repayment").get<int32_t>(),days=found->at("days").get<int32_t>();require(amount>0&&repayment>=amount&&days>0,"Management loan offer payment schedule invalid");expected["loans"].push_back({{"key",key},{"name",found->at("name")},{"description",found->at("description")},{"amount",amount},{"daily_principal",amount/days},{"daily_interest",(repayment-amount)/days},{"days",days}});}
        Invocation call{Call::StringBool};call.target=business(c);call.rva=rva;call.text=key.data();call.length=key.size();call.amount=found->at("amount").get<int32_t>();call.category=take?17:15;call.subtract=!take;execute(c,call,attempted);auto after=*query(c,"management.loans",{});if(!take)expected["offers"]=after["offers"];
        return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.allocate_release_funds"){
        auto value=model::integer(args.at("value"));auto before=*query(c,"management.release",{});pin(c,0x4ec880,128,"fc03d6d85d9c83f1acf14b2c8cdef6570663cc902f22c311987e4170c5baeb1b");Invocation call{Call::IntBool};call.target=manager(c);call.rva=0x4ec880;call.value=value;call.amount=int64_t(value)-before.at("funds").get<int32_t>();call.subtract=call.amount>0;call.amount=std::abs(call.amount);call.category=18;execute(c,call,attempted);auto after=*query(c,"management.release",{});auto expected=before;expected["funds"]=value;return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.purchase_statistic"){
        auto index=model::integer(args.at("index"),0,9);auto before=*query(c,"management.statistics",{});require(before.at("purchased")[index]==0,"Management statistic already purchased");pin(c,0x2de2d0,144,"0057c30d5bba210c0105ed3f99d246885541d32e3ca9473da5a31b28d543088e");Invocation call{Call::IntBool};call.target=software(c);call.rva=0x2de2d0;call.value=index;call.amount=int64_t(before.at("values")[index].get<int32_t>())*(index==8?10000:index==9?5000:2000);call.category=5;call.subtract=true;execute(c,call,attempted);auto after=*query(c,"management.statistics",{});auto expected=before;expected["purchased"][index]=1;return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.spend_statistics"){
        auto allocations=model::allocation(args.at("values"));auto before=*query(c,"management.statistics",{}),expected=before;int64_t sum=0;
        for(size_t n=0;n<10;++n){sum+=allocations[n];auto value=before["values"][n].get<int32_t>(),spent=before["spent"][n].get<int32_t>();add(value,allocations[n]);add(spent,allocations[n]);expected["values"][n]=value;expected["spent"][n]=spent;if(allocations[n])expected["purchased"][n]=0;}
        auto free=before["free_points"].get<int32_t>(),purchased=before["purchased_points"].get<int32_t>();require(free>=0&&purchased>=0,"Management statistics point balance invalid");add(purchased,std::max<int64_t>(0,sum-free));expected["free_points"]=std::max<int64_t>(0,int64_t(free)-sum);expected["purchased_points"]=purchased;for(size_t n=0;n<3;++n)expected["character_slots"][n]=expected["values"][n+5];
        Invocation call{Call::SpendStatistics};call.target=software(c);call.data=allocations.data();execute(c,call,attempted);auto after=*query(c,"management.statistics",{});return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.set_feature"){
        auto index=model::integer(args.at("index"),0,1);auto enabled=model::boolean(args.at("enabled"));require(feature_available(c,index),"Management feature technology is not available");auto before=*query(c,"management.rules",{}),expected=before;expected["features"][index]=int32_t(enabled);pin(c,0x46fb70,1392,"32a01881fc7c79b4107041a50953a554bd8e6357bb7a5ab7d7cc0d85e2b147ec");Invocation call{Call::Feature};call.target=owner(c,0x13df3c0,0x16ffdd0);call.index=index;call.value=enabled;execute(c,call,attempted);auto after=*query(c,"management.rules",{});return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.set_policy"){
        auto policy=model::integer(args.at("policy"),0,1);auto field=model::policy_field(args.at("field").get<std::string>());auto value=model::policy_integer(field)?model::integer(args.at("value"),0,model::policy_max(field)):int32_t(model::boolean(args.at("value")));auto before=*query(c,"management.policies",{});auto name=model::policy_names[field];auto old=model::policy_integer(field)?before[policy][name].get<int32_t>():int32_t(before[policy][name].get<bool>());pin(c,0x3af510,1536,"f5418f6e034c57f7c0e482fb9d4f5b119a2a51db9638b05db70263a0c7181a1d");Invocation call{Call::Policy};call.target=owner(c,0x13df360,0x16fdc60)+0x48+size_t(policy)*40;call.index=int32_t(field);call.before=old;call.value=value;call.text=name;call.length=std::strlen(name);execute(c,call,attempted);auto after=*query(c,"management.policies",{});auto expected=before;if(model::policy_integer(field))expected[policy][name]=value;else expected[policy][name]=bool(value);return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.set_respawn_seconds"||op=="management.set_respawn_delay"){
        auto seconds=op=="management.set_respawn_seconds";auto value=seconds?model::integer(args.at("value")):int32_t(model::boolean(args.at("value")));auto before=*query(c,"management.rules",{});auto key=seconds?"respawn_seconds":"respawn_delay";auto old=seconds?before[key].get<int32_t>():int32_t(before[key].get<bool>());pin(c,0xb63030,16,"499f1f307c1cb989f968a6b7fcaec591e1828877223d0b0e7e8e8b76cde8c9ca");Invocation call{seconds?Call::RespawnSeconds:Call::RespawnDelay};call.target=owner(c,0x13df9e0,0x1786ef0);call.before=old;call.value=value;call.text=seconds?"seconds":"delayForToons";call.length=std::strlen(call.text);execute(c,call,attempted);auto after=*query(c,"management.rules",{});auto expected=before;if(seconds)expected[key]=value;else expected[key]=bool(value);return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.set_xp"){
        const auto& input=args.at("values");require(input.is_array()&&input.size()==13,"Management XP requires thirteen editable values");auto before=*query(c,"management.rules",{});std::array<uint32_t,28> values{};for(size_t n=0;n<14;++n)values[n]=before.at("xp")[n].get<uint32_t>();values[14]=values[0];for(size_t n=1;n<14;++n)values[n+14]=uint32_t(model::integer(input[n-1]));require(uint64_t(values[15])+values[16]>0&&uint64_t(values[15])+values[16]<=INT32_MAX,"Management XP base and multiplier sum outside positive int32");Invocation call{Call::XP};call.target=owner(c,0x13df9e0,0x1786ef0);call.data=values.data();execute(c,call,attempted);auto after=*query(c,"management.rules",{});auto expected=before;for(size_t n=0;n<14;++n)expected["xp"][n]=values[n+14];return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.set_release_type"){
        auto type=model::integer(args.at("type"),1,3);auto before=*query(c,"management.release",{});require(before.at("present").get<bool>(),"Management current release absent");auto version=model::next_version(before.at("version").get<std::array<int32_t,3>>(),type);
        #include "release_type_pins.inc"
        Invocation call{Call::SetType};call.target=release(c);call.value=type;call.data=version.data();execute(c,call,attempted);auto after=*query(c,"management.release",{});auto expected=before;expected["type"]=type;expected["planned_version"]=version;return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.reset_release_text"){
        auto before=*query(c,"management.release",{});auto money=ledger(c);auto r=release(c);auto announcement=default_text(c,r,true,attempted),changelog=default_text(c,r,false,attempted);require(*query(c,"management.release",{})==before&&ledger(c)==money,"Management default text formatting changed current world state; no replay");
        pin(c,0xa97f50,352,"ac2729def4e766c43ebfff386da85ec4740dde30d19f30820e47c15797ec1232");pin(c,0xa980b0,352,"3f2b8ed13dc846851a0bc01bea322ad795ac8d4d2885d7a32ebe96b71e64fc7a");Invocation custom{Call::Custom};custom.target=r;custom.before=before["custom"].get<bool>();custom.value=0;execute(c,custom,attempted);for(auto rva:{0xa97f50,0xa980b0}){Invocation reset{Call::Void};reset.target=r;reset.rva=rva;execute(c,reset,attempted);}auto after=*query(c,"management.release",{}),expected=before;expected["text"][2]=announcement;expected["text"][3]=changelog;expected["custom"]=false;return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.enable_release_custom"){
        auto before=*query(c,"management.release",{}),expected=before;Invocation call{Call::Custom};call.target=release(c);call.before=before.at("custom").get<bool>();call.value=1;execute(c,call,attempted);auto after=*query(c,"management.release",{});expected["custom"]=true;return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.show_release"||op=="management.begin_release"||op=="management.abort_release"||op=="management.skip_release"||op=="management.publish_release"){
        auto before=*query(c,"management.release_cycle",{});auto phase=before["phase"].get<int32_t>(),type=before["release"]["type"].get<int32_t>();bool show=op=="management.show_release",begin=op=="management.begin_release",abort=op=="management.abort_release";require(show?phase==0:begin?phase==1:phase==1||phase==2,"Management release command is not valid in the current native phase");
        #include "release_cycle_call_pins.inc"
        Invocation call{show?Call::ReleaseShow:Call::Void};call.target=show?manager(c):c.singleton(0x13df6d0);call.rva=show?release(c):begin?0x4e6eb0:abort?0x4e6d40:0x4e6dc0;call.index=type;execute(c,call,attempted);auto after=*query(c,"management.release_cycle",{});
        bool published=after["release"]["version"]==before["release"]["planned_version"]&&after["history_count"].get<int64_t>()==before["history_count"].get<int64_t>()+1&&after["free_points"].get<int64_t>()==before["free_points"].get<int64_t>()+(type==3?4:2)&&after["phase"]==0;
        bool unchanged=after["release"]==before["release"]&&after["history_count"]==before["history_count"]&&after["free_points"]==before["free_points"];
        bool ok=abort?unchanged&&after["phase"]==0:show?(type==4?published:unchanged&&after["phase"]==1):begin?((type==2||type==3)?unchanged&&after["phase"]==2:published):published;
        return complete(ok,before,after,result,cause);
    }
    if(op=="management.set_release_title"||op=="management.set_release_announcement"||op=="management.set_release_changelog"){
        auto text=model::text(args.at("text"),50000);auto index=op=="management.set_release_title"?0:op=="management.set_release_announcement"?2:3;auto before=*query(c,"management.release",{});auto r=release(c);bool reset=index!=0&&text.empty();
        if(reset){auto money=ledger(c);text=default_text(c,r,index==2,attempted);require(*query(c,"management.release",{})==before&&ledger(c)==money,"Management default formatting changed native state; no replay");pin(c,index==2?0xa980b0:0xa97f50,352,index==2?"3f2b8ed13dc846851a0bc01bea322ad795ac8d4d2885d7a32ebe96b71e64fc7a":"ac2729def4e766c43ebfff386da85ec4740dde30d19f30820e47c15797ec1232");Invocation call{Call::Void};call.target=r;call.rva=index==2?0xa980b0:0xa97f50;execute(c,call,attempted);}
        else {pin(c,0x11bfce0,192,"2dbdfc9aedeb3dd9bae6c3f8e9c4ee9c4185ab937b68f74816e243b07f908c78");Invocation call{Call::StringAssign};call.target=r+0x80+size_t(index)*0x20;call.text=text.data();call.length=text.size();execute(c,call,attempted);if(index==3){Invocation custom{Call::Custom};custom.target=r;custom.before=before.at("custom").get<bool>();custom.value=1;execute(c,custom,attempted);}}
        auto after=*query(c,"management.release",{});auto expected=before;expected["text"][index]=text;if(index==3&&!reset)expected["custom"]=true;return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.set_service_price"||op=="management.set_service_sync"){
        bool price=op=="management.set_service_price";auto ordinal=model::integer(args.at("ordinal"),0,1023);auto all=services(c,args.at("building"));require(size_t(ordinal)<all.size(),"Management service ordinal absent");auto selected=all[size_t(ordinal)];auto before=*query(c,"management.services",args);auto value=price?model::integer(args.at("value")):int32_t(model::boolean(args.at("value")));auto currency=price?model::integer(args.at("currency"),0,1):0;require(price||!value||selected.row.at("allow_sync").get<bool>(),"Management service prototype disables price synchronization");
        require(before[ordinal]["handle"]==selected.row["handle"],"Management service owner changed before mutation");auto expected=before;auto& desired=expected[ordinal];auto key=desired["config_key"].get<std::string>();bool configured=desired["config_manager_present"].get<bool>();
        if(price){desired["currency"]=currency;desired["price"]=value;}else desired["synced"]=bool(value);
        if(configured&&desired["synced"].get<bool>()){
            require(!key.empty(),"Management synchronized service configuration key is empty");auto config=desired["config"];
            if(price||config.is_null())config={{"currency",desired["currency"]},{"price",desired["price"]}};
            else {desired["currency"]=config["currency"];desired["price"]=config["price"];}
            for(auto& row:expected)if(row["config_key"]==key)row["config"]=config;
        }
        if(price){pin(c,0x244f50,176,"d54f74e4bc4380a076b4542f478ba4e2a34e6a2f1c6246d0d7d5b081259b72e5");
            pin(c,0x89da30,256,"c66bc1c211518a01df8b88bfa0ad80d408cb7531e49a8091e0e37dfbe9e3668d");}
        else pin(c,0x245000,128,"7a2f496335333a05ecdd3d25cd5db54f1ebf50a8ea885a39c20c4ed3e39d0663");
        Invocation call{price?Call::ServicePrice:Call::ServiceSync};call.target=selected.object;call.value=value;call.index=currency;execute(c,call,attempted);auto after=*query(c,"management.services",args);return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.start_campaign"||op=="management.abort_campaign"){
        auto type=model::integer(args.at("type"),0,8);auto start=op=="management.start_campaign";auto before=*query(c,"management.campaigns",{});bool exists=std::any_of(before["campaigns"].begin(),before["campaigns"].end(),[&](const Json& row){return row["type"]==type;});require(start?(!exists&&before["campaigns"].size()<4):exists,"Management campaign duplicate, capacity, or absence prerequisite failed");
        if(start){
            #include "campaign_payment_pin.inc"
            pin(c,0x51c640,656,"5c1626817461f4dece5e2b11c6ad90999d8a0f12771359f0999647cdf0372c96");
        }else pin(c,0x51c510,208,"3c37f16172ddd10cf6ce05b7b609efbb35a791cafa737a37f07ba2eb11bfffda");
        Invocation call{start?Call::CampaignStart:Call::CampaignAbort};call.target=owner(c,0x13df2c0,0x16dfa60);call.rva=business(c);call.value=type;if(start){call.amount=int64_t(before["daily_costs"][type].get<int32_t>())*7;call.category=17;call.subtract=true;}execute(c,call,attempted);auto after=*query(c,"management.campaigns",{}),expected=before;bool valid=true;
        if(start){valid=after["campaigns"].size()==before["campaigns"].size()+1;if(valid){auto row=after["campaigns"].back();valid=row["type"]==type&&row["duration"]==7&&row["active"]==true;expected["campaigns"].push_back(row);}}
        else {expected["campaigns"]=Json::array();for(auto row:before["campaigns"])if(row["type"]!=type)expected["campaigns"].push_back(row);}
        return complete(valid&&after==expected,before,after,result,cause);
    }
    if(op=="management.set_buzz"){
        auto positive=model::integer(args.at("positive"),INT32_MIN),negative=model::integer(args.at("negative"),INT32_MIN);auto before=*query(c,"management.campaigns",{}),expected=before;std::array<int32_t,4> values{before["positive_buzz"].get<int32_t>(),before["negative_buzz"].get<int32_t>(),positive,negative};Invocation call{Call::Buzz};call.target=owner(c,0x13df2c0,0x16dfa60);call.data=values.data();execute(c,call,attempted);auto after=*query(c,"management.campaigns",{});expected["positive_buzz"]=positive;expected["negative_buzz"]=negative;return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.purge_expired_awards"){
        auto before=*query(c,"management.campaigns",{}),expected=before;auto day=c.read<int32_t>(owner(c,0x13df9a0,0x17499d0)+0x44);expected["awards"]=Json::array();for(auto row:before["awards"])if(int64_t(row["day"].get<int32_t>())+6>=day)expected["awards"].push_back(row);
        pin(c,0x51b780,1248,"b602bd020e30513fb673a4fa9f4fa76362086a54ac5531bea9aa34756bfbc5a9");Invocation call{Call::Void};call.target=owner(c,0x13df2c0,0x16dfa60);call.rva=0x51b780;execute(c,call,attempted);auto after=*query(c,"management.campaigns",{});return complete(after==expected,before,after,result,cause);
    }
    if(op=="management.add_award"){
        auto name=model::text(args.at("name"),256),source=model::text(args.at("source"),256);auto impressions=model::integer(args.at("impressions")),conversions=model::integer(args.at("conversions"));auto good=model::boolean(args.at("good"));auto day=c.read<int32_t>(owner(c,0x13df9a0,0x17499d0)+0x44);require(day>=0&&day<=INT32_MAX-6,"Management award day is outside native expiry domain");auto before=*query(c,"management.campaigns",{}),expected=before;require(before["awards"].size()<512,"Management award capacity reached");for(const auto& row:before["awards"])require(row["day"]!=day||row["name"]!=name||row["source"]!=source,"Management award day/name/source key duplicated");
        pin(c,0xa02670,80,"75d24a585d71fb3cf549ddf13f41901b6747711875a65117b5d369802ed7e9ae");pin(c,0x51a9b0,208,"0b1b493bfd7d71c6f28183c3933cff43012443b28c234b318be78f48d029178d");pin(c,0xb5d8d0,416,"e7f96ddf7ca57d05ccc14e74103463d751604c8c4d4113292dcbb3918977cd99");pin(c,0x11bfce0,192,"2dbdfc9aedeb3dd9bae6c3f8e9c4ee9c4185ab937b68f74816e243b07f908c78");
        std::array<int32_t,4> values{day,impressions,conversions,int32_t(good)};Invocation call{Call::Award};call.target=owner(c,0x13df2c0,0x16dfa60);call.data=values.data();call.text=name.data();call.length=name.size();call.second_text=source.data();call.second_length=source.size();execute(c,call,attempted);auto after=*query(c,"management.campaigns",{});expected["awards"].push_back({{"name",name},{"source",source},{"day",day},{"impressions",impressions},{"conversions",conversions},{"good",good}});return complete(after==expected,before,after,result,cause);
    }
    throw std::runtime_error("Unsupported management action: "+op);
}
}
