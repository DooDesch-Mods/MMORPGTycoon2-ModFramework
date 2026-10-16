#pragma once
#include "mt2fw/core.h"
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib,"bcrypt.lib")
#endif
namespace mt2fw {
inline std::string digest(const std::string& data) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    NTSTATUS result = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (result >= 0) result = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
    if (result >= 0) result = BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())), ULONG(data.size()), 0);
    unsigned char bytes[32]{};
    if (result >= 0) result = BCryptFinishHash(hash, bytes, sizeof(bytes), 0);
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (result < 0) throw std::runtime_error("SHA256 failed NTSTATUS=" + std::to_string(result));
    std::string text;
    for (auto byte : bytes) { text += "0123456789abcdef"[byte >> 4]; text += "0123456789abcdef"[byte & 15]; }
    return text;
#else
    throw std::runtime_error("This build has no SHA256 provider; Windows is required");
#endif
}
inline std::string file_digest(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot read file for SHA256: " + path_text(path));
    std::string bytes(std::istreambuf_iterator<char>{stream}, {});
    if (stream.bad()) throw std::runtime_error("SHA256 file read failed: " + path_text(path));
    return digest(bytes);
}
}
