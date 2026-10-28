struct SwapCadence {
    // Intervals are classified by both endpoints. Focus transitions are not
    // attributed to either steady foreground or steady background presentation.
    struct Bucket {
        uint64_t native=0,retained=0,intervals=0,total_ns=0;
        std::array<uint64_t,9> gaps{};
        void gap(uint64_t ns){
            static constexpr uint64_t bounds[]={16666667,33333334,50000000,100000000,250000000,500000000,1000000000,2000000000};
            size_t i=0;while(i<8&&ns>bounds[i])++i;
            ++gaps[i];++intervals;total_ns+=ns;
        }
        Json status()const{return {{"native",native},{"retained",retained},{"intervals",intervals},{"total_ns",total_ns},{"gap_bins",gaps}};}
    };
    std::array<Bucket,4> buckets{},native_buckets{};
    uint64_t last_ns=0,last_native_ns=0,clock_errors=0;
    int last_focus=-1,last_native_focus=-1;
    static uint64_t now_ns(){return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
    void record_into(std::array<Bucket,4>& rows,uint64_t& last,int& prior,uint64_t now,bool retained,int focus){
        auto& frames=rows[size_t(focus)];if(retained)++frames.retained;else ++frames.native;
        if(last){
            if(now<=last)++clock_errors;
            else rows[size_t(focus==prior?focus:3)].gap(now-last);
        }
        last=now;prior=focus;
    }
    void record(uint64_t now,bool retained,bool focused,bool minimized){
        const int focus=minimized?2:(focused?0:1);
        record_into(buckets,last_ns,last_focus,now,retained,focus);
        if(!retained)record_into(native_buckets,last_native_ns,last_native_focus,now,false,focus);
    }
    Json status(uint64_t now)const{return {{"schema",1},{"sample_monotonic_ns",now},{"last_success_monotonic_ns",last_ns},
        {"last_focus",last_focus},{"clock_errors",clock_errors},
        {"gap_upper_bounds_ns",{16666667,33333334,50000000,100000000,250000000,500000000,1000000000,2000000000,nullptr}},
        {"foreground",buckets[0].status()},{"background",buckets[1].status()},{"minimized",buckets[2].status()},{"transition",buckets[3].status()},
        {"native_cadence",{{"last_success_monotonic_ns",last_native_ns},{"last_focus",last_native_focus},
            {"foreground",native_buckets[0].status()},{"background",native_buckets[1].status()},{"minimized",native_buckets[2].status()},{"transition",native_buckets[3].status()}}},
        {"scope","successful ordered SDL swap returns; retained pixels can repeat; not GPU/display completion"}};}
};
SwapCadence swap_cadence;
void observe_successful_swap(bool result,bool retained){
    if(!result)return;
    const auto flags=SDL_GetWindowFlags(window);
    swap_cadence.record(SwapCadence::now_ns(),retained,(flags&SDL_WINDOW_INPUT_FOCUS)!=0,(flags&SDL_WINDOW_MINIMIZED)!=0);
}
