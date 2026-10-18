#include <nlohmann/json.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
using Json=nlohmann::json;
using DWORD=uint32_t;
struct Region {const unsigned char* data;size_t size;};
std::vector<Region> memory_regions;
const void* forbidden=nullptr;
std::map<std::string,void*> panes;
std::string lookup_failure;
int lookups=0;
void require(bool value,const std::string& cause) {if(!value) throw std::runtime_error(cause);}
void check(DWORD code,const std::string& operation) {require(!code,operation+" exception="+std::to_string(code));}
template<class T> bool read(const void* object,size_t offset,T& output) {
    if(!object) return false;
    auto address=reinterpret_cast<uintptr_t>(object)+offset;
    if(reinterpret_cast<const void*>(address)==forbidden) return false;
    for(const auto& region:memory_regions) {
        const auto begin=reinterpret_cast<uintptr_t>(region.data);
        if(address>=begin && address-begin<=region.size && sizeof(T)<=region.size-(address-begin)) {
            std::memcpy(&output,reinterpret_cast<const void*>(address),sizeof(T)); return true;
        }
    }
    return false;
}
DWORD string_call(uintptr_t rva,void*,const char* name,void** output) {
    require(rva==0x81d860,"Unexpected native lookup target"); ++lookups;
    if(lookup_failure==name) return 123;
    auto found=panes.find(name); *output=found==panes.end()?nullptr:found->second; return 0;
}
#include "wizard_state.inc"
template<class T> void put(std::vector<unsigned char>& buffer,size_t offset,T value) {std::memcpy(buffer.data()+offset,&value,sizeof(T));}
const std::array<const char*,6> ids{"desktop_newgame_name_displayname","desktop_newgame_type_displayname","desktop_newgame_affinity_displayname","desktop_newgame_map_displayname","desktop_newgame_logo_displayname","desktop_newgame_summary_displayname"};
struct Memory {
    std::vector<unsigned char> window=std::vector<unsigned char>(0x700),carousel=std::vector<unsigned char>(0x500),layout=std::vector<unsigned char>(0x90);
    std::array<std::vector<unsigned char>,6> pages;
    Memory() {
        memory_regions.clear(); panes.clear(); forbidden=nullptr; lookup_failure.clear(); lookups=0;
        for(auto& page:pages) page.resize(0x300);
        memory_regions={{window.data(),window.size()},{carousel.data(),carousel.size()},{layout.data(),layout.size()}};
        for(size_t index=0;index<pages.size();++index) {
            memory_regions.push_back({pages[index].data(),pages[index].size()}); panes[ids[index]]=pages[index].data(); put(pages[index],0x280,window.data());
        }
        panes["multi"]=carousel.data(); put(window,0x548,carousel.data()); put(carousel,0x280,window.data());
        put(carousel,0x428,pages[0].data()); put(window,0x678,layout.data()); put(layout,0x80,int32_t(4));
    }
    Json query() {return probe_wizard_state("doodesch.multiplayer:state",window.data());}
};
int cases=0;
void rejected(Memory& memory,const std::string& expected) {
    try {memory.query();throw std::logic_error("Expected rejection: "+expected);}
    catch(const std::runtime_error& error) {require(std::string(error.what()).find(expected)!=std::string::npos,"Wrong rejection: "+std::string(error.what()));}
    ++cases;
}
int main() {
    try {
        for(size_t index=0;index<ids.size();++index) {
            Memory memory; put(memory.carousel,0x428,memory.pages[index].data()); auto result=memory.query();
            require(result.at("page")==int(index)+1 && result.at("page_id")==ids[index] && result.at("status")=="observed" && result.at("confirmed")==true && result.at("map_ready")==true && result.at("native_attempted")==false && result.at("world_creation_confirmed")==false && lookups==7,"Actual page/read-only receipt differs"); ++cases;
        }
        {Memory memory;put(memory.carousel,0x428,memory.layout.data());put(memory.layout,0x80,int32_t(4));rejected(memory,"page owner differs");}
        {Memory memory;panes.erase(ids[0]);rejected(memory,"matched=0 required=1");}
        {Memory memory;panes[ids[1]]=memory.pages[0].data();rejected(memory,"matched=2 required=1");}
        {Memory memory;put(memory.pages[0],0x280,memory.carousel.data());rejected(memory,"page owner differs");}
        {Memory memory;put(memory.carousel,0x280,memory.layout.data());rejected(memory,"Carousel identity or owner differs");}
        {Memory memory;panes["multi"]=memory.layout.data();rejected(memory,"Carousel identity or owner differs");}
        {Memory memory;lookup_failure=ids[2];rejected(memory,"exception=123");}
        {Memory memory;put(memory.window,0x688,memory.layout.data());require(memory.query().at("map_ready")==false,"Pending task admitted");++cases;}
        {Memory memory;put(memory.window,0x690,uint8_t(1));require(memory.query().at("map_ready")==false,"Dirty map admitted");++cases;}
        {Memory memory;put(memory.window,0x678,static_cast<void*>(nullptr));auto result=memory.query();require(result.at("map_ready")==false && result.at("map").at("regions").is_null(),"Absent layout admitted");++cases;}
        {Memory memory;put(memory.layout,0x80,int32_t(0));require(memory.query().at("map_ready")==false,"Empty regions admitted");++cases;}
        {Memory memory;put(memory.window,0x690,uint8_t(2));rejected(memory,"canonical boolean");}
        for(const auto offset:{size_t(0x688),size_t(0x690),size_t(0x678)}) {Memory memory;forbidden=memory.window.data()+offset;rejected(memory,"unreadable");}
        {Memory memory;forbidden=memory.layout.data()+0x80;rejected(memory,"region count is unreadable");}
        {Memory memory;forbidden=memory.carousel.data()+0x428;rejected(memory,"current page is unreadable");}
        std::cout<<cases<<" actual-body wizard state cases passed\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"error: "<<error.what()<<'\n';return 1;}
}
