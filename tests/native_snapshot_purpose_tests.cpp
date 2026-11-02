#define main adapter_baseline_main
#include "native_step_adapter_tests.cpp"
#undef main

struct SnapshotFixture:Fixture {
    uint32_t operation=MT2FW_MT2_SNAPSHOT_ARM_PERIODIC;
    unsigned native_calls=0;
    MT2FW_MT2_Result result=MT2FW_MT2_UNAVAILABLE;
    bool from_service=false;
    SnapshotFixture(){
        NativeStepAdapter::Driver driver;
        driver.inspect=[this](auto*,const auto&){NativeStepKernel::Proof p;p.context=p.exclusive_routes=p.quiescent=p.bootstrap=true;
            p.world_epoch=native_epoch;p.witness[0]=42;return p;};
        driver.advance=[this](const auto&,auto& out){++originals;out.original_attempted=out.original_returned=out.timer_calls=out.workers_held=1;};
        driver.snapshot=[this](const auto& request,auto callback,void* user){++native_calls;
            check(request.operation==operation&&!callback&&!user&&plugin->native_pins==1,"Snapshot purpose or owner pin changed in bridge");};
        check(adapter.configure(std::move(driver)),"Snapshot driver configure failed");
        spec.service=[](void* p,const auto* step){auto& f=*static_cast<SnapshotFixture*>(p);
            if(f.from_service)f.call(step->lease,step->session,step->world_epoch,step->tick,0);return MT2FW_MT2_OK;};
        spec.boundary=[](void* p,const auto* frame,auto,void*){auto& f=*static_cast<SnapshotFixture*>(p);
            if(!f.from_service)f.call(frame->lease,frame->session,frame->world_epoch,frame->tick,frame->invocation);
            return MT2FW_MT2_STEP_KEEP_HELD;};
        bind();check(activate()==MT2FW_MT2_OK,"Snapshot fixture activation failed");
    }
    void call(uint64_t lease,uint64_t session,uint64_t epoch,uint64_t tick,uint64_t invocation){
        MT2FW_MT2_SnapshotRequest request{};request.size=sizeof(request);request.operation=operation;
        request.lease=lease;request.session=session;request.world_epoch=epoch;request.tick=tick;request.order=1;
        request.invocation=invocation;request.peer_mask=3;std::memcpy(request.nonce,"12345678-1234-1234-1234-123456789abc",37);
        MT2FW_MT2_StepReceipt out{sizeof(out)};result=api.snapshot(api.context,&request,nullptr,nullptr,&out);
    }
};
int main(){try{
    for(uint32_t operation:{uint32_t(MT2FW_MT2_SNAPSHOT_ARM),uint32_t(MT2FW_MT2_SNAPSHOT_ARM_PERIODIC)}){
        SnapshotFixture f;f.operation=operation;f.adapter.boundary();
        check(f.result==MT2FW_MT2_OK&&f.native_calls==1&&f.originals==0&&f.plugin->native_pins==0,
            "compatible ARM purpose failed actual adapter admission or borrowed original Next");}
    {SnapshotFixture f;f.operation=MT2FW_MT2_SNAPSHOT_ARM_PERIODIC+1;f.adapter.boundary();
        check(f.result!=MT2FW_MT2_OK&&!f.native_calls&&!f.originals,"unknown Snapshot operation reached native driver");}
    {SnapshotFixture f;f.from_service=true;f.adapter.boundary();
        check(f.result!=MT2FW_MT2_OK&&!f.native_calls&&!f.originals,"ARM_PERIODIC bypassed borrowed boundary admission");}
    std::cout<<"PASS "<<checks<<" actual Snapshot-purpose adapter checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
