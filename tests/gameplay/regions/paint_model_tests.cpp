#include "gameplay/regions/paint_model.h"
#include <iostream>

using namespace mt2fw;
namespace model=mt2fw::mt2::gameplay::regions::paint_model;
int main(){try{
    auto require=[](bool value,const char* cause){if(!value)throw std::runtime_error(cause);};
    Json triangle=Json::array({{{"x",9007199254740993ll},{"z",0}},{{"x",9007199254740995ll},{"z",0}},{{"x",9007199254740993ll},{"z",2}}});
    Json stamp={{"position",{1,2,3}},{"radius",2},{"shape",Json::array({triangle})}};
    Json value=Json::array({stamp});
    auto parsed=model::parse(value);require(parsed.size()==1&&parsed[0].paths.size()==1&&parsed[0].paths[0][0].x==9007199254740993ll,"Native terrain coordinates lost integer precision");
    require(parsed[0].radius==2&&parsed[0].position==std::array<float,3>{1,2,3},"Native terrain stamp parameters changed");
    auto rejected=[&](const Json& input,const char* cause){bool failed=false;try{model::parse(input);}catch(const std::exception& error){failed=!std::string(error.what()).empty();}require(failed,cause);};
    auto invalid=value;invalid[0]["radius"]=1e-300;rejected(invalid,"Radius underflow reached the native brush");
    invalid=value;invalid[0]["shape"][0][0]["x"]=uint64_t(1)<<62;rejected(invalid,"Out-of-range Clipper coordinate was admitted");
    invalid=value;invalid[0]["shape"][0][0]["x"]=1.5;rejected(invalid,"Fractional Clipper coordinate was admitted");
    invalid=value;invalid[0]["shape"][0][0]["address"]=1234;rejected(invalid,"Undeclared polygon data was admitted");
    invalid=value;invalid[0]["shape"][0].erase(2);rejected(invalid,"Two-vertex polygon was admitted");
    invalid=value;invalid[0]["position"][0]=1000001;rejected(invalid,"Out-of-world terrain position was admitted");
    invalid=Json::array();for(size_t i=0;i<65;++i)invalid.push_back(value[0]);rejected(invalid,"Terrain stamp budget was ignored");
    std::cout<<"Terrain integer geometry, shape and native float budget checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}}
