#pragma once
#include "context.h"
namespace mt2fw::mt2::gameplay {
const Json& operations();
Json query(Context&,const std::string& operation,const Json& arguments);
bool action(Context&,const std::string& operation,const Json& arguments,bool& attempted,Json& result,std::string& cause);
void validate_operation(const std::string& operation,const char* kind,const Json& arguments);
}
