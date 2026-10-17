#pragma once
#include "mt2fw/core.h"

namespace mt2fw::native_input {
inline size_t limit(const Json& item){
    if(!item.contains("max_bytes"))return 128;
    if(!item.at("max_bytes").is_number_integer())throw std::runtime_error("Native input max_bytes must be an integer from 1 to 1024");
    auto value=item.at("max_bytes").get<int64_t>();if(value<1||value>1024)throw std::runtime_error("Native input max_bytes must be an integer from 1 to 1024");return size_t(value);
}
inline std::string prefix(const std::string& value,size_t maximum){
    size_t result=0;
    for(size_t i=0;i<value.size();){
        auto first=static_cast<unsigned char>(value[i]);size_t count=first<0x80?1:first>=0xc2&&first<=0xdf?2:first>=0xe0&&first<=0xef?3:first>=0xf0&&first<=0xf4?4:0;
        if(!count||i+count>value.size())throw std::runtime_error("Native input contains invalid UTF-8");
        for(size_t j=1;j<count;++j)if((static_cast<unsigned char>(value[i+j])&0xc0)!=0x80)throw std::runtime_error("Native input contains invalid UTF-8");
        if(count>1){auto second=static_cast<unsigned char>(value[i+1]);if((first==0xe0&&second<0xa0)||(first==0xed&&second>=0xa0)||(first==0xf0&&second<0x90)||(first==0xf4&&second>=0x90))throw std::runtime_error("Native input contains invalid UTF-8");}
        if(first==0)throw std::runtime_error("Native input contains a null byte");
        i+=count;if(i<=maximum)result=i;
    }
    return value.substr(0,result);
}
inline void validate(const std::string& value,size_t maximum){if(value.size()>maximum)throw std::runtime_error("Native input value exceeds max_bytes="+std::to_string(maximum));prefix(value,maximum);}
}
