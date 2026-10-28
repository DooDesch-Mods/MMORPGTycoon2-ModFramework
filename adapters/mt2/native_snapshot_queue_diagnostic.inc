// Diagnose the failed hold while the native queue mutex owns pointer lifetime.
// Completed copied arrays can contain deleted tasks, so never inspect their objects.
Json queue_diagnostic(){
    Json result={{"tick",certificate.tick},{"order",certificate.order},{"nonce",certificate.nonce},
        {"phase",int(phase)},{"main_thread",thread},{"queues",Json::array()},{"sample_limit",16}};
    require(loading_dispatch::path_lock&&loading_dispatch::original_unlock,"Snapshot queue diagnostic lacks original mutex entries");
    const auto manager=tasks();loading_dispatch::path_lock(reinterpret_cast<void*>(manager+0xf0));
    struct Unlock {uintptr_t mutex;~Unlock(){loading_dispatch::original_unlock(reinterpret_cast<void*>(mutex));}} unlock{manager+0xf0};
    size_t sampled=0;uint64_t omitted=0;
    for(size_t offset:{size_t(0x28),size_t(0x40),size_t(0x58),size_t(0x70),size_t(0x88),size_t(0xa0),size_t(0xb8),size_t(0xd0)}){
        Json row={{"offset",offset},{"class",offset==0xd0?"copied_callbacks":"live"}};
        const auto count=read<int32_t>(manager+offset+8),capacity=read<int32_t>(manager+offset+12);
        const auto data=read<uintptr_t>(manager+offset);
        row["count"]=count;row["capacity"]=capacity;row["allocation"]=data;
        if(!count)continue;
        row["tasks"]=Json::array();
        const bool valid=count>=0&&capacity>=count&&capacity<=1000000&&data;
        row["bounds_valid"]=valid;
        if(offset==0xd0)row["consumed_save_receipt"]=loading_dispatch::save_completion::consumed_snapshot(manager);
        if(valid)for(int32_t i=0;i<count;++i){
            if(sampled==16){omitted+=uint64_t(count-i);break;}
            ++sampled;Json task={{"index",i}};
            try{
                const auto object=read<uintptr_t>(data+size_t(i)*8);task["pointer"]=object;
                if(offset==0xd0)task["header_read"]=false;
                else {
                    const auto ap=read<uintptr_t>(object);task["address_point"]=ap;
                    task["type"]=read<int32_t>(object+8);
                    task["cancel_byte"]=read<uint8_t>(object+0xc);task["orphan_byte"]=read<uint8_t>(object+0xd);
                    task["complete_byte"]=read<uint8_t>(object+0xe);task["header_read"]=true;
                    const bool native=ap>=image&&ap-image<0x2000000;task["native_address_point"]=native;
                    if(native){
                        task["address_point_rva"]=ap-image;
                        const auto run=read<uintptr_t>(ap+0x10),finish=read<uintptr_t>(ap+0x18);
                        task["run_address"]=run;task["finish_address"]=finish;
                        if(run>=image&&run-image<0x2000000)task["run_rva"]=run-image;
                        if(finish>=image&&finish-image<0x2000000)task["finish_rva"]=finish-image;
                    }
                }
            }catch(const std::exception& error){task["read_error"]=error.what();}
            row["tasks"].push_back(std::move(task));
        }
        result["queues"].push_back(std::move(row));
    }
    result["sampled"]=sampled;result["omitted"]=omitted;return result;
}
[[noreturn]] void reject_queue(const std::string& cause){
    Json evidence;
    try{evidence=queue_diagnostic();}
    catch(const std::exception& error){evidence={{"diagnostic_error",error.what()}};}
    runtime->log("steps","error","Snapshot queue hold rejected: "+cause+" evidence="+evidence.dump());
    throw std::runtime_error(cause);
}
