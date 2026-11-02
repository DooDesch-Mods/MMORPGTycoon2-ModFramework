    // The native cancellation loop must finish before the later full drain.
    owned_batch({9,2,7,5,4,8});memory[0x1010c]=memory[0x1030c]=1;
    memory[0x103fd]=1;memory[0x10200]=image+0x123456;
    pump_native();
    check(callback_order==std::vector<uintptr_t>({0x10100,0x10200,0x10300}));
    check(finishes==2&&nonplan_finishes==1&&end_thinks==2&&special_effects==1&&native_deletes==2);
    check(retained()==std::vector<uintptr_t>({0x10000,0x10400,0x10500})&&d::pending==3&&o::batches==0);
    check(read<uint8_t>(0x1010e)==1&&read<uint8_t>(0x1030e)==1&&read<uint8_t>(0x1000e)==0);
    check(per_task_finishes[0x10100]==1&&per_task_finishes[0x10300]==1&&per_task_finishes[0x10000]==0);
    const auto cancelled_done=read<uint8_t>(0x1010e)&&read<uint8_t>(0x1030e);
    check(cancelled_done&&native_runs==0&&d::finish_entered==2&&d::finish_returned==2);
    next_snapshot();pump_native();check(finishes==2&&native_deletes==2&&d::pending==3);
    next_snapshot();{d::StepDrain drain;pump_native();}
    check(snapshot_ids()==std::vector<int32_t>({4,8,9})&&finishes==5&&native_deletes==5&&native_runs==0);
    check(o::admitted==3&&d::pending==0&&per_task_finishes[0x10100]==1&&per_task_finishes[0x10300]==1);
    // A completed task cannot enter the new completion path a second time.
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10100),image+d::PumpFinishReturn);},"type/completion");
    check(finishes==5);
    // Loading ownership is unchanged; cancellation admission is Step-only.
    owned_batch({9});memory[0x1000c]=1;control.phase=Control::Phase::Loading;
    pump_unlock();check(d::pending==1&&finishes==0);
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"drain admission");
    control.phase=Control::Phase::Held;
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"drain admission");
    // Exact original Pump caller, scheduler, task type and Finish slot remain required.
    owned_batch({9});memory[0x1000c]=1;
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn+1);},"caller differs");
    check(finishes==0);
    owned_batch({9});memory[0x1000c]=1;memory[owner+0x68]=0;
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"owner/address");check(finishes==0);
    owned_batch({9});memory[0x1000c]=1;memory[image+d::Plan+0x18]=image+0x301541;
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"Finish slot changed");check(finishes==0);
    owned_batch({9});memory[0x1000c]=1;memory[0x10008]=3;
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"type/completion");check(finishes==0);
    owned_batch({9});memory[0x1000c]=1;actual_thread=8;
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"game thread");check(finishes==0);
    owned_batch({9});memory[0x1000c]=1;control.entered=false;
    rejects([&]{d::finish_call(reinterpret_cast<void*>(0x10000),image+d::PumpFinishReturn);},"admitted outer");check(finishes==0);
    owned_batch({9});memory[0x1000c]=1;original_raises=true;
    rejects(pump_native,"original failed");
    check(finishes==1&&d::finish_entered==1&&d::finish_returned==0&&memory[0x1000e]==0&&native_deletes==0&&native_runs==0);
    check(d::first_error=="original failed");
