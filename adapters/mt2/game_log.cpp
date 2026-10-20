#include "game_log.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace mt2fw::mt2 {
namespace {
// vsLog_ borrows pointer/length and copies the message into its own queue.
// The game's GCC string layout must not be replaced by an MSVC std::string.
struct GameStringView {
    const char* data;
    size_t length;
    size_t capacity;
    size_t reserved = 0;
};
static_assert(sizeof(GameStringView) == 32);
constexpr uintptr_t log_rva = 0xa1ad40;
constexpr uintptr_t worker_rva = 0x1c4eac0;
constexpr unsigned char log_prefix[] = {
    0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xc8,0,0,0
};
DWORD write_native(uintptr_t base, const GameStringView* message) {
    __try {
        if(std::memcmp(reinterpret_cast<void*>(base + log_rva), log_prefix, sizeof(log_prefix)))
            return ERROR_REVISION_MISMATCH;
        if(!*reinterpret_cast<void**>(base + worker_rva)) return ERROR_NOT_READY;
        using Log = void(*)(const char*, int, const GameStringView*);
        reinterpret_cast<Log>(base + log_rva)("MT2Framework", 0, message);
        return ERROR_SUCCESS;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
}
bool game_log(std::string_view message, std::string& error) {
    std::array<char,1025> text{};
    const auto length = (std::min)(message.size(), text.size() - 1);
    for(size_t i = 0; i < length; ++i) {
        const auto value = static_cast<unsigned char>(message[i]);
        text[i] = value < 32 || value == 127 ? ' ' : static_cast<char>(value);
    }
    GameStringView view{text.data(), length, length};
    const auto code = write_native(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)), &view);
    if(code) {
        error = "Original game log write failed, code=" + std::to_string(code);
        return false;
    }
    error.clear();
    return true;
}
}
