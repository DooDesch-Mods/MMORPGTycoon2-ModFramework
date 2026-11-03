#include "native_step_driver.h"
#include "native_step_pins.h"
#include "native_step_proof.h"
#include "native_step_profile.h"
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

using namespace mt2fw::mt2;
namespace {
int checks=0;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
template<class F>bool raises(F&& fn,const std::string& text){try{fn();return false;}catch(const std::exception& e){return std::string(e.what()).find(text)!=std::string::npos;}}
struct Trace {
    uint64_t epoch=1,next=1;
    bool active=false,registration_fails=false;
    unsigned registered=0,begins=0,ends=0;
    std::vector<std::pair<uint32_t,uint64_t>> stack;
    std::vector<std::string> events;
    MT2FW_MT2_ServiceV1 api{};
    Trace(){
        api.context=this;
        api.section_register=[](void* p,const char*){auto& t=*static_cast<Trace*>(p);return t.registration_fails?0u:++t.registered;};
        api.scope_begin=[](void* p,uint32_t section)->MT2FW_MT2_Scope{auto& t=*static_cast<Trace*>(p);if(!t.active)return {};
            const auto token=t.next++;t.stack.emplace_back(section,token);++t.begins;t.events.push_back("+"+std::to_string(section));return {t.epoch,token};};
        api.scope_end=[](void* p,uint32_t section,MT2FW_MT2_Scope scope){auto& t=*static_cast<Trace*>(p);
            check(!t.stack.empty()&&t.stack.back()==std::make_pair(section,scope.token),"profiling scope exit lost its nesting or token");
            t.stack.pop_back();++t.ends;t.events.push_back("-"+std::to_string(section));};
        step_profile::state={};
    }
    ~Trace(){step_profile::state={};}
    void configure(){check(step_profile::configure(api),"profiling bridge registration failed");}
    void next_capture(){check(stack.empty(),"capture reset while scopes remain open");++epoch;active=true;events.clear();}
};
struct Test {
    static constexpr uintptr_t image=0x100000000,game=0x200000000,tasks=0x210000000,
        timer=0x220000000,systems=0x230000000,mode=0x240000000,state=0x250000000,world=0x260000000,map=0x270000000;
    std::map<uintptr_t,unsigned char> memory;
    StepNativeObservation observed;
    std::unique_ptr<NativeStepDriver> driver;
    std::vector<std::string> calls;
    MT2FW_MT2_StepReceipt receipt{sizeof(receipt)};
    uint64_t thread=101;
    bool exited=false;
    bool route_ok=true,throw_go=false,throw_finish=false,throw_pause=false,fail_restore=false,
        omit_timer=false,omit_update=false,duplicate_timer=false,wrong_caller=false,done=false;
    unsigned go_count=0,timer_count=0,finish_count=0,pause_count=0;
    template<class T>void put(uintptr_t at,T value){const auto* bytes=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof(T);++i)memory[at+i]=bytes[i];}
    template<class T>T get(uintptr_t at){T value{};auto* bytes=reinterpret_cast<unsigned char*>(&value);for(size_t i=0;i<sizeof(T);++i){auto p=memory.find(at+i);if(p==memory.end())throw std::runtime_error("test missing memory");bytes[i]=p->second;}return value;}
    explicit Test(bool c_proof=false){
        auto& id=observed.identity;id.owner=reinterpret_cast<const MT2FW_Host*>(0x1234);
        id.session=44;id.provider_token=9;id.generation=3;id.snapshot_bytes=8192;id.thread=thread;
        id.agreed_seed=uint64_t(0);id.peer_mask=3;id.snapshot_uuid="actual-snapshot";id.snapshot_sha256=std::string(64,'a');
        id.region_rng_mode=1;id.region_rng_sha256.fill(11);
        observed.phase=StepNativeObservation::Phase::Preload;observed.world_epoch=7;
        observed.game=game;observed.mode=mode;observed.state=state;observed.world=world;observed.map=map;
        observed.witness[0]=42;observed.resume_workers=true;
        put(image+0x13df8f0,game);put(game,image+0x1743740);put(image+0x13df340,tasks);put(game+0x68,tasks);
        put(tasks,image+0x16fdaf0);put(tasks+0xf8,uint8_t(1));put(tasks+0xa8,int32_t(0));put(tasks+0x90,int32_t(0));put(tasks+0xc0,int32_t(1));
        put(game+0x28,mode);put(image+0x13df320,mode);put(mode,image+0x16e6730);put(mode+0x2c4,int32_t(4));
        put(mode+0x58,state);put(state+0x160,world);put(image+0x13df9c0,world);put(world,image+0x174aef0);
        put(world+0x158,map);put(image+0x13df8c0,map);put(map,image+0x1742860);put(map+0x47f,uint8_t(1));
        put(image+0x1c4eb70,timer);put(timer,image+0x16f9b80);put(image+0x16f9ba0,image+step_pins::timer_rva);put(timer+8,uint8_t(1));
        put(game+8,systems);put(game+0x10,int32_t(1));put(systems,timer);put(game+0x1c,uint32_t(0x3d000000));
        NativeStepDriver::Backend backend;backend.image=image;backend.thread=[this]{return thread;};
        backend.read=[this](uintptr_t at,void* out,size_t n){auto* bytes=static_cast<unsigned char*>(out);for(size_t i=0;i<n;++i){auto p=memory.find(at+i);if(p==memory.end())throw std::runtime_error("test missing native memory");bytes[i]=p->second;}};
        backend.write=[this](uintptr_t at,const void* in,size_t n){if(fail_restore&&pause_count)throw std::runtime_error("restore-write");const auto* bytes=static_cast<const unsigned char*>(in);for(size_t i=0;i<n;++i)memory[at+i]=bytes[i];};
        backend.world_exited=[this]{return exited;};
        backend.prove_routes=[this]{if(!route_ok)throw std::runtime_error("actual-route-changed");};
        backend.before_step=[this](const auto&){check(get<uint8_t>(tasks+0xf8)==1,"bootstrap restore must precede worker release");calls.push_back("bootstrap");};
        backend.resume=[this](void* p){check(reinterpret_cast<uintptr_t>(p)==tasks,"Resume receiver");calls.push_back("resume");put(tasks+0xf8,uint8_t(0));};
        backend.pause=[this](void* p){check(reinterpret_cast<uintptr_t>(p)==tasks,"Pause receiver");++pause_count;calls.push_back("pause");if(throw_pause)throw std::runtime_error("pause-failed");put(tasks+0xf8,uint8_t(1));};
        backend.all_done=[this](void*){return done;};
        backend.finish_all=[this](void*){++finish_count;calls.push_back("finish");if(throw_finish)throw std::runtime_error("finish-failed");done=true;put(tasks+0xc0,int32_t(0));};
        backend.original_timer=[this](void* p,float incoming){check(reinterpret_cast<uintptr_t>(p)==timer&&incoming==0.5f,"Timer preserves receiver and incoming argument");++timer_count;calls.push_back("timer");put(game+0x1c,uint32_t(0x3f000000));};
        backend.original_go=[this](void* p){
            check(reinterpret_cast<uintptr_t>(p)==game&&get<uint8_t>(tasks+0xf8)==0,"Go needs actual Game and released workers");
            check(receipt.original_attempted==1&&receipt.original_returned==0,"Go attempt/return bits bracket original");
            ++go_count;calls.push_back("go");
            if(!omit_timer){driver->timer_update(reinterpret_cast<void*>(timer),0.5f,image+(wrong_caller?1:step_pins::timer_return_rva));
                if(duplicate_timer)driver->timer_update(reinterpret_cast<void*>(timer),0.5f,image+step_pins::timer_return_rva);
                check(get<uint32_t>(game+0x1c)==MT2FW_MT2_STEP_FIXED_DT_BITS,"Remaining original frame sees fixed dt");}
            if(throw_go)throw std::runtime_error("original-go-failed");
            if(!omit_update)driver->after_update(p);
        };
        if(c_proof)backend.bootstrap=[this](StepNativeObservation& out){out.resume_workers=observed.resume_workers;out.witness=observed.witness;return true;};
        if(c_proof)driver=std::make_unique<NativeStepDriver>(std::move(backend));
        else driver=std::make_unique<NativeStepDriver>(std::move(backend),[this](const MT2FW_Host* owner){check(owner==observed.identity.owner,"Source exact borrowed owner");return observed;});
    }
    static MT2FW_MT2_Result MT2FW_CALL c_proof(void* p,const MT2FW_MT2_StepProofQuery* q,MT2FW_MT2_StepProofReply* r){
        auto& t=*static_cast<Test*>(p);const auto& o=t.observed;const auto& i=o.identity;
        check(q->size==sizeof(*q)&&q->version==MT2FW_MT2_STEP_DRAFT_VERSION&&q->session==i.session&&q->session_provider_token==i.provider_token&&q->thread==i.thread,"C proof gets actual query identity");
        *r={};r->size=sizeof(*r);r->phase=uint32_t(o.phase);r->session=i.session;r->session_provider_token=i.provider_token;
        r->generation=i.generation;r->snapshot_bytes=i.snapshot_bytes;r->seed_present=i.agreed_seed.has_value()?1u:0u;r->agreed_seed=i.agreed_seed.value_or(0);
        r->thread=i.thread;r->world_epoch=o.world_epoch;r->game=o.game;r->mode=o.mode;r->state=o.state;r->world=o.world;r->map=o.map;
        r->frozen_peer_mask=i.peer_mask;r->resume_workers_known=0;r->resume_workers=0;
        r->region_rng_mode=i.region_rng_mode;r->region_rng_count=i.region_rng_count;r->region_rng_index=q->region_rng_index;
        std::copy(i.region_rng_sha256.begin(),i.region_rng_sha256.end(),r->region_rng_sha256);
        if(q->region_rng_index!=UINT32_MAX){r->region_rng_id=int32_t(3+q->region_rng_index*4);r->region_rng_words[0]=uint64_t(17+q->region_rng_index);r->region_rng_extra=-7;}
        std::memcpy(r->snapshot_uuid,i.snapshot_uuid.c_str(),i.snapshot_uuid.size()+1);
        for(size_t n=0;n<32;++n)r->snapshot_sha256[n]=static_cast<unsigned char>(std::stoul(i.snapshot_sha256.substr(n*2,2),nullptr,16));
        return MT2FW_MT2_OK;
    }
    MT2FW_MT2_StepSpec spec(){MT2FW_MT2_StepSpec s{};s.proof=c_proof;s.proof_user=this;s.size=sizeof(s);s.dt_bits=MT2FW_MT2_STEP_FIXED_DT_BITS;s.session=44;s.session_provider_token=9;s.frozen_peer_mask=3;constexpr char uuid[]="actual-snapshot";std::memcpy(s.snapshot_uuid,uuid,sizeof(uuid));return s;}
    void freeze(){driver->freeze_preload(observed.identity);observed.phase=StepNativeObservation::Phase::Loaded;}
    void advance(){MT2FW_MT2_StepBoundary frame{};frame.size=sizeof(frame);frame.session=44;frame.world_epoch=7;frame.thread=101;frame.dt_bits=MT2FW_MT2_STEP_FIXED_DT_BITS;driver->advance(frame,receipt);}
};
}
int main(){try{
    {using namespace step_profile;Trace trace;int calls=0;
        check(call(Part::before_step,[&]{return ++calls;})==1&&!state.counts[size_t(Part::before_step)].entered,"unconfigured profiling changed a call or emitted a count");
        trace.registration_fails=true;check(!configure(trace.api)&&state.availability_cause[0]&&!state.configured,"trace registration failure was hidden");
        check(call(Part::before_step,[&]{return ++calls;})==2,"diagnostic configuration failure blocked native work");
        state={};trace.registration_fails=false;trace.configure();
        check(trace.registered==names.size()&&call(Part::before_step,[&]{return ++calls;})==3&&!trace.begins&&!state.epoch,"inactive capture emitted profiling events");
        trace.next_capture();
        call(Part::driver_preproof,[&]{check(consumer_proof([]{return MT2FW_MT2_OK;})==MT2FW_MT2_OK,"successful consumer result changed");});
        check(trace.events==std::vector<std::string>({"+3","+11","-11","-3"}),"nested consumer proof lost provider parent");
        const auto& outer=state.counts[size_t(Part::driver_preproof)];check(outer.entered==1&&outer.returned==1&&!outer.failed,"normal scope counters differ");
        check(raises([&]{call(Part::original_go,[&]{call(Part::finish_all,[]{throw std::runtime_error("retained-drain-cause");});});},"retained-drain-cause"),"scope changed original exception");
        check(std::string(state.first_failure)=="step.finish_all: retained-drain-cause","first nested failure cause was replaced");
        const auto& drain=state.counts[size_t(Part::finish_all)];check(drain.entered==1&&!drain.returned&&drain.failed==1,"throwing drain claimed a return");
        bool preserved=false;try{call(Part::resume,[]{throw 17;});}catch(int value){preserved=value==17;}
        check(preserved&&state.counts[size_t(Part::resume)].failed==1,"scope changed an unknown exception");
        check(consumer_proof([]{return MT2FW_MT2_UNAVAILABLE;})==MT2FW_MT2_UNAVAILABLE,"failed proof status changed");
        const auto& proof=state.counts[size_t(Part::consumer_proof)];check(proof.entered==2&&proof.returned==2&&proof.failed==1,"failed returned proof lacks distinct return and failure counters");
        check(trace.stack.empty()&&trace.begins==trace.ends,"exception paths left profiling scopes open");
        trace.next_capture();call(Part::before_step,[]{});
        check(state.epoch==trace.epoch&&state.counts[size_t(Part::before_step)].entered==1&&!state.first_failure[0]&&!state.counts[size_t(Part::finish_all)].entered,"capture counters crossed epochs");
    }
    {using namespace step_profile;Trace trace;trace.configure();trace.next_capture();Test t;t.freeze();t.advance();
        check(trace.stack.empty()&&trace.begins==trace.ends,"actual driver scopes are unbalanced");
        for(auto part:{Part::driver_preproof,Part::before_step,Part::resume,Part::original_go,Part::finish_all,Part::pause_restore,Part::driver_postproof}){
            const auto& c=state.counts[size_t(part)];check(c.entered==1&&c.returned==1&&!c.failed,"actual driver component was not measured exactly once");}
        check(!state.counts[size_t(Part::after_go)].entered,"absent optional after_go was measured as an executed call");
    }
    {using namespace step_profile;Trace trace;trace.configure();trace.next_capture();Test t;t.freeze();t.throw_finish=true;
        check(raises([&]{t.advance();},"finish-failed"),"profiled driver hid original drain failure");
        const auto& c=state.counts[size_t(Part::finish_all)];check(c.entered==1&&!c.returned&&c.failed==1,"actual throwing drain counters differ");
        check(trace.stack.empty()&&trace.begins==trace.ends&&t.go_count==1&&t.pause_count==1,"profiled driver failure changed original/restore attempts or scope balance");
    }
    {using namespace step_profile;Trace trace;trace.configure();trace.next_capture();Test t;t.freeze();t.done=true;t.put(Test::tasks+0xc0,int32_t(0));t.advance();
        check(!state.counts[size_t(Part::finish_all)].entered&&!t.finish_count,"already drained step emitted a false FinishAllTasks call");}
    {Test t(true);t.observed.identity.region_rng_mode=2;t.observed.identity.region_rng_count=2;
        check(t.driver->inspect(t.observed.identity.owner,t.spec()).context,"preserved Region preload unavailable");
        const auto rows=t.driver->region_rng_rows();check(rows.size()==2&&rows[0].id==3&&rows[1].id==7&&rows[1].value.words[0]==18&&rows[1].value.extra==-7,"preserved Region proof rows differ");
        ++t.observed.identity.region_rng_sha256[0];check(raises([&]{t.driver->region_rng_rows();},"frozen preload identity"),"Region row commitment changed after freeze");}
    for(int mode=0;mode<5;++mode){Test t(true);auto s=t.spec();std::pair<Test*,int> context{&t,mode};s.proof_user=&context;
        s.proof=[](void* p,const auto* q,auto* r){auto& v=*static_cast<std::pair<Test*,int>*>(p);auto result=Test::c_proof(v.first,q,r);
            switch(v.second){case 0:r->region_rng_mode=0;break;case 1:r->region_rng_count=1;break;case 2:r->region_rng_index=0;break;case 3:r->region_rng_words[0]=1;break;case 4:r->region_rng_reserved=1;break;}return result;};
        check(raises([&]{read_step_proof(t.observed.identity.owner,s,101);},"Region RNG"),"malformed Region metadata accepted");}
    {Test t;auto proof=t.driver->inspect(t.observed.identity.owner,t.spec());check(!proof.context&&!proof.bootstrap&&t.go_count==0,"No handoff remains unavailable");}
    {Test t;t.observed.identity.agreed_seed.reset();check(raises([&]{t.driver->freeze_preload(t.observed.identity);},"shared agreed seed"),"Missing seed rejected");}
    {Test t;t.freeze();check(t.driver->frozen(),"Seed zero and real preload accepted");check(raises([&]{t.driver->freeze_preload(t.observed.identity);},"already frozen"),"Frozen preload cannot be replaced");}
    {Test t;t.observed.phase=StepNativeObservation::Phase::Loading;check(raises([&]{t.driver->freeze_preload(t.observed.identity);},"selection"),"Late preload rejected");}
    {Test t;t.route_ok=false;check(raises([&]{t.driver->freeze_preload(t.observed.identity);},"actual-route-changed"),"Foreign physical route rejected before freeze");}
    {Test t;t.freeze();t.advance();check(t.calls==std::vector<std::string>({"bootstrap","resume","go","timer","finish","pause"}),"Original native call order");
        check(t.receipt.original_attempted==1&&t.receipt.original_returned==1&&t.receipt.timer_calls==1&&t.receipt.workers_held==1,"Native receipt has actual effects");
        check(t.get<uint32_t>(Test::game+0x1c)==0x3d000000&&t.get<uint8_t>(Test::tasks+0xf8)==1,"Passive dt and held scheduler restored");
        check(!t.driver->failed()&&t.go_count==1&&t.timer_count==1&&t.finish_count==1&&t.pause_count==1,"One original per boundary");}
    for(int field=0;field<8;++field){Test t;t.freeze();switch(field){case 0:t.observed.identity.agreed_seed=1;break;case 1:++t.observed.identity.session;break;
        case 2:++t.observed.identity.generation;break;case 3:++t.observed.identity.snapshot_bytes;break;case 4:t.observed.identity.snapshot_sha256[0]='b';break;
        case 5:++t.observed.identity.provider_token;break;case 6:t.observed.identity.peer_mask=1;break;case 7:t.observed.identity.snapshot_uuid="other";break;}
        check(!t.driver->inspect(t.observed.identity.owner,t.spec()).bootstrap&&t.go_count==0,"Changed immutable selected context rejects bootstrap");}
    for(int field=0;field<6;++field){Test t;t.freeze();switch(field){case 0:t.observed.witness.fill(0);break;case 1:t.observed.resume_workers.reset();break;
        case 2:t.observed.world_epoch=0;break;case 3:t.observed.phase=StepNativeObservation::Phase::Unavailable;break;
        case 4:t.put(Test::map+0x47f,uint8_t(0));break;case 5:t.put(Test::tasks+0xa8,int32_t(1));break;}
        check(!t.driver->inspect(t.observed.identity.owner,t.spec()).bootstrap,"Missing or stale actual bootstrap rejects activation");}
    {Test t;t.freeze();t.thread=102;check(!t.driver->inspect(t.observed.identity.owner,t.spec()).context,"Different thread rejected");}
    {Test t;t.freeze();auto s=t.spec();s.session_provider_token=10;check(!t.driver->inspect(t.observed.identity.owner,s).context,"StepSpec cannot select another token");}
    {Test t;t.freeze();t.route_ok=false;check(raises([&]{t.advance();},"actual-route-changed")&&t.go_count==0,"Changed route prevents original entry");}
    for(int failure=0;failure<7;++failure){Test t;t.freeze();std::string cause;
        switch(failure){case 0:t.throw_go=true;cause="original-go-failed";break;case 1:t.throw_finish=true;cause="finish-failed";break;
            case 2:t.wrong_caller=true;cause="Timer caller/count";break;case 3:t.duplicate_timer=true;cause="Timer caller/count";break;
            case 4:t.omit_timer=true;cause="proof is incomplete";break;case 5:t.omit_update=true;cause="proof is incomplete";break;
            case 6:t.throw_pause=true;cause="pause-failed";break;}
        check(raises([&]{t.advance();},cause),"Native failure retains original cause");check(t.driver->failed()&&t.go_count==1&&t.pause_count==1,"Failure pauses once without replay");
        check(raises([&]{t.advance();},"failed result")&&t.go_count==1,"Failed driver cannot retry original");
        check(t.receipt.original_attempted==1,"Failure retains actual attempt bit");
        if(failure==6)check(t.receipt.original_returned==1&&t.receipt.workers_held==0,"Failed pause retains returned original and unknown hold");
        if(failure==0||failure==1||failure==2||failure==3)check(t.receipt.original_returned==0,"Unreturned original never claims completion");
    }
    {Test t;t.freeze();t.throw_go=t.throw_pause=true;check(raises([&]{t.advance();},"original-go-failed; held restoration failed: pause-failed"),"Cleanup failure preserves both native causes");}
    {Test t;t.freeze();t.fail_restore=true;check(raises([&]{t.advance();},"restore-write")&&t.pause_count==1&&t.receipt.workers_held==0,"Failed dt restore never repeats Pause or claims complete hold");}
    {Test t;t.freeze();t.observed.resume_workers=false;check(raises([&]{t.advance();},"running Task scheduler")&&t.go_count==0,"Original prior-paused state is not silently resumed");}
    {Test t;t.freeze();t.driver->timer_update(reinterpret_cast<void*>(Test::timer),0.5f,0);
        check(t.timer_count==1&&t.get<uint32_t>(Test::game+0x1c)==0x3f000000,"Passive Timer remains original");}
    {Test t(true);auto s=t.spec();auto p=t.driver->inspect(t.observed.identity.owner,s);
        check(p.context&&!p.bootstrap&&t.driver->frozen(),"C callback freezes actual Preload before load");
        t.observed.phase=StepNativeObservation::Phase::Loaded;p=t.driver->inspect(t.observed.identity.owner,s);
        check(p.bootstrap&&p.world_epoch==7&&p.witness[0]==42,"C callback loaded NativeOpen proof was not consumed");t.advance();check(t.go_count==1,"C callback did not reach one original");}
    {Test t(true);t.observed.phase=StepNativeObservation::Phase::Loaded;check(!t.driver->inspect(t.observed.identity.owner,t.spec()).context,"Already-loaded C proof bypassed preload");}
    {Test t(true);t.observed.identity.agreed_seed.reset();check(!t.driver->inspect(t.observed.identity.owner,t.spec()).context&&!t.driver->frozen(),"C callback manufactured missing seed");}
    for(int mode=0;mode<6;++mode){Test t;auto s=t.spec();s.proof=[](void* p,const MT2FW_MT2_StepProofQuery* q,MT2FW_MT2_StepProofReply* r){
            auto pair=static_cast<std::pair<Test*,int>*>(p);auto result=Test::c_proof(pair->first,q,r);
            switch(pair->second){case 0:r->size=1;break;case 1:r->reserved[0]=1;break;case 2:++r->session;break;case 3:r->seed_present=0;break;
                case 4:std::memset(r->snapshot_uuid,'x',sizeof(r->snapshot_uuid));break;case 5:std::memcpy(r->cause,"actual-owner-cause",sizeof("actual-owner-cause"));return MT2FW_MT2_FAILED;}
            return result;};std::pair<Test*,int> context{&t,mode};s.proof_user=&context;
        check(raises([&]{read_step_proof(t.observed.identity.owner,s,101);},mode==5?"actual-owner-cause":"Step proof"),"Malformed C proof or original failure was accepted");}
    for(int kind=0;kind<2;++kind){Test t;auto s=t.spec();std::pair<Test*,int> state{&t,kind};s.proof_user=&state;
        s.proof=[](void* p,const auto* q,auto* r){auto& v=*static_cast<std::pair<Test*,int>*>(p);auto result=Test::c_proof(v.first,q,r);
            if(v.second)r->resume_workers_known=1;else r->initial_witness_sha256[0]=1;return result;};
        check(raises([&]{read_step_proof(t.observed.identity.owner,s,101);},"Step proof"),"MP supplied Framework-owned certificate or Pause receipt");}
    {Test t(true);check(t.driver->inspect(t.observed.identity.owner,t.spec()).context,"first preload missing");
        t.observed.phase=StepNativeObservation::Phase::Loaded;t.advance();
        check(raises([&]{t.driver->reset_after_world_exit();},"verified exited world"),"driver reset without native exit");
        t.exited=true;t.driver->reset_after_world_exit();check(!t.driver->frozen()&&!t.driver->failed(),"driver retained first preload after verified reset");
        ++t.observed.identity.generation;t.observed.identity.agreed_seed=uint64_t(123);t.observed.phase=StepNativeObservation::Phase::Preload;
        check(t.driver->inspect(t.observed.identity.owner,t.spec()).context&&t.driver->frozen(),"second C proof reused retired selected generation");
        t.observed.phase=StepNativeObservation::Phase::Loaded;t.receipt={sizeof(t.receipt)};t.advance();check(t.go_count==2,"second native world original unavailable");}
    std::cout<<checks<<" native Step driver checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"Native Step driver failure: "<<error.what()<<'\n';return 1;}}
