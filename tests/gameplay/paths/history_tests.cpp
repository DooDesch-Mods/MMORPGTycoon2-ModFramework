#include "gameplay/paths/history_transition.h"
using mt2fw::mt2::gameplay::paths::detail::history_transition_cause;
int main(){
    if(history_transition_cause({1,2},{3},{1},{3,4},2,1,0,0))return 1;
    if(!history_transition_cause({1,2},{3},{1},{3,4},2,0,0,0))return 2;
    if(!history_transition_cause({1,2},{3},{1},{3,4},1,1,0,0))return 3;
    if(!history_transition_cause({1,2},{3},{},{3,4},2,1,0,0))return 4;
    if(!history_transition_cause({1,2},{3},{1},{3,3},2,1,0,0))return 5;
    if(!history_transition_cause({1,2},{3},{1},{3,4},2,1,1,0))return 6;
    if(!history_transition_cause({1,2},{3},{1},{3,4},2,1,0,5))return 7;
    if(!history_transition_cause({1,2},{3},{1},{4,3},2,1,0,0))return 8;
    std::vector<uintptr_t> full,trimmed;for(uintptr_t i=10;i<30;++i)full.push_back(i);trimmed=full;trimmed.erase(trimmed.begin());trimmed.push_back(30);
    if(history_transition_cause({1},full,{},trimmed,1,1,0,0))return 9;return 0;
}
