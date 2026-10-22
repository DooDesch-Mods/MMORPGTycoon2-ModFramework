#pragma once
#include "validation.h"
#include <bit>

namespace mt2fw::mt2::gameplay::dungeons::detail {
struct FillToken {int32_t type=0;uint32_t bits=0;std::string text;};
struct FillRecord {size_t parent=SIZE_MAX,index=0;FillToken label;std::vector<FillToken> tokens;};
inline std::vector<FillRecord> fill_records(const Json& value) {
    valid(value.is_array() && !value.empty() && value.size()<=2048,"Dungeon Fill requires 1..2048 portable Record rows");
    size_t bytes=0,count=0;std::vector<size_t> children(value.size());std::vector<FillRecord> out;
    const auto token=[&](const Json& item){
        valid(item.is_array() && item.size()==3,"Dungeon Fill token requires type,bits,text");FillToken t;t.type=integer(item[0],0,4,"Fill token type");
        valid(item[1].is_number_integer() && (item[1].is_number_unsigned() || item[1].get<int64_t>()>=0) && item[1].get<uint64_t>()<=UINT32_MAX,"Dungeon Fill token bits exceed uint32");t.bits=uint32_t(item[1].get<uint64_t>());
        t.text=item[2].get<std::string>();bytes+=t.text.size();valid(t.text.size()<=8192 && bytes<=65536 && t.text.find('\0')==std::string::npos,"Dungeon Fill text budget/NUL differs");
        valid((t.type==1 || t.type==2)?t.bits==0:t.text.empty(),"Dungeon Fill token payload kind differs");if(t.type==3)valid(std::isfinite(std::bit_cast<float>(t.bits)),"Dungeon Fill float is non-finite");return t;
    };
    for(size_t i=0;i<value.size();++i){const auto& item=value[i];valid(item.is_array() && item.size()==4 && item[0].is_number_integer() && item[1].is_number_integer() && item[3].is_array(),"Dungeon Fill exact Record row differs");
        valid(!item[0].is_number_unsigned() || item[0].get<uint64_t>()<=INT64_MAX,"Dungeon Fill parent index exceeds int64");const auto parent=item[0].get<int64_t>();valid(item[1].is_number_unsigned() || item[1].get<int64_t>()>=0,"Dungeon Fill child index is negative");const auto index=item[1].get<uint64_t>();
        valid((i==0 && parent==-1 && index==0) || (i && parent>=0 && uint64_t(parent)<i && index==children[size_t(parent)]++),"Dungeon Fill Record parent/order differs");
        FillRecord row;row.parent=i?size_t(parent):SIZE_MAX;row.index=size_t(index);row.label=token(item[2]);valid(row.label.type<=2,"Dungeon Fill Record label is not text");count+=item[3].size();valid(count<=16384,"Dungeon Fill token count exceeds 16384");
        for(const auto& t:item[3])row.tokens.push_back(token(t));out.push_back(std::move(row));
    }valid(out[0].label.type==1 && out[0].label.text=="mmoFill","Dungeon Fill root must be mmoFill");return out;
}
}
