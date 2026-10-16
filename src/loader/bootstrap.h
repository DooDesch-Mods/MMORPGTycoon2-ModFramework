#pragma once
#include <windows.h>
#include <cstdint>
struct MT2FW_Bootstrap {
    uint32_t size = sizeof(MT2FW_Bootstrap);
    uint32_t version = 1;
    const wchar_t* profile;
    const wchar_t* game;
    uint32_t isolation_flags;
    uint32_t process_id;
};
