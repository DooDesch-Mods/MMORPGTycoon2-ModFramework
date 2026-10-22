#pragma once
#include "mt2fw/core.h"
#include <initializer_list>
#include <optional>
#include <type_traits>

namespace mt2fw::mt2::gameplay {
using CodeVerifier = std::function<bool(uint64_t,size_t,const std::string&,std::string&)>;
void configure_code_verifier(CodeVerifier verifier);
struct Context {
    uintptr_t image = 0;
    uint32_t thread = 0;
    uint64_t epoch = 0;
    void require_world() const;
    void require_mutation() const;
    void read_bytes(uintptr_t address,void* output,size_t bytes) const;
    template<class T>T read(uintptr_t address) const {
        static_assert(std::is_trivially_copyable_v<T>);
        T value{};read_bytes(address,&value,sizeof(value));return value;
    }
    std::string text(uintptr_t address,size_t maximum=256) const;
    uintptr_t singleton(uint64_t rva) const;
    void verify(uint64_t rva,std::initializer_list<unsigned char> bytes) const;
    void verify_hash(uint64_t rva,size_t bytes,const std::string& sha256) const;
    void verify_original(uint64_t rva,size_t bytes=32) const;
    uintptr_t resolve(const Json& handle,uint64_t expected_vtable_rva=0) const;
};
Context context(uint64_t epoch);
}
