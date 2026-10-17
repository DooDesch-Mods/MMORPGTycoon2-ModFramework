#pragma once
#include "mt2fw/core.h"
namespace mt2fw::document_ui {
void initialize();
Json request(const std::string& operation,const Json& arguments,const fs::path& profile);
void collect(const Runtime& runtime);
void shutdown();
}
