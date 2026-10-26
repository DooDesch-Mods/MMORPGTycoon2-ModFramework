#include "gameplay/regions/sample_grid.h"
#include <iostream>
#include <limits>

using namespace mt2fw;
using namespace mt2fw::mt2::gameplay::regions;
int main(){try{
    int checks=0,reads=0;auto check=[&](bool value){++checks;if(!value)throw std::runtime_error("Terrain sample assertion failed");};
    auto read=[&](size_t index){++reads;check(index==0||index==5);return float(index)*0.5f;};
    const Json points=Json::array({{{"column",2},{"row",1}},{{"column",0},{"row",0}},{{"column",2},{"row",1}}});
    auto values=sample_grid(3,2,points,read);check(values.size()==3&&reads==2);
    check(values[0]["index"]==5&&values[0]["height"]==2.5&&values[1]["index"]==0&&values[2]==values[0]);
    auto reject=[&](Json input){const int before=reads;bool failed=false;try{sample_grid(3,2,input,read);}catch(const std::exception& e){failed=std::string(e.what()).find("Terrain")!=std::string::npos;}check(failed&&reads==before);};
    reject(Json::array());reject(Json::object());reject(Json::array({{{"column",3},{"row",0}}}));reject(Json::array({{{"column",0},{"row",-1}}}));
    reject(Json::array({{{"column",0.5},{"row",0}}}));reject(Json::array({{{"column",true},{"row",0}}}));
    reject(Json::array({{{"column",std::numeric_limits<uint64_t>::max()},{"row",0}}}));
    reject(Json::array({{{"column",0},{"row",0},{"address",1}}}));
    auto partly_valid=points;partly_valid.push_back({{"column",3},{"row",0}});reject(partly_valid);
    Json large=Json::array();for(int i=0;i<129;++i)large.push_back(points[0]);reject(large);
    bool failed=false;try{sample_grid(3,2,Json::array({points[0]}),[](size_t){return std::numeric_limits<float>::quiet_NaN();});}catch(const std::exception& e){failed=std::string(e.what()).find("finite")!=std::string::npos;}check(failed);
    std::cout<<checks<<" sparse terrain sample checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
