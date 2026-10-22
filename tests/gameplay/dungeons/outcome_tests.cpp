#include "gameplay/dungeons/outcome.h"
#include "gameplay/dungeons/validation.h"
#include "gameplay/dungeons/fill_records.h"
#include "gameplay/dungeons/undo_commit.h"
#include <stdexcept>

using namespace mt2fw;
using mt2fw::mt2::gameplay::dungeons::detail::outcome;
using mt2fw::mt2::gameplay::dungeons::detail::integer;
using mt2fw::mt2::gameplay::dungeons::detail::integers;
using mt2fw::mt2::gameplay::dungeons::detail::color_input;
using mt2fw::mt2::gameplay::dungeons::detail::fill_records;
using mt2fw::mt2::gameplay::dungeons::detail::committed_history;
int main() {
    bool attempted=false;Json result={{"stale",true}};std::string cause;
    const auto rejected=outcome(result,cause,[&]() -> bool {throw std::runtime_error("expired handle");});
    if(rejected || attempted || !result.is_null() || cause!="expired handle")return 1;
    const auto unknown=outcome(result,cause,[&]() -> bool {attempted=true;throw std::runtime_error("native readback differs");});
    if(unknown || !attempted || !result.is_null() || cause!="native readback differs")return 2;
    const auto confirmed=outcome(result,cause,[&]() -> bool {result={{"confirmed",true}};return true;});
    if(!confirmed || !attempted || !cause.empty() || !result.at("confirmed").get<bool>())return 3;
    attempted=false;
    for(const Json value:{Json(uint64_t(4294967297)),Json(UINT64_MAX),Json(-1),Json(1.5),Json(true)}) {
        if(outcome(result,cause,[&](){integer(value,1,10,"count");attempted=true;return true;}) || attempted || cause.empty())return 4;
    }
    if(integer(Json(10),1,10,"count")!=10)return 5;
    if(outcome(result,cause,[&](){integers<3>(Json::array({1,2,3,4}),"slider");return true;}))return 6;
    if(outcome(result,cause,[&](){integers<3>(Json::array({1,2,UINT64_MAX}),"slider");return true;}))return 7;
    const Json color=Json::array({1.0,0.25,0.5,1.0});Json colors=Json::array({color,color,color,color});
    if(color_input(colors)[0][1]!=0.25f)return 8;
    colors[2].push_back(0);
    if(outcome(result,cause,[&](){color_input(colors);return true;}))return 9;
    const Json label=Json::array({1,0,"mmoFill"});const Json fill=Json::array({Json::array({-1,0,label,Json::array()})});
    if(fill_records(fill).size()!=1)return 10;
    auto wrong=fill;wrong[0][0]=UINT64_MAX;
    if(outcome(result,cause,[&](){fill_records(wrong);return true;}))return 11;
    wrong=fill;wrong[0][3].push_back(Json::array({5,0,""}));
    if(outcome(result,cause,[&](){fill_records(wrong);return true;}))return 12;
    wrong=fill;wrong[0][3].push_back(Json::array({3,uint64_t(0x7fc00000),""}));
    if(outcome(result,cause,[&](){fill_records(wrong);return true;}))return 13;
    wrong=fill;wrong.push_back(Json::array({1,0,Json::array({1,0,"color"}),Json::array()}));
    if(outcome(result,cause,[&](){fill_records(wrong);return true;}))return 14;
    const std::vector<uintptr_t> history{1,2},appended{1,2,3};
    if(!committed_history(history,appended,true,1,0,0))return 15;
    if(committed_history(history,appended,true,0,0,0))return 16;
    if(committed_history(history,appended,true,2,0,0))return 17;
    if(committed_history(history,appended,false,1,0,0))return 18;
    if(committed_history(history,appended,true,1,1,0))return 19;
    if(committed_history(history,appended,true,1,0,3))return 20;
    if(committed_history(history,{2,1,3},true,1,0,0))return 21;
    if(committed_history(history,{1,2,1},true,1,0,0))return 22;
    std::vector<uintptr_t> full,trimmed;for(uintptr_t i=1;i<=20;++i)full.push_back(i);
    trimmed=full;trimmed.erase(trimmed.begin());trimmed.push_back(21);
    if(!committed_history(full,trimmed,true,1,0,0))return 23;
    return 0;
}
