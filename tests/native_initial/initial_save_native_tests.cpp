#include "mt2mp/net/snapshot_world_path.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace mt2mp::game::native {
std::array<unsigned char,1536> game_bytes{},mode_bytes{};
constexpr uintptr_t game_slot=0x13df8f0,mode_slot=0x13df320;
bool bound=true,game_type=true,mode_type=true,folder_readable=true;
std::string native_folder="Fresh";
void* singleton(uintptr_t slot){return slot==game_slot?game_bytes.data():mode_bytes.data();}
bool check_thread(std::string& error){if(!bound){error="wrong thread";return false;}return true;}
bool has_vtable(const void* pointer,uintptr_t type){return (pointer==game_bytes.data()&&type==0x1743730&&game_type)||(pointer==mode_bytes.data()&&type==0x16e6720&&mode_type);}
template<class T> bool read_at(const void* pointer,size_t offset,T& value){if(pointer!=game_bytes.data()&&pointer!=mode_bytes.data())return false;std::memcpy(&value,static_cast<const unsigned char*>(pointer)+offset,sizeof(T));return true;}
template<class T> void write(void* pointer,size_t offset,T value){std::memcpy(static_cast<unsigned char*>(pointer)+offset,&value,sizeof(T));}
bool read_game_string(const void* pointer,size_t offset,std::string& result){if(pointer!=game_bytes.data()||offset!=0x470||!folder_readable)return false;result=native_folder;return true;}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
#include "read_initial_save_world.inc"
void reset(){
    bound=game_type=mode_type=folder_readable=true;native_folder="Fresh";game_bytes.fill(0);mode_bytes.fill(0);
    write<void*>(game_bytes.data(),0x28,mode_bytes.data());write<void*>(mode_bytes.data(),0x58,game_bytes.data());write<int>(mode_bytes.data(),0x2c4,3);
}
int checks=0;
void expect(bool ok,const char* cause){++checks;if(!ok)throw std::runtime_error(cause);}
template<class Change> void rejects(Change change){reset();change();std::string folder="stale",error;expect(!read_initial_save_world(mode_bytes.data(),folder,error)&&folder.empty()&&!error.empty(),"guard must return an explicit failure with no folder");}
}
int main(){
    using namespace mt2mp::game::native;
    try {
        reset();std::string folder,error;
        expect(read_initial_save_world(mode_bytes.data(),folder,error)&&folder=="Fresh","exact current pre-ready state3 identity accepted");
        rejects([]{bound=false;});rejects([]{game_type=false;});rejects([]{mode_type=false;});
        rejects([]{write<void*>(game_bytes.data(),0x28,nullptr);});
        rejects([]{write<void*>(mode_bytes.data(),0x58,nullptr);});
        rejects([]{write<int>(mode_bytes.data(),0x2c4,0);});rejects([]{write<int>(mode_bytes.data(),0x2c4,4);});
        rejects([]{write<unsigned char>(mode_bytes.data(),0x304,1);});
        rejects([]{folder_readable=false;});rejects([]{native_folder="../Other";});
        reset();expect(!read_initial_save_world(nullptr,folder,error)&&folder.empty(),"foreign save receiver rejected");
        std::cout<<checks<<" initial native guard conditions passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
