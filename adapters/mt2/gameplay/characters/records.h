#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

// Original MIT framework implementation. Native field and saved-format evidence:
// MMORPGTycoon2-Multiplayer 8a13602, src/game/{ability_edit_runtime.inc,
// costume_record_reader.h, costume_native_view.h, character_import_payload.h}.
namespace mt2fw::mt2::gameplay::characters::records {
using Json = nlohmann::json;
inline void require(bool condition, const std::string& cause) {
    if (!condition) throw std::runtime_error(cause);
}
inline std::string text(const Json& value, size_t maximum = 4096) {
    require(value.is_string(), "character text must be a string");
    auto out = value.get<std::string>();
    require(out.size() <= maximum && out.find('\0') == std::string::npos,
            "character text exceeds its bound or contains a NUL");
    return out;
}
inline float number(const Json& value) {
    require(value.is_number(), "character scalar must be a number");
    const auto out = value.get<float>();
    require(std::isfinite(out), "character scalar must be finite");
    return out;
}
inline int32_t integer(const Json& value) {
    require(value.is_number_integer(), "character integer must be integral");
    if (value.is_number_unsigned()) require(value.get<uint64_t>() <= INT32_MAX, "character integer exceeds int32");
    else require(value.get<int64_t>() >= INT32_MIN && value.get<int64_t>() <= INT32_MAX, "character integer exceeds int32");
    return value.get<int32_t>();
}
template<size_t N, class T = float> std::array<T, N> array(const Json& value) {
    require(value.is_array() && value.size() == N, "character fixed array has the wrong size");
    std::array<T, N> out{};
    for (size_t i = 0; i < N; ++i) {
        if constexpr (std::is_same_v<T, float>) out[i] = number(value[i]);
        else if constexpr (std::is_same_v<T, int32_t>) out[i] = integer(value[i]);
        else { require(value[i].is_boolean(), "character flag must be boolean"); out[i] = value[i].get<bool>(); }
    }
    return out;
}
inline void ability(const Json& value) {
    require(value.is_object(), "ability record must be a table");
    const auto& strings = value.at("text");
    require(strings.is_array() && strings.size() == 8, "ability text requires eight strings");
    for (const auto& s : strings) text(s);
    number(value.at("item_value")); integer(value.at("item_kind"));
    require(value.at("item_flag").is_boolean() && value.at("final_flag").is_boolean(), "ability flags must be boolean");
    array<5, int32_t>(value.at("selectors")); array<4>(value.at("color"));
    array<3>(value.at("timing")); array<5, bool>(value.at("flags"));
    array<2>(value.at("final_values")); integer(value.at("level"));
    for (const char* key : {"costs", "effects"}) {
        const auto& rows = value.at(key);
        require(rows.is_array() && rows.size() <= 64, "ability effect count exceeds 64");
        for (const auto& row : rows) {
            auto ints = array<3, int32_t>(row.at("integers"));
            require(ints[0] >= 0 && ints[0] < 8, "ability effect kind is outside native range");
            array<2>(row.at("values")); integer(row.at("count"));
        }
    }
}
inline void pose(const Json& value) {
    array<4>(value.at("rotation")); array<3>(value.at("position")); array<3>(value.at("scale"));
}
inline void costume(const Json& value) {
    text(value.at("name")); text(value.at("skeleton"));
    require(value.at("flag").is_boolean(), "costume flag must be boolean");
    const auto& parts = value.at("parts");
    require(parts.is_array() && parts.size() <= 128, "costume parts exceed 128");
    for (const auto& part : parts) {
        text(part.at("bone")); text(part.at("model")); pose(part.at("pose")); number(part.at("model_scale"));
        const auto& attachments = part.at("attachments"); const auto& colors = part.at("colors");
        require(attachments.is_array() && attachments.size() <= 128 && colors.is_array() && colors.size() <= 128,
                "costume attachment/color arrays exceed 128");
        for (const auto& row : attachments) { text(row.at("name")); pose(row.at("pose")); }
        for (const auto& color : colors) array<4>(color);
    }
}
inline std::vector<uint8_t> compressed(const std::string& value) {
    require(!value.empty() && value.size() <= 60000 && !(value.size() & 1), "character import hex extent is invalid");
    auto digit = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
    std::vector<uint8_t> out; out.reserve(value.size() / 2);
    for (size_t i = 0; i < value.size(); i += 2) {
        auto high = digit(value[i]), low = digit(value[i + 1]);
        require(high >= 0 && low >= 0, "character import requires lowercase hexadecimal");
        out.push_back(uint8_t(high * 16 + low));
    }
    return out;
}
// Validate the entire saved Record before the game's RTTI loader receives it.
inline std::optional<std::string> saved_definition(const std::vector<uint8_t>& raw) {
    require(!raw.empty() && raw.size() <= 32 * 1024 * 1024, "character import expanded extent is invalid");
    struct Parser {
        const std::vector<uint8_t>& data; size_t pos = 0, rows = 0, tokens = 0;
        bool legacy = false; std::vector<std::string> strings; std::string name; bool named = false;
        uint32_t get(unsigned n, bool little = false) {
            require(n <= data.size() - pos, "character import Record is truncated");
            uint32_t v = 0;
            for (unsigned i = 0; i < n; ++i) { uint32_t b = data[pos++]; if (little) v |= b << (8 * i); else v = (v << 8) | b; }
            return v;
        }
        std::string string() {
            auto n = get(2); require(n <= data.size() - pos, "character import string is truncated");
            std::string s(reinterpret_cast<const char*>(data.data() + pos), n); pos += n;
            require(s.find('\0') == std::string::npos, "character import string contains a NUL"); return s;
        }
        std::pair<unsigned, std::string> token(bool label = false) {
            unsigned type = get(1); require(type <= (label ? 2u : 4u), "character import token kind is invalid");
            std::string s;
            if (type == 1 || type == 2) {
                if (legacy) { auto i = get(4); require(i < strings.size(), "character import string index is invalid"); s = strings[i]; }
                else s = string();
            } else if (type == 3) {
                uint32_t bits = get(4, true); float v; std::memcpy(&v, &bits, 4);
                require(std::isfinite(v), "character import float is non-finite");
            } else if (type == 4) get(4);
            return {type, s};
        }
        void node(unsigned depth) {
            require(depth <= 256 && rows < 100000, "character import Record ownership budget exceeded");
            auto label = token(true);
            if (!rows) require(label.first == 1 && label.second == "mmoCharacterDefinition", "character import root is not mmoCharacterDefinition");
            ++rows; auto n = get(4);
            require(n <= 1000000 - tokens && n <= data.size() - pos, "character import token budget exceeded");
            tokens += n;
            for (uint32_t i = 0; i < n; ++i) {
                auto value = token();
                if (depth == 1 && label.second == "name" && i == 0) {
                    require(!named && (value.first == 1 || value.first == 2) && value.second.size() <= 1024,
                            "character import name property is absent, duplicated or invalid");
                    name = value.second; named = true;
                }
            }
            auto children = get(4);
            require(children <= 100000 - rows && children <= (data.size() - pos) / 9, "character import child budget exceeded");
            while (children--) node(depth + 1);
        }
    } p{raw};
    auto version = p.string(); p.legacy = version == "RecordV1";
    require(p.legacy || version == "RecordV2", "character import version must be RecordV1 or RecordV2");
    if (p.legacy) {
        auto n = p.get(4); require(n <= 100000 && n <= (raw.size() - p.pos) / 2, "character import string table budget exceeded");
        while (n--) p.strings.push_back(p.string());
    }
    p.node(0); require(p.pos == raw.size(), "character import Record has trailing data");
    return p.named ? std::optional<std::string>(p.name) : std::nullopt;
}
}
