#pragma once
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include "npc_saved_identity.h"

namespace mt2fw::mt2::determinism {
// A load receipt belongs to the exact immutable fixture. It does not assign
// persistent identity from a current array index after loading has finished.
class NpcReceipt {
public:
    using Bytes=std::vector<uint8_t>;
    using Read=std::function<Bytes(uintptr_t,size_t)>;
    using Keys=std::map<uintptr_t,std::string>;
    static constexpr const char* fixture="3b4f0014ac86678f30e78d944b0d6f2fed9524fe95f083e1fb768355c1fb0fc5";
private:
    struct Row {uintptr_t pointer;uint64_t uid;int32_t type;bool operator==(const Row&)const=default;};
    struct Frame {uintptr_t array,reader,context,stream,file;uint64_t thread;int32_t declared,kind;};
    Read memory;uintptr_t image;std::mutex mutex;
    bool armed=false,complete=false;std::string path,failure;
    uintptr_t bound_mode=0,bound_context=0,bound_map=0;
    uint64_t next=0;
    std::map<uint64_t,Frame> pending;
    std::map<uint64_t,std::vector<uint64_t>> stacks;
    std::map<uintptr_t,std::vector<Row>> arrays;
    Keys identities;std::map<uintptr_t,int32_t> array_types;
    struct Port {NpcReceipt& owner;bool read(uintptr_t p,void* out,size_t n,std::string&){auto b=owner.bytes(p,n);std::memcpy(out,b.data(),n);return true;}};
    static void need(bool yes,const std::string& why){if(!yes)throw std::runtime_error("NPC load receipt: "+why);}
    Bytes bytes(uintptr_t p,size_t n){need(p&&n&&p<=UINTPTR_MAX-n,"invalid read extent");auto b=memory(p,n);need(b.size()==n,"short read address="+std::to_string(p)+" bytes="+std::to_string(n));return b;}
    template<class T>T at(uintptr_t p){auto b=bytes(p,sizeof(T));T v;std::memcpy(&v,b.data(),sizeof(T));return v;}
    void type(uintptr_t p,uintptr_t ap){need(at<uintptr_t>(p)==image+ap,"address point differs at="+std::to_string(p)+" required_rva="+std::to_string(ap));}
    std::string string(uintptr_t p){const auto data=at<uintptr_t>(p),n=at<uint64_t>(p+8);need(n>0&&n<32768,"filename extent invalid");auto b=bytes(data,size_t(n+1));need(b.back()==0,"filename terminator differs");std::string s(b.begin(),b.end()-1);need(s.find('\0')==std::string::npos,"filename contains null");return s;}
    void chain(const Frame& f){
        need(at<uintptr_t>(image+0x13df320)==bound_mode,"Mode owner changed");type(bound_mode,0x16e6730);
        need(at<uintptr_t>(bound_mode+0xd8)==f.context&&f.context==bound_context,"load context differs");
        need(at<uintptr_t>(f.reader+8)==f.stream,"Reader stream changed");type(f.stream,0x17315c0);
        need(at<uintptr_t>(f.stream+0x18)==f.file,"Reader file changed");type(f.file,0x1742fd0);
        need(at<int32_t>(f.file+0x70)==5&&string(f.file+8)==path,"Reader source is not the pinned fixture");
    }
    std::vector<Row> rows(uintptr_t array,uintptr_t context){
        type(array,0x172de80);const auto data=at<uintptr_t>(array+8);const auto n=at<int32_t>(array+0x10),cap=at<int32_t>(array+0x14);
        need(n>=0&&n<=2&&cap>=n,"fixture NPC array bounds differ");
        std::vector<Row> result;std::set<uintptr_t> pointers;std::set<uint64_t> uids;
        for(int32_t i=0;i<n;++i){auto p=at<uintptr_t>(data+size_t(i)*8);type(p,0x1742980);
            auto uid=at<uint64_t>(p+0x18);need(uid&&pointers.insert(p).second&&uids.insert(uid).second,"duplicate or zero NPC identity");
            const auto actual_context=at<uintptr_t>(p+8);
            need(actual_context==context,"NPC context differs address="+std::to_string(p)+" actual="+std::to_string(actual_context)+" required="+std::to_string(context));const auto kind=at<int32_t>(p+0x80c);need(kind>=0&&kind<3,"NPC type invalid");result.push_back({p,uid,kind});}
        return result;
    }
    uint64_t before(uintptr_t array,uintptr_t reader,uintptr_t context,uint64_t thread,bool opened){
        std::lock_guard lock(mutex);if(!armed)return 0;need(failure.empty(),failure);need(!complete,"unexpected NPC load after completed receipt");need(opened,"fixture open has no acknowledgement");
        auto mode=at<uintptr_t>(image+0x13df320);type(mode,0x16e6730);need(context&&at<uintptr_t>(mode+0xd8)==context,"current Mode context differs");
        if(!bound_mode){bound_mode=mode;bound_context=context;}
        Frame f{array,reader,context,at<uintptr_t>(reader+8),0,thread,0,-1};f.file=at<uintptr_t>(f.stream+0x18);chain(f);
        Port port{*this};mt2mp::game::npc_saved_identity::ChildHeader header;std::string error;
        need(mt2mp::game::npc_saved_identity::child_header(port,reader,header,error),error);
        f.declared=header.count;f.kind=-1;
        for(int32_t k=0;k<3;++k)if(mt2mp::game::snapshot_npc_key::type_names[size_t(k)]==header.label)f.kind=k;
        need(f.kind>=0&&(f.declared==0||(f.kind==0&&f.declared==2)),"fixture declared NPC type/count differs");
        need(rows(array,context).empty(),"NPC array was not empty before load");need(!arrays.contains(array)&&pending.size()<128,"duplicate array or pending load budget exceeded");
        for(const auto& [id,other]:pending){(void)id;need(other.array!=array,"recursive load of same array");}
        const auto ticket=++next;pending.emplace(ticket,f);stacks[thread].push_back(ticket);return ticket;
    }
    void after(uint64_t ticket,uint64_t thread){
        if(!ticket)return;std::lock_guard lock(mutex);auto found=pending.find(ticket);need(found!=pending.end(),"load return has no entry");const auto f=found->second;
        need(thread==f.thread&&stacks.contains(thread)&&!stacks.at(thread).empty()&&stacks.at(thread).back()==ticket,"load return thread or nesting differs");chain(f);
        auto current=rows(f.array,f.context);need(current.size()==size_t(f.declared),"native child skip/count differs");
        for(const auto& row:current)need(row.type==f.kind,"returned NPC type differs from serialized section");
        need(arrays.emplace(f.array,std::move(current)).second,"duplicate returned array");array_types.emplace(f.array,f.kind);pending.erase(found);auto& stack=stacks.at(thread);stack.pop_back();if(stack.empty())stacks.erase(thread);
    }
    void fail(const std::string& cause){std::lock_guard lock(mutex);if(failure.empty())failure=cause;}
    Keys current(){
        need(armed&&failure.empty(),failure.empty()?"receipt is not armed":failure);need(pending.empty()&&stacks.empty(),"native loads remain incomplete");need(arrays.size()==363,"fixture requires363 returned owner arrays actual="+std::to_string(arrays.size()));
        need(at<uintptr_t>(image+0x13df320)==bound_mode,"completed Mode changed");type(bound_mode,0x16e6730);
        const auto completed_context=at<uintptr_t>(bound_mode+0xd8);
        need(completed_context==0,"completed load context was not retired actual="+std::to_string(completed_context)+" required=0");
        const auto state=at<uintptr_t>(bound_mode+0x58);type(state,0x16de920);need(at<uintptr_t>(image+0x13df2a0)==state,"current GameState differs");
        const auto world=at<uintptr_t>(state+0x160);type(world,0x174aef0);need(at<uintptr_t>(image+0x13df9c0)==world,"current World differs");
        const auto map=at<uintptr_t>(world+0x158);type(map,0x1742860);need(at<uintptr_t>(image+0x13df8c0)==map,"current Map differs");if(bound_map)need(bound_map==map,"bound Map changed");
        const auto shard=map+0x1c0;type(shard,0x174ad20);need(at<int32_t>(shard+0x48)==0,"root shard kind differs");
        const auto regions=map+0x230;type(regions,0x16f04e0);const auto data=at<uintptr_t>(regions+8);
        need(at<int32_t>(regions+0x10)==121&&at<int32_t>(regions+0x14)>=121,"saved Region count differs");
        std::set<int32_t> ids;std::set<uintptr_t> region_pointers,seen_arrays,seen_buildings;std::set<uint64_t> seen_uids;Keys keys;
        for(size_t i=0;i<121;++i){const auto region=at<uintptr_t>(data+i*8);type(region,0x1787930);const auto id=at<int32_t>(region+0x834);
            need(id>=0&&id<=120&&ids.insert(id).second&&region_pointers.insert(region).second,"fixture Region id or uniqueness differs");need(at<uintptr_t>(region+0x70)==shard,"Region root shard differs");
            const auto manager=at<uintptr_t>(region+0x998);type(manager,0x16e6aa0);
            need(at<uintptr_t>(manager+0xc8)==region,"NPC manager reciprocal Region differs");
            std::map<uintptr_t,int32_t> loaded_links;
            for(int32_t kind=0;kind<3;++kind){
                const auto array=manager+0x38+size_t(kind)*0x30;
                need(seen_arrays.insert(array).second&&arrays.contains(array),"NPC manager lacks unique load receipt");
                need(array_types.at(array)==kind,"NPC serialized section differs from current owner bucket");
                auto live=rows(array,0);need(live.size()==size_t(id==67&&kind==0?2:0),"saved fixture NPC owner count differs region="+std::to_string(id));
                need(live==arrays.at(array),"returned load order/pointer/UID changed");
                for(size_t child=0;child<live.size();++child){const auto& row=live[child];
                    need(row.type==kind&&seen_buildings.insert(row.pointer).second&&seen_uids.insert(row.uid).second,"global NPC identity repeats or type differs");
                    need(at<uintptr_t>(row.pointer+0x170)==shard&&at<uintptr_t>(row.pointer+0x178)==region,"NPC reciprocal owner differs");
                    loaded_links.emplace(row.pointer,child==0?34:39);
                }
            }
            Port port{*this};mt2mp::game::npc_saved_identity::Pool pool;std::string error;
            auto links=[&](uintptr_t p,int32_t& link,std::string& e){auto found=loaded_links.find(p);if(found==loaded_links.end()){e="NPC lacks immutable child-order link receipt";return false;}link=found->second;return true;};
            need(mt2mp::game::npc_saved_identity::observe(port,image,manager,links,pool,error),error);
            for(const auto& row:pool.records)keys.emplace(row.pointer,mt2mp::game::npc_saved_identity::key(fixture,id,row));
        }
        need(seen_arrays.size()==arrays.size()&&keys.size()==2,"fixture coverage differs");
        // The flat owned list is a subset, not the serialized owner list.
        const auto owned=at<uintptr_t>(map+0x358);const auto n=at<int32_t>(map+0x360),cap=at<int32_t>(map+0x364);need(n>=0&&n<=121&&cap>=n,"owned Region bounds differ");std::set<uintptr_t> flat;
        for(int32_t i=0;i<n;++i){auto p=at<uintptr_t>(owned+size_t(i)*8);need(region_pointers.contains(p)&&flat.insert(p).second,"owned Region is repeated or absent from saved owner list");}
        bound_map=map;return keys;
    }
public:
    NpcReceipt(Read read,uintptr_t base):memory(std::move(read)),image(base){need(base&&base<UINTPTR_MAX-0x2000000,"image invalid");}
    void arm(const std::string& virtual_path,const std::string& sha){std::lock_guard lock(mutex);need(!armed&&next==0,"receipt cannot rearm");need(sha==fixture&&virtual_path.starts_with("user/save/")&&virtual_path.find("..") ==std::string::npos,"fixture pin differs");path=virtual_path;armed=true;}
    void cancel(){std::lock_guard lock(mutex);armed=false;}
    template<class Original>void invoke(uintptr_t array,uintptr_t reader,uintptr_t context,uint64_t thread,bool opened,Original&& original){
        uint64_t ticket=0;
        try{ticket=before(array,reader,context,thread,opened);}catch(const std::exception& e){fail(e.what());}catch(...){fail("NPC receipt entry observer raised non-standard exception");}
        try{original();}catch(const std::exception& e){if(ticket)fail(std::string("NPC load original raised: ")+e.what());throw;}catch(...){if(ticket)fail("NPC load original raised non-standard exception");throw;}
        try{after(ticket,thread);}catch(const std::exception& e){fail(e.what());}catch(...){fail("NPC receipt return observer raised non-standard exception");}
    }
    Keys snapshot(){
        std::lock_guard lock(mutex);
        // Repeat every read after constructing the complete graph. This catches
        // changed fields and same-size roster swaps without any native callback.
        auto original=memory;std::vector<std::pair<uintptr_t,Bytes>> samples;
        memory=[&](uintptr_t p,size_t n){auto b=original(p,n);samples.emplace_back(p,b);return b;};
        Keys found;try{found=current();memory=original;}catch(...){memory=original;throw;}
        for(const auto& [p,b]:samples)need(original(p,b.size())==b,"owner/roster changed during final observation");
        if(complete)need(found==identities,"bound saved identity changed");else{identities=found;complete=true;}
        return found;
    }
    std::string error(){std::lock_guard lock(mutex);return failure;}
};
}
