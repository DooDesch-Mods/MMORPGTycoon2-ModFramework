#include "mt2fw/core.h"
#include "mt2fw/save_metadata.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace mt2fw;
int checks=0;
void expect(bool accepted,const char* cause){++checks;if(!accepted)throw std::runtime_error(cause);}
template<class F> void rejects(F fn){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}expect(failed,"expected explicit rejection");}
size_t messages(const Runtime& runtime,const std::string& message){size_t count=0;for(const auto& row:runtime.logs())if(row.message==message)++count;return count;}
int main(){
    auto root=fs::current_path()/"evidence"/("initial-save-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        auto dir=root/"mods/test.initial";fs::create_directories(dir);
        atomic_json(dir/"mod.json",{{"id","test.initial"},{"name","Initial fixture"},{"version","1.0.0"},{"api_version","1.0.0"},{"type","lua"},{"entry","main.lua"},{"capabilities",Json::array({"world.read"})},{"multiplayer","local"}});
        {std::ofstream script(dir/"main.lua");script<<"mt2.events.on('world_ready',function() mt2.log.info('ready-event') end)\nmt2.events.on('saved',function() mt2.log.info('saved-event') end)\n";expect(bool(script),"write fixture script");}
        Runtime runtime(root);runtime.load();auto& plugin=*runtime.plugins().at(0);
        expect(plugin.active,"fixture mod active");plugin.content_registered=true;
        const auto fingerprint=plugin.fingerprint;
        auto initial=runtime.capture_initial_save("Fresh");
        expect(native_initial_metadata(initial) && initial.at("epoch").is_null(),"explicit uninitialized phase");
        expect(initial.at("mods").empty() && initial.at("objects").empty(),"no fabricated initialized mod data or handles");
        expect(initial.at("required")==Json::array({fingerprint}),"required content captured at native savepoint");
        plugin.fingerprint["later"]="changed";
        expect(initial.at("required")==Json::array({fingerprint}),"required snapshot independent of subsequent change");plugin.fingerprint=fingerprint;
        expect(runtime.epoch==0 && messages(runtime,"ready-event")==0 && messages(runtime,"saved-event")==0,"capture changes no lifecycle state");
        rejects([&]{runtime.capture_save("Fresh");});
        rejects([&]{runtime.save_acknowledged(initial,"initial-hash");});
        expect(messages(runtime,"saved-event")==0,"marked snapshot cannot emit ordinary saved");
        store_save_metadata(root,initial,"initial-hash");
        fs::path metadata;size_t count=0;for(const auto& row:fs::directory_iterator(root/"saves")){metadata=row.path();++count;}
        expect(count==1,"one hash-keyed metadata record");auto readback=read_json(metadata);
        expect(readback.at("save")=="initial-hash" && native_initial_metadata(readback),"persisted initial phase and exact save identity");
        runtime.check_save("initial-hash");
        rejects([&]{runtime.capture_initial_save("Other");});
        runtime.world_ready("Fresh");
        expect(runtime.epoch==1 && messages(runtime,"ready-event")==1,"normal WorldReady occurs once after initial load");
        expect(plugin.world_data.empty() && runtime.object_bindings.empty(),"initial load restores no invented state");
        auto ready=runtime.capture_save("Fresh");
        expect(!native_initial_metadata(ready) && ready.at("epoch")==1 && ready.at("mods").contains("test.initial"),"next ready-world snapshot remains ordinary");
        runtime.saved("regular-hash");
        expect(messages(runtime,"saved-event")==1,"regular saved event unchanged");
        rejects([&]{runtime.capture_initial_save("Fresh");});
        runtime.world_leave();plugin.world_data={{"ambiguous",1}};
        rejects([&]{runtime.capture_initial_save("Next");});plugin.world_data=Json::object();
        runtime.object_bindings={{"current",{{"epoch",runtime.epoch}}}};
        rejects([&]{runtime.capture_initial_save("Next");});
        runtime.object_bindings={{"unknown",Json::object()}};
        rejects([&]{runtime.capture_initial_save("Next");});
        runtime.object_bindings={{"old",{{"epoch",1}}}};const auto stale=runtime.object_bindings;
        auto next=runtime.capture_initial_save("Next");
        expect(next.at("objects").empty() && runtime.object_bindings==stale,"stale previous-world bindings are neither copied nor mutated");
        for(const auto* field:{"epoch","mods","objects"}) {
            auto invalid=initial;
            if(std::string(field)=="epoch")invalid[field]=1;else invalid[field]={{"invented",1}};
            rejects([&]{store_save_metadata(root,invalid,"bad-hash");});
            invalid["save"]="initial-hash";atomic_json(metadata,invalid);
            rejects([&]{runtime.check_save("initial-hash");});
        }
        atomic_json(metadata,readback);plugin.active=false;
        rejects([&]{runtime.check_save("initial-hash");});plugin.active=true;
        runtime.check_save("initial-hash");
        expect(messages(runtime,"saved-event")==1,"load checks never manufacture a save acknowledgement");
        std::cout<<checks<<" initial save core conditions passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<" evidence="<<root<<'\n';return 1;}
}
