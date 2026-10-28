#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace mt2fw::mt2::region_rng {
struct Value {
    std::array<uint64_t,4> words{};int32_t extra=0;
    bool operator==(const Value&)const=default;
};
struct Row {int32_t id=-1;Value value;bool operator==(const Row&)const=default;};
inline void need(bool value,const std::string& cause){if(!value)throw std::runtime_error("Region RNG "+cause);}
inline uint64_t initial_seed(uint64_t agreed,int32_t id){
    need(id>=0,"saved ID is negative");const auto seed=agreed^uint64_t(uint32_t(id));
    // If XOR is zero, all other valid IDs produce values below bit 63.
    return seed?seed:uint64_t(1)<<63;
}
inline void valid_rows(uint32_t mode,const std::vector<Row>& rows){
    need(mode==1||mode==2,"mode is unknown");
    need(mode==1?rows.empty():!rows.empty(),"mode and preserved row count differ");
    int32_t previous=-1;
    for(const auto& row:rows){
        need(row.id>previous,"saved IDs are negative, duplicated or unsorted");previous=row.id;
        need((row.value.words[0]|row.value.words[1]|row.value.words[2]|row.value.words[3])!=0,"preserved state is all zero");
    }
}
class Load {
    struct Receipt {Value before;bool returned=false,attempted=false,confirmed=false;};
    std::map<uintptr_t,Receipt> loaded_;
    std::map<int32_t,uintptr_t> owners_;
    std::map<int32_t,Value> expected_;
    uint32_t mode_=0;bool sealed_=false;
public:
    void arm(uint32_t mode,const std::vector<Row>& rows){
        need(!mode_,"load already armed");valid_rows(mode,rows);mode_=mode;
        for(const auto& row:rows)expected_.emplace(row.id,row.value);
    }
    bool active()const{return mode_&&!sealed_;}
    void constructed(uintptr_t object,Value value){
        need(active()&&object,"constructor return is outside armed lifetime");
        need(loaded_.emplace(object,Receipt{value,true}).second,"constructor receiver repeated");
    }
    template<class Seed>
    void census(const std::map<int32_t,uintptr_t>& owners,Seed&& seed){
        need(active()&&!owners.empty(),"root census is empty or outside load");
        if(!owners_.empty()){need(owners_==owners,"root saved census changed");return;}
        std::set<uintptr_t> objects;
        for(const auto& [id,object]:owners){
            need(id>=0&&object&&objects.insert(object).second,"root identity is invalid or duplicated");
            auto found=loaded_.find(object);need(found!=loaded_.end()&&found->second.returned,"root Region lacks original RNG constructor return");
            if(mode_==1)expected_.emplace(id,seed(id));
            else need(expected_.contains(id),"root saved ID is absent from preserved snapshot");
        }
        need(expected_.size()==owners.size(),"preserved snapshot and root census counts differ");owners_=owners;
    }
    Value attempt(int32_t id,uintptr_t object,Value actual){
        need(active()&&owners_.contains(id)&&owners_.at(id)==object,"Finish receiver differs from saved root identity");
        auto& receipt=loaded_.at(object);need(receipt.returned&&!receipt.attempted,"restore was already attempted or lacks constructor return");
        need(receipt.before==actual,"state changed before original root Finish");receipt.attempted=true;return expected_.at(id);
    }
    void confirm(int32_t id,uintptr_t object,Value actual){
        need(active()&&owners_.contains(id)&&owners_.at(id)==object,"restore acknowledgement receiver differs");
        auto& receipt=loaded_.at(object);need(receipt.attempted&&!receipt.confirmed,"restore acknowledgement repeated or unattempted");
        need(actual==expected_.at(id),"restore readback differs; no replay");receipt.confirmed=true;
    }
    void unchanged(int32_t id,uintptr_t object,Value actual)const{
        need(owners_.contains(id)&&owners_.at(id)==object&&loaded_.at(object).confirmed,"readback lacks confirmed restore");
        need(actual==expected_.at(id),"original load consumed restored state");
    }
    template<class Read>std::vector<Row> seal(const std::map<int32_t,uintptr_t>& owners,Read&& read){
        need(active()&&owners_==owners,"bootstrap census changed or seal repeated");std::vector<Row> rows;
        for(const auto& [id,object]:owners_){const auto actual=read(object);unchanged(id,object,actual);rows.push_back({id,actual});}
        sealed_=true;return rows;
    }
};
}
