#include "native_step_adapter.h"
#include <iostream>
#include <stdexcept>
using namespace mt2fw;
using namespace mt2fw::mt2;
int checks=0;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
struct Fixture {
    Runtime runtime;
    uint64_t thread=7,native_epoch=1;
    bool world=true,lab=false,bootstrap=true,stale=false,retire_inside=false;
    int originals=0,pin_reads=0,proof_reads=0,resets=0;bool exit_proven=false;
    size_t expected_proof_pins=0;
    bool throw_proof=false,bad_proof_size=false,retire_proof=false,rebind_proof=false;
    NativePatches patches;
    Plugin* plugin=nullptr;
    uint64_t provider_token=0;
    MT2FW_MT2_PatchesV1 patch_api{};
    NativeStepAdapter adapter;
    MT2FW_MT2_StepServiceDraftV1 api{};
    MT2FW_MT2_StepReceipt receipt{sizeof(receipt)};
    MT2FW_MT2_StepSpec spec{};
    Fixture():runtime(fs::current_path()/"adapter-owned-fixture",[](const auto&,const auto&){return Json();}),
        patches(runtime,[this]{return thread;},[this](uint64_t& epoch,std::string& cause){epoch=world?runtime.epoch:0;cause=world?"owned world":"title before load";return world;}),
        adapter(runtime,patches,[this]{return thread;},[]{return uint64_t(7);},[this]{return lab;}){
        runtime.world_ready("fixture");patches.set_game_thread(7);
        auto value=std::make_unique<Plugin>(&runtime);value->manifest.id="doodesch.multiplayer";value->manifest.type="native";
        value->active=true;value->staging=false;value->native_loaded=true;plugin=value.get();
        const_cast<std::vector<std::unique_ptr<Plugin>>&>(runtime.plugins()).push_back(std::move(value));
        check(patches.query(&plugin->host,1,&patch_api,sizeof(patch_api))==MT2FW_MT2_OK,"provider query failed");
        MT2FW_MT2_PatchReceipt r{sizeof(r)};
        auto provider=[](void* user,const MT2FW_MT2_SessionQuery* q,MT2FW_MT2_SessionReply* out){auto& f=*static_cast<Fixture*>(user);out->role=MT2FW_MT2_SESSION_CLIENT;out->epoch=q->epoch+(f.stale?1:0);return MT2FW_MT2_OK;};
        check(patch_api.bind_session_provider(patch_api.context,provider,this,&provider_token,&r)==MT2FW_MT2_OK,"provider bind failed");
        spec.size=sizeof(spec);spec.dt_bits=MT2FW_MT2_STEP_FIXED_DT_BITS;spec.session=11;spec.session_provider_token=provider_token;
        spec.save_request=[](void*,const MT2FW_MT2_SaveRequest*,char*,uint32_t){return MT2FW_MT2_UNAVAILABLE;};
        spec.frozen_peer_mask=3;std::memcpy(spec.snapshot_uuid,"snapshot-1",sizeof("snapshot-1"));spec.user=this;
        spec.proof_user=this;
        spec.proof=[](void* user,const MT2FW_MT2_StepProofQuery* q,MT2FW_MT2_StepProofReply* out){
            auto& f=*static_cast<Fixture*>(user);++f.proof_reads;
            check(f.plugin->native_pins==f.expected_proof_pins,"proof callback lacks its additional native owner pin");
            check(q->size==sizeof(*q)&&q->version==MT2FW_MT2_STEP_DRAFT_VERSION&&q->session==11&&q->session_provider_token==f.provider_token&&q->thread==7,"proof query identity changed");
            if(f.throw_proof)throw std::runtime_error("actual-proof-failure");
            if(f.retire_proof)check(!f.adapter.retire(*f.plugin),"proof callback retired its storage while in flight");
            if(f.rebind_proof){auto receipt=MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};check(f.api.bind(f.api.context,&f.spec,&receipt)==MT2FW_MT2_BUSY,"proof callback reentered bind");}
            if(f.bad_proof_size)out->size=1;
            return MT2FW_MT2_OK;
        };
        spec.service=[](void* user,const auto*){auto& f=*static_cast<Fixture*>(user);check(f.plugin->native_pins==1,"service callback has no native owner pin");++f.pin_reads;return MT2FW_MT2_OK;};
        spec.boundary=[](void* user,const auto* frame,MT2FW_MT2_StepNext next,void* context){
            auto& f=*static_cast<Fixture*>(user);check(f.plugin->native_pins==1,"boundary callback has no native owner pin");++f.pin_reads;
            if(f.retire_inside){check(!f.adapter.retire(*f.plugin),"in-flight owner retired early");return MT2FW_MT2_STEP_KEEP_HELD;}
            auto out=MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};
            return next(context,frame->invocation,&out)==MT2FW_MT2_OK?MT2FW_MT2_STEP_COMPLETED:MT2FW_MT2_STEP_STOP_UNKNOWN;
        };
    }
    void configure(){check(adapter.configure({[this](auto*,const auto& spec){NativeStepKernel::Proof p;p.context=p.exclusive_routes=p.quiescent=true;
        MT2FW_MT2_StepProofQuery q{sizeof(q),MT2FW_MT2_STEP_DRAFT_VERSION,11,provider_token,7};MT2FW_MT2_StepProofReply r{};r.size=sizeof(r);
        expected_proof_pins=plugin->native_pins+1;const auto result=spec.proof(spec.proof_user,&q,&r);
        if(result!=MT2FW_MT2_OK||r.size!=sizeof(r)){p.context=false;p.cause=r.cause[0]?r.cause:"proof size differs";return p;}
        p.bootstrap=bootstrap;p.world_epoch=world?native_epoch:0;p.witness[0]=42;return p;},[this](const auto& frame,auto& out){
        out.original_attempted=1;++originals;
        auto fresh=MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};
        check(api.read(api.context,frame.lease,&fresh)==MT2FW_MT2_OK&&fresh.phase==MT2FW_MT2_STEP_ENTERED&&fresh.original_attempted==1&&fresh.original_returned==0,"adapter nested original-running read failed");
        check(fresh.lease==frame.lease&&fresh.session==frame.session&&fresh.world_epoch==frame.world_epoch&&fresh.tick==frame.tick&&fresh.invocation==frame.invocation&&fresh.thread==frame.thread&&fresh.actual_role==MT2FW_MT2_SESSION_CLIENT,"adapter nested read identity differs");
        out.original_returned=1;out.timer_calls=1;out.workers_held=1;
    },[this]{NativeStepKernel::Proof p;p.world_exited=exit_proven;p.quiescent=true;p.thread=thread;return p;},[this]{++resets;}}),"driver configure failed");}
    void bind(){check(adapter.query(&plugin->host,MT2FW_MT2_STEP_DRAFT_VERSION,&api,sizeof(api))==MT2FW_MT2_OK,"adapter query failed");check(api.bind(api.context,&spec,&receipt)==MT2FW_MT2_OK,"adapter bind failed");
        if(bootstrap){auto life=MT2FW_MT2_StepReceipt{sizeof(MT2FW_MT2_StepReceipt)};life.generation=1;
            life.seed_attempted=life.seed_returned=life.bootstrap_held=life.workers_held=1;life.world_epoch=native_epoch;
            life.bootstrap_coverage=MT2FW_MT2_STEP_COVERAGE_INITIAL_DEVELOPERS;life.initial_witness_sha256[0]=42;adapter.lifecycle(life);
            check(api.read(api.context,receipt.lease,&receipt)==MT2FW_MT2_OK,"synthetic adapter covered bootstrap read");}}
    MT2FW_MT2_Result activate(){MT2FW_MT2_StepActivation a{};a.size=sizeof(a);a.lease=receipt.lease;a.session=11;a.world_epoch=native_epoch;a.first_tick=1;
        std::memcpy(a.snapshot_uuid,"snapshot-1",sizeof("snapshot-1"));a.initial_witness_sha256[0]=42;return api.activate(api.context,&a,&receipt);}
};
int main(){try{
    {Fixture f;f.configure();f.spec.save_request=nullptr;
        check(f.adapter.query(&f.plugin->host,MT2FW_MT2_STEP_DRAFT_VERSION,&f.api,sizeof(f.api))==MT2FW_MT2_OK,"draft 9 query failed");
        check(f.api.bind(f.api.context,&f.spec,&f.receipt)==MT2FW_MT2_INVALID&&f.originals==0,"missing draft 9 Save callback admitted");}
    {Fixture f;check(f.adapter.query(&f.plugin->host,MT2FW_MT2_STEP_DRAFT_VERSION,&f.api,sizeof(f.api))==MT2FW_MT2_UNAVAILABLE,"unconfigured native driver advertised support");}
    {Fixture f;f.lab=true;check(!f.adapter.configure({[](auto*,const auto&){return NativeStepKernel::Proof{};},[](const auto&,auto&){}}),"lab allowed a second configured owner");}
    {Fixture f;f.configure();auto copy=f.plugin->host;check(f.adapter.query(&copy,MT2FW_MT2_STEP_DRAFT_VERSION,&f.api,sizeof(f.api))==MT2FW_MT2_UNAVAILABLE,"copied Host was accepted");}
    {Fixture f;f.configure();f.thread=8;check(f.adapter.query(&f.plugin->host,MT2FW_MT2_STEP_DRAFT_VERSION,&f.api,sizeof(f.api))==MT2FW_MT2_UNAVAILABLE,"foreign-thread query accepted");}
    {Fixture f;f.configure();f.world=false;f.bootstrap=false;f.bind();check(f.receipt.actual_role==MT2FW_MT2_SESSION_CLIENT,"pre-load role was not independently acknowledged");check(f.activate()==MT2FW_MT2_UNAVAILABLE&&f.originals==0,"preload registration became fake world authority");}
    {Fixture f;f.configure();f.bind();check(f.activate()==MT2FW_MT2_OK,"activation failed");auto r=f.adapter.boundary();
        check(f.originals==1&&r.actual_role==MT2FW_MT2_SESSION_CLIENT&&r.completed_tick==1,"real Client provider role was changed");
        check(f.pin_reads==2&&f.plugin->native_pins==0,"native callback pin was leaked");
        check(f.adapter.retire(*f.plugin)&&f.adapter.retained(),"callback retirement resumed the native world");}
    {Fixture f;f.configure();f.spec.session_provider_token++;check(f.adapter.query(&f.plugin->host,MT2FW_MT2_STEP_DRAFT_VERSION,&f.api,sizeof(f.api))==MT2FW_MT2_OK,"query failed");
        check(f.api.bind(f.api.context,&f.spec,&f.receipt)==MT2FW_MT2_UNAVAILABLE&&std::strstr(f.receipt.cause,"token"),"stale SessionProvider token was admitted or hidden");}
    {Fixture f;f.configure();f.bind();f.stale=true;check(f.activate()==MT2FW_MT2_FAILED&&f.originals==0,"stale world epoch reply was admitted");}
    {Fixture f;f.configure();f.bind();check(f.activate()==MT2FW_MT2_OK,"activation failed");f.retire_inside=true;auto r=f.adapter.boundary();
        check(f.originals==0&&r.phase==MT2FW_MT2_STEP_FAILED&&f.plugin->native_pins==0,"in-flight retirement leaked pin or entered original");
        check(f.adapter.retire(*f.plugin),"quiescent callback retirement was not confirmed");}
    {Fixture f;f.configure();check(f.adapter.query(&f.plugin->host,1,&f.api,sizeof(f.api))==MT2FW_MT2_INVALID,"v1 caller accepted by v2 export");}
    {Fixture f;f.configure();check(f.adapter.query(&f.plugin->host,MT2FW_MT2_STEP_DRAFT_VERSION,&f.api,sizeof(f.api))==MT2FW_MT2_OK,"v2 query failed");f.spec.size=128;
        check(f.api.bind(f.api.context,&f.spec,&f.receipt)==MT2FW_MT2_INVALID,"old sized binding was read as v2");}
    {Fixture f;f.configure();check(f.adapter.query(&f.plugin->host,MT2FW_MT2_STEP_DRAFT_VERSION,&f.api,sizeof(f.api))==MT2FW_MT2_OK,"query failed");f.throw_proof=true;
        check(f.api.bind(f.api.context,&f.spec,&f.receipt)==MT2FW_MT2_UNAVAILABLE&&std::strstr(f.receipt.cause,"actual-proof-failure")&&f.plugin->native_pins==0,"proof cause or native pin was lost");}
    {Fixture f;f.configure();check(f.adapter.query(&f.plugin->host,MT2FW_MT2_STEP_DRAFT_VERSION,&f.api,sizeof(f.api))==MT2FW_MT2_OK,"query failed");f.bad_proof_size=true;
        check(f.api.bind(f.api.context,&f.spec,&f.receipt)==MT2FW_MT2_UNAVAILABLE&&f.plugin->native_pins==0,"bad proof size was accepted");}
    {Fixture f;f.configure();f.retire_proof=f.rebind_proof=true;f.bind();check(f.proof_reads>0&&f.plugin->native_pins==0,"proof pin did not retire after read");}
    {Fixture f;f.configure();f.world=false;f.bootstrap=false;f.stale=true;
        check(f.adapter.query(&f.plugin->host,MT2FW_MT2_STEP_DRAFT_VERSION,&f.api,sizeof(f.api))==MT2FW_MT2_OK,"preload stale query");
        check(f.api.bind(f.api.context,&f.spec,&f.receipt)==MT2FW_MT2_UNAVAILABLE,"stale preload epoch-zero acknowledgement accepted");}
    {Fixture f;f.configure();f.bind();check(f.activate()==MT2FW_MT2_OK,"first exit-test activation");const auto lease=f.receipt.lease;
        check(!f.adapter.reset_after_world_exit(),"active callback ownership reset");
        check(f.api.unbind(f.api.context,lease,&f.receipt)==MT2FW_MT2_OK,"actual typed callback retirement");
        f.throw_proof=true;const auto reads=f.proof_reads;
        check(!f.adapter.reset_after_world_exit()&&f.resets==0,"unbind manufactured Framework native exit");
        f.exit_proven=true;check(f.adapter.reset_after_world_exit()&&f.resets==1&&f.proof_reads==reads,"Framework exit proof called retired MP proof or omitted driver reset");
        f.exit_proven=false;f.throw_proof=false;f.runtime.world_leave();f.runtime.world_ready("next");f.native_epoch=2;f.bind();check(f.receipt.lease>lease&&f.activate()==MT2FW_MT2_OK,"second same-owner adapter binding failed");
        auto r=f.adapter.boundary();check(f.originals==1&&r.completed_tick==1,"second adapter world cannot advance");}
    {Fixture f;f.runtime.world_leave();f.runtime.world_ready("second");f.native_epoch=2;
        check(f.runtime.epoch==3,"actual Framework load/leave/load counter differs");f.configure();f.bind();
        check(f.activate()==MT2FW_MT2_OK,"independent native2/framework3 load was rejected");auto r=f.adapter.boundary();
        check(r.world_epoch==2&&r.completed_tick==1&&f.originals==1,"native epoch was overwritten by Framework counter");}
    {Fixture f;f.configure();f.bind();++f.runtime.epoch;
        check(f.activate()==MT2FW_MT2_FAILED&&std::strstr(f.receipt.cause,"epoch pair changed")&&f.originals==0,"changed Framework world was accepted");}
    {Fixture f;f.configure();f.bind();++f.native_epoch;
        check(f.activate()==MT2FW_MT2_FAILED&&std::strstr(f.receipt.cause,"epoch pair changed")&&f.originals==0,"changed NativeOpen world was accepted");}
    {Fixture f;f.configure();f.bind();check(f.activate()==MT2FW_MT2_OK,"rebind initial activation");
        check(f.api.bind(f.api.context,&f.spec,&f.receipt)==MT2FW_MT2_BUSY,"busy rebind accepted");
        ++f.runtime.epoch;auto r=f.adapter.boundary();
        check(r.phase==MT2FW_MT2_STEP_FAILED&&f.originals==0,"failed bind discarded prior epoch ownership");}
    std::cout<<checks<<" native step adapter checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"Step adapter failed after "<<checks<<": "<<error.what()<<'\n';return 1;}}
