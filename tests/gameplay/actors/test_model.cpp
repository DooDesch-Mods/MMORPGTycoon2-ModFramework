#include "../../../adapters/mt2/gameplay/actors/model.h"
#include <iostream>
#include <functional>
#include <limits>
using namespace mt2fw::mt2::gameplay::actors::model;
int main(){
    int failures=0;auto check=[&](bool value,const char* cause){if(!value){++failures;std::cerr<<cause<<'\n';}};
    auto rejects=[&](std::function<void()> fn,const char* cause){bool failed=false;try{fn();}catch(const std::exception& e){failed=!std::string(e.what()).empty();}check(failed,cause);};
    Json phase={{"name","First"},{"health",50},{"vfx","fire"},{"color",{1,0,0,1}},{"abilities",{0,1}}};
    auto updated=phase_update(phase,{{"health",100},{"name","Second"}});check(updated.at("health")==100 && updated.at("name")=="Second","phase edits must apply requested fields");
    check(updated.at("abilities")==phase.at("abilities") && updated.at("vfx")==phase.at("vfx"),"phase edits must preserve unrequested fields");
    rejects([&]{phase_update(phase,{{"abilities",{2}}});},"phase fields cannot silently replace ownership");
    rejects([&]{phase_update(phase,Json::object());},"empty phase settings rejected");
    rejects([&]{color({1,2,3});},"phase color extent checked");
    rejects([&]{color({1,2,3,std::numeric_limits<double>::infinity()});},"phase color finite checked");
    rejects([&]{integer(Json(uint64_t(UINT64_MAX)));},"unsigned overflow rejected");
    check(reordered_index(0,3,3)==2 && reordered_index(2,0,3)==0 && reordered_index(1,2,3)==1,"phase drop boundaries map to final indices");
    rejects([&]{reordered_index(3,0,3);},"absent reorder source rejected");
    rejects([&]{reordered_index(0,4,3);},"absent reorder boundary rejected");
    Json transform={{"rotation",{0,0,0,1}},{"position",{1,2,3}},{"scale",{1,1,1}}};
    check(pose(transform).position[2]==3,"native pose component order preserved");
    rejects([&]{auto value=transform;value["rotation"]={0,0,0,0};pose(value);},"zero rotation rejected");
    rejects([&]{auto value=transform;value["scale"][0]=0;pose(value);},"zero scale rejected");
    rejects([&]{auto value=transform;value["position"][1]=std::numeric_limits<double>::infinity();pose(value);},"non-finite position rejected");
    Json money={{"cash",5000},{"present",true},{"record_cash",4000},{"income",500},{"expense",100},{"line_counts",std::array<int32_t,20>{}},{"line_income",std::array<int32_t,20>{}}};
    auto paid=charge(money,1000,6);check(paid["cash"]==4000 && paid["record_cash"]==3000 && paid["expense"]==1100 && paid["line_counts"][6]==1 && paid["line_income"][6]==-1000 && paid["income"]==500,"Boss billing uses exact SubtractCash line and sign");
    rejects([&]{auto value=money;value["expense"]=INT32_MAX;charge(value,1000,6);},"accounting overflow rejected before purchase");
    auto record=[](const char* owner,int slot,const char* destination){return Json{{"handle",{{"epoch",1},{"owner",{{"epoch",1},{"uid",owner}}},{"slot",slot},{"sequence",slot}}},{"destination",{{"epoch",1},{"uid",destination}}},{"name",std::to_string(slot)}};};
    Json quests=Json::array({record("a",2,"safe"),record("b",0,"gone"),record("a",0,"gone"),record("a",1,"safe"),record("gone",0,"safe")});
    auto survivors=surviving_quests(quests,"gone");check(survivors.size()==2 && survivors[0]["handle"]["slot"]==1 && survivors[1]["handle"]["slot"]==0 && survivors[0]["name"]=="2" && survivors[1]["name"]=="1","quest cascade preserves global order and reindexes owner slot order after swaps");
    check(surviving_quests(quests,"absent")==quests,"quest cascade preserves all unrelated authored records");
    Json stroke_quests=Json::array({record("a",2,"safe"),record("a",0,"zone_a"),record("b",0,"zone_b"),record("a",1,"zone_b")});
    auto erased=surviving_quests(surviving_quests(stroke_quests,"zone_a"),"zone_b");check(erased.size()==1 && erased[0]["name"]=="2" && erased[0]["handle"]["slot"]==0 && erased[0]["handle"]["sequence"]==0,"multi-Zone quest cascade preserves global record order and cumulative owner reindexing");
    check(surviving_quests(surviving_quests(stroke_quests,"zone_b"),"zone_a")==erased,"multi-Zone retirement order preserves the same final surviving quest ownership");
    Json contour=Json::array({Json::array({{{"x",0},{"z",0}},{{"x",100},{"z",0}},{{"x",100},{"z",100}}})});polygon(contour);
    rejects([&]{auto value=contour;value[0][0]["x"]=uint64_t(UINT64_MAX);polygon(value);},"Zone coordinate unsigned overflow rejected");
    rejects([&]{auto value=contour;value[0].erase(value[0].begin());polygon(value);},"Zone contour below three vertices rejected");
    rejects([&]{auto value=contour;value[0][0]["z"]=1.5;polygon(value);},"Zone polygon uses exact integral coordinates");
    std::cout<<"actor model checks failures="<<failures<<'\n';return failures?1:0;
}
