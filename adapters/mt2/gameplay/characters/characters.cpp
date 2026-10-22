#include "characters.h"
#include "records.h"
#include "pins.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <memory>
#include <set>
#include <vector>

// Original MIT framework adapter. Native layouts and call signatures are based on
// Multiplayer 8a13602 src/game/character_* , ability_* and costume_* evidence.
// No networking controller, replay hook or multiplayer lifetime is used here.
namespace mt2fw::mt2::gameplay::characters {
namespace {
using records::require;
struct Text {const char* data; size_t size;};
template<class T> void put(uintptr_t p, size_t offset, const T& value) { std::memcpy(reinterpret_cast<void*>(p + offset), &value, sizeof value); }
uint32_t boundary(void (*call)(void*), void* value) noexcept {
    __try { call(value); return 0; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
}
template<class F> void native(F&& call) {
    struct Invocation {std::remove_reference_t<F>* function; std::exception_ptr error;};
    Invocation invocation{&call, {}};
    auto invoke = [](void* p) {auto& value = *static_cast<Invocation*>(p); try {(*value.function)();} catch (...) {value.error = std::current_exception();}};
    auto fault = boundary(invoke, &invocation);
    require(!fault, "character native call failed SEH=" + std::to_string(fault));
    if (invocation.error) std::rethrow_exception(invocation.error);
}
template<class R, class... A> R call(Context& c, uintptr_t rva, A... args) {
    c.verify_original(rva);
    return reinterpret_cast<R(*)(A...)>(c.image + rva)(args...);
}
template<size_t N> void pins(Context& c, const Pin (&values)[N]) {
    for (const auto& p : values) c.verify_hash(p.rva, p.size, p.sha);
}
void ap(Context& c, uintptr_t object, uintptr_t expected) {
    require(object && c.read<uintptr_t>(object) == c.image + expected, "character native class differs");
}
std::vector<uintptr_t> pointers(Context& c, uintptr_t header, size_t maximum = 10000) {
    auto data = c.read<uintptr_t>(header); auto count = c.read<int32_t>(header + 8); auto capacity = c.read<int32_t>(header + 12);
    require(count >= 0 && size_t(count) <= maximum && capacity >= count && (!count || data), "character native array bounds differ");
    std::vector<uintptr_t> out; std::set<uintptr_t> seen;
    for (int32_t i = 0; i < count; ++i) {
        auto p = c.read<uintptr_t>(data + size_t(i) * 8);
        require(p && seen.insert(p).second, "character native array has null/duplicate ownership"); out.push_back(p);
    }
    require(data == c.read<uintptr_t>(header) && count == c.read<int32_t>(header + 8) && capacity == c.read<int32_t>(header + 12), "character native array changed while reading");
    return out;
}
bool flag(Context& c, uintptr_t address) { auto value = c.read<uint8_t>(address); require(value <= 1, "character native boolean differs"); return value != 0; }
struct Definition {uintptr_t object; int32_t kind, index; uint64_t uid; bool deleted;};
std::vector<Definition> definitions(Context& c) {
    c.require_world(); auto manager = c.singleton(0x13df7a0); ap(c, manager, 0x1732430);
    std::vector<Definition> out; std::set<uint64_t> uids;
    for (int32_t kind = 0; kind < 5; ++kind) {
        auto members = pointers(c, manager + 0x88 + size_t(kind) * 0x50);
        for (size_t i = 0; i < members.size(); ++i) {
            auto p = members[i]; ap(c, p, 0x1705990); auto uid = c.read<uint64_t>(p + 0x18); bool deleted = flag(c, p + 0x490);
            require(c.read<int32_t>(p + 0x2c) == kind && c.read<int32_t>(p + 0x30) == int32_t(i) &&
                    (uid || deleted) && (!uid || uids.insert(uid).second), "character definition lifetime/kind/index differs");
            out.push_back({p, kind, int32_t(i), uid, deleted});
        }
    }
    return out;
}
Json handle(Context& c, const Definition& d) {return {{"epoch", c.epoch}, {"uid", std::to_string(d.uid)}, {"kind", d.kind}, {"index", d.index}};}
Definition select(Context& c, const Json& value) {
    require(value.is_object() && value.at("epoch").is_number_integer() && value.at("epoch").get<uint64_t>() == c.epoch,
            "character definition handle has an expired world epoch");
    auto uid = records::text(value.at("uid"), 20); auto kind = records::integer(value.at("kind")); auto index = records::integer(value.at("index"));
    for (const auto& d : definitions(c)) if (d.kind == kind && d.index == index) {
        require(!d.deleted && d.uid && uid == std::to_string(d.uid), "character definition handle is deleted or its exact UID changed");
        require(c.resolve(value, 0x1705990) == d.object, "character definition UID registry differs from its manager slot"); return d;
    }
    throw std::runtime_error("character definition handle is absent from the current manager");
}
Json stats(Context& c, const Definition& d) {
    Json attributes = Json::array();
    for (size_t offset : {size_t(0x58), size_t(0x88)}) {
        auto members = pointers(c, d.object + offset); require(members.size() >= 3, "character attributes are incomplete"); Json values = Json::array();
        for (size_t i = 0; i < 3; ++i) { ap(c, members[i], 0x172e8d0); float v = c.read<float>(members[i] + 0x28); require(std::isfinite(v), "character attribute is non-finite"); values.push_back(v); }
        attributes.push_back(values);
    }
    float speed = c.read<float>(d.object + 0x120); require(std::isfinite(speed), "character speed is non-finite");
    auto phases = pointers(c, d.object + 0x160); int32_t health = 0;
    if (!phases.empty()) { ap(c, phases[0], 0x16ddb00); health = c.read<int32_t>(phases[0] + 0x58); }
    return {{"attributes", attributes}, {"speed", speed}, {"boss_phases", phases.size()}, {"boss_health", health}};
}
Json row(Context& c, const Definition& d) {
    Json out = {{"handle", handle(c, d)}, {"name", c.text(d.object + 0xb0, 1024)}, {"definition_name", c.text(d.object + 0x248, 1024)},
                {"deleted", d.deleted}, {"edited", flag(c, d.object + 0x491)}, {"released", flag(c, d.object + 0x492)}};
    auto parent = c.read<uintptr_t>(d.object + 0x4a0); out["parent"] = nullptr;
    if (parent && !d.deleted) {
        ap(c, parent, 0x1705990); require(!flag(c, parent + 0x490), "character variant has a deleted parent");
        out["parent"] = handle(c, {parent, c.read<int32_t>(parent + 0x2c), c.read<int32_t>(parent + 0x30), c.read<uint64_t>(parent + 0x18), false});
    }
    if (!d.deleted) out["stats"] = stats(c, d);
    return out;
}
constexpr std::array<size_t, 8> string_offsets{0x48, 0x68, 0x88, 0xa8, 0xc8, 0xf8, 0x118, 0x138};
Json ability_record(Context& c, uintptr_t p) {
    ap(c, p, 0x171c930); Json strings = Json::array();
    for (auto offset : string_offsets) strings.push_back(c.text(p + offset, 4096));
    Json out = {{"text", strings}, {"item_value", c.read<float>(p + 0xe8)}, {"item_kind", c.read<int32_t>(p + 0xec)},
                {"item_flag", flag(c, p + 0xf0)}, {"selectors", c.read<std::array<int32_t, 5>>(p + 0x15c)},
                {"color", c.read<std::array<float, 4>>(p + 0x170)}, {"timing", c.read<std::array<float, 3>>(p + 0x180)},
                {"final_values", c.read<std::array<float, 2>>(p + 0x1f8)}, {"level", c.read<int32_t>(p + 0x200)}, {"final_flag", flag(c, p + 0x204)}};
    Json flags = Json::array(); for (size_t i = 0; i < 5; ++i) flags.push_back(flag(c, p + 0x1f0 + i)); out["flags"] = flags;
    std::set<uintptr_t> seen;
    for (size_t group = 0; group < 2; ++group) {
        Json values = Json::array(); auto members = pointers(c, p + (group ? 0x1c8 : 0x198), 64);
        for (auto effect : members) {
            ap(c, effect, 0x1704e90); require(c.read<uint64_t>(effect + 0x18) && seen.insert(effect).second, "ability effect lifetime/ownership differs");
            values.push_back({{"integers", c.read<std::array<int32_t, 3>>(effect + 0x20)}, {"values", c.read<std::array<float, 2>>(effect + 0x2c)}, {"count", c.read<int32_t>(effect + 0x34)}});
        }
        out[group ? "effects" : "costs"] = values;
    }
    records::ability(out); return out;
}
struct Ability {uintptr_t object; uint64_t uid; int32_t slot;};
std::vector<Ability> abilities(Context& c, const Definition& owner) {
    std::vector<Ability> out; auto members = pointers(c, owner.object + 0x130);
    for (size_t i = 0; i < members.size(); ++i) {
        ap(c, members[i], 0x171c930); auto uid = c.read<uint64_t>(members[i] + 0x18);
        require(uid != 0, "ability has a zero full UID"); auto proxy = c.read<uintptr_t>(members[i] + 0x40);
        require(!proxy || c.read<uintptr_t>(proxy) == members[i], "ability weak proxy is expired");
        out.push_back({members[i], uid, int32_t(i)});
    }
    return out;
}
Json ability_handle(Context& c, const Definition& owner, const Ability& a) {
    return {{"owner", handle(c, owner)}, {"epoch", c.epoch}, {"uid", std::to_string(a.uid)}, {"slot", a.slot}};
}
Ability select_ability(Context& c, const Definition& owner, const Json& h) {
    require(h.at("epoch").get<uint64_t>() == c.epoch && h.at("owner") == handle(c, owner), "ability owner/epoch differs");
    auto slot = records::integer(h.at("slot")); auto uid = records::text(h.at("uid"), 20);
    for (const auto& a : abilities(c, owner)) if (a.slot == slot) { require(uid == std::to_string(a.uid), "ability slot was replaced or shifted"); require(c.resolve(h, 0x171c930) == a.object, "ability UID registry differs from its current owner slot"); return a; }
    throw std::runtime_error("ability handle is absent");
}
Json ability_rows(Context& c, const Definition& owner) {
    Json out = Json::array(); for (const auto& a : abilities(c, owner)) out.push_back({{"handle", ability_handle(c, owner, a)},
        {"display_slot", c.read<int32_t>(a.object + 0x208)}, {"record", ability_record(c, a.object)}}); return out;
}
Json pose(Context& c, uintptr_t p) {return {{"rotation", c.read<std::array<float, 4>>(p)}, {"position", c.read<std::array<float, 3>>(p + 0x10)}, {"scale", c.read<std::array<float, 3>>(p + 0x1c)}};}
Json costume(Context& c, uintptr_t p) {
    ap(c, p, 0x16c0210); Json parts = Json::array();
    for (auto part : pointers(c, p + 0x60, 128)) {
        ap(c, part, 0x1736c70); Json attachments = Json::array(), colors = Json::array();
        for (auto a : pointers(c, part + 0xd0, 128)) { ap(c, a, 0x17229b0); attachments.push_back({{"name", c.text(a + 0x18, 4096)}, {"pose", pose(c, a + 0x38)}}); }
        auto color_data = c.read<uintptr_t>(part + 0x100); auto count = c.read<int32_t>(part + 0x108); auto cap = c.read<int32_t>(part + 0x10c);
        require(count >= 0 && count <= 128 && cap >= count && (!count || color_data), "costume colors have invalid bounds");
        for (int32_t i = 0; i < count; ++i) colors.push_back(c.read<std::array<float, 4>>(color_data + size_t(i) * 16));
        parts.push_back({{"bone", c.text(part + 0x18, 4096)}, {"model", c.text(part + 0x38, 4096)}, {"pose", pose(c, part + 0x58)},
                         {"model_scale", c.read<float>(part + 0xc4)}, {"attachments", attachments}, {"colors", colors}});
    }
    Json out = {{"name", c.text(p + 0x18, 4096)}, {"skeleton", c.text(p + 0x38, 4096)}, {"flag", flag(c, p + 0xc0)}, {"parts", parts}};
    records::costume(out); return out;
}
std::vector<std::string> categories(Context& c, uintptr_t owner) {
    // CharacterDefinition is embedded at +0x38; its ordered category strings are +0xd8.
    auto data = c.read<uintptr_t>(owner + 0x110); auto count = c.read<int32_t>(owner + 0x118); auto cap = c.read<int32_t>(owner + 0x11c);
    require(count >= 0 && count <= 128 && cap >= count && (!count || data), "weapon category bounds differ");
    std::vector<std::string> out; for (int32_t i = 0; i < count; ++i) out.push_back(c.text(data + size_t(i) * 32, 4096)); return out;
}
void prepare_ability(Context& c, uintptr_t draft, const Json& value) {
    records::ability(value);
    for (size_t group = 0; group < 2; ++group) {
        const auto& desired = value.at(group ? "effects" : "costs"); size_t header = group ? 0x1c8 : 0x198;
        auto current = pointers(c, draft + header, 64);
        while (current.size() > desired.size()) {
            call<void>(c, group ? 0x14dd0 : 0x14d40, reinterpret_cast<void*>(draft), reinterpret_cast<void*>(current.back()));
            auto next = pointers(c, draft + header, 64); require(next.size() + 1 == current.size(), "ability native effect removal did not advance"); current = std::move(next);
        }
        while (current.size() < desired.size()) {
            auto effect = call<uintptr_t>(c, 0xa02670, size_t(0x38)); require(effect != 0, "ability native effect allocation failed");
            call<void>(c, 0x67e0, reinterpret_cast<void*>(effect)); call<void>(c, group ? 0x14d10 : 0x14ce0, reinterpret_cast<void*>(draft), reinterpret_cast<void*>(effect));
            auto next = pointers(c, draft + header, 64); require(next.size() == current.size() + 1, "ability native effect addition did not advance"); current = std::move(next);
        }
        for (size_t i = 0; i < desired.size(); ++i) {
            std::array<unsigned char, 0x38> source{}; auto p = uintptr_t(source.data());
            put(p, 0x20, records::array<3, int32_t>(desired[i].at("integers"))); put(p, 0x2c, records::array<2>(desired[i].at("values"))); put(p, 0x34, records::integer(desired[i].at("count")));
            call<void*>(c, 0x6820, reinterpret_cast<void*>(current[i]), source.data());
        }
    }
    for (size_t i = 0; i < 8; ++i) { auto s = records::text(value.at("text")[i]); Text text{s.data(), s.size()}; call<void>(c, 0x11bfce0, reinterpret_cast<void*>(draft + string_offsets[i]), &text); }
    put(draft, 0xe8, records::number(value.at("item_value"))); put(draft, 0xec, records::integer(value.at("item_kind"))); put(draft, 0xf0, uint8_t(value.at("item_flag").get<bool>()));
    put(draft, 0x15c, records::array<5, int32_t>(value.at("selectors"))); put(draft, 0x170, records::array<4>(value.at("color"))); put(draft, 0x180, records::array<3>(value.at("timing")));
    for (size_t i = 0; i < 5; ++i) put(draft, 0x1f0 + i, uint8_t(value.at("flags")[i].get<bool>()));
    put(draft, 0x1f8, records::array<2>(value.at("final_values"))); put(draft, 0x200, records::integer(value.at("level"))); put(draft, 0x204, uint8_t(value.at("final_flag").get<bool>()));
}
void transform(Context& c, uintptr_t p, const Json& value) {
    auto r = records::array<4>(value.at("rotation")); auto t = records::array<3>(value.at("position")); auto s = records::array<3>(value.at("scale"));
    call<void>(c, 0x9ff370, reinterpret_cast<void*>(p), r.data(), t.data(), s.data());
}
struct BorrowedCostume {
    std::array<unsigned char, 0xc8> object{};
    std::vector<std::array<unsigned char, 0x110>> parts;
    std::vector<uintptr_t> part_pointers;
    std::vector<std::vector<std::array<unsigned char, 0xa8>>> attachments;
    std::vector<std::vector<uintptr_t>> attachment_pointers;
    std::vector<std::vector<std::array<float, 4>>> colors;
    std::vector<std::string> strings;
    void string(uintptr_t p, size_t offset, const Json& value) { strings.push_back(records::text(value)); const auto& s = strings.back(); put(p, offset, s.data()); put(p, offset + 8, s.size()); put(p, offset + 16, s.size()); }
    static void array(uintptr_t p, size_t offset, uintptr_t data, size_t size) { put(p, offset, data); put(p, offset + 8, int32_t(size)); put(p, offset + 12, int32_t(size)); }
    void build(Context& c, const Json& value) {
        const auto& input = value.at("parts"); parts.resize(input.size()); part_pointers.resize(input.size()); attachments.resize(input.size()); attachment_pointers.resize(input.size()); colors.resize(input.size());
        // Every borrowed string keeps a stable allocation until native deep assignment returns.
        size_t count = 2 + input.size() * 2; for (const auto& part : input) count += part.at("attachments").size(); strings.reserve(count);
        auto p = uintptr_t(object.data()); put(p, 0, c.image + 0x16c0210); string(p, 0x18, value.at("name")); string(p, 0x38, value.at("skeleton")); put(p, 0xbc, 1.f); put(p, 0xc0, uint8_t(value.at("flag").get<bool>()));
        for (size_t i = 0; i < input.size(); ++i) {
            auto part = uintptr_t(parts[i].data()); const auto& source = input[i]; put(part, 0, c.image + 0x1736c70); string(part, 0x18, source.at("bone")); string(part, 0x38, source.at("model"));
            transform(c, part + 0x58, source.at("pose")); put(part, 0xc4, records::number(source.at("model_scale")));
            const auto& attached = source.at("attachments"); attachments[i].resize(attached.size()); attachment_pointers[i].resize(attached.size());
            for (size_t n = 0; n < attached.size(); ++n) { auto a = uintptr_t(attachments[i][n].data()); put(a, 0, c.image + 0x17229b0); string(a, 0x18, attached[n].at("name")); transform(c, a + 0x38, attached[n].at("pose")); attachment_pointers[i][n] = a; }
            for (const auto& color : source.at("colors")) colors[i].push_back(records::array<4>(color));
            array(part, 0xd0, uintptr_t(attachment_pointers[i].data()), attachment_pointers[i].size()); array(part, 0x100, uintptr_t(colors[i].data()), colors[i].size()); part_pointers[i] = part;
        }
        array(p, 0x60, uintptr_t(part_pointers.data()), part_pointers.size());
    }
};
void verify_stat(Context& c) {
    c.verify_hash(0x4a9f0, 531, "783f85ef829dc176527018608ba559ee11badb2b2916e3d5355ca7d95569aea2");
    c.verify_hash(0x97350, 120, "6fe9d0db120218bd614ce720f9a7d2963f5dbd9a25e5b44b954f4c7f33fdd775");
}
void delete_object(Context& c, uintptr_t p) {
    if (!p) return; auto vtable = c.read<uintptr_t>(p); auto destroy = c.read<uintptr_t>(vtable + 8);
    require(destroy >= c.image && destroy < c.image + 0x1300000, "character private receiver destructor is outside the pinned image");
    c.verify_original(destroy - c.image);
    reinterpret_cast<void(*)(void*)>(destroy)(reinterpret_cast<void*>(p));
}
struct PrivateAbility {
    alignas(16) std::array<unsigned char, 0x348> view{};
    alignas(16) std::array<unsigned char, 0x390> slot{};
    alignas(8) std::array<unsigned char, 0x78> record{};
    uintptr_t icon = 0, source = 0; uint64_t source_uid = 0; bool slot_constructed = false, record_constructed = false, source_constructed = false, icon_constructed = false, cleanup_attempted = false;
    void cleanup(Context& c) {
        require(!cleanup_attempted, "private ability cleanup already attempted; replay forbidden"); cleanup_attempted = true;
        // The native Slot owns its docked Icon and deletes it in its destructor.
        if (slot_constructed) { uintptr_t docked = 0; std::memcpy(&docked, slot.data() + 0x380, 8); require(!docked || docked == icon, "private ability Slot owns a foreign Icon"); call<void>(c, 0x568970, slot.data()); slot_constructed = false; if (docked) { icon = 0; icon_constructed = false; } }
        if (icon) { if (icon_constructed) delete_object(c, icon); else call<void>(c, 0x1264b60, reinterpret_cast<void*>(icon), size_t(0x410)); icon = 0; }
        if (source) { if (source_constructed) delete_object(c, source); else call<void>(c, 0x1264b60, reinterpret_cast<void*>(source), size_t(0x210)); source = 0; }
        if (record_constructed) { call<void>(c, 0x9676a0, record.data()); record_constructed = false; }
        if (source_uid) require(!call<uintptr_t>(c, 0x47ce20, source_uid), "private copied ability UID survived native cleanup");
    }
};
bool owns(const std::string& operation) {return operation.rfind("characters.", 0) == 0;}
struct ZStream {
    const uint8_t* next_in = nullptr; uint32_t avail_in = 0, total_in = 0;
    uint8_t* next_out = nullptr; uint32_t avail_out = 0, total_out = 0;
    const char* message = nullptr; void* state = nullptr; void* allocate = nullptr; void* release = nullptr; void* opaque = nullptr;
    int32_t data_type = 0; uint32_t adler = 0, reserved = 0;
};
static_assert(sizeof(ZStream) == 0x58);
std::optional<std::string> validate_import(Context& c, const std::vector<uint8_t>& compressed) {
    auto module = GetModuleHandleW(L"zlib1.dll"); require(module != nullptr, "game zlib1.dll is not loaded");
    auto init = reinterpret_cast<int(*)(ZStream*, const char*, int)>(GetProcAddress(module, "inflateInit_"));
    auto inflate = reinterpret_cast<int(*)(ZStream*, int)>(GetProcAddress(module, "inflate"));
    auto end = reinterpret_cast<int(*)(ZStream*)>(GetProcAddress(module, "inflateEnd"));
    auto version = reinterpret_cast<const char*(*)()>(GetProcAddress(module, "zlibVersion"));
    require(init && inflate && end && version, "game zlib public API is incomplete");
    std::array<std::pair<uintptr_t, uintptr_t>, 3> imports{{{0xa7f588, uintptr_t(init)}, {0xa7f598, uintptr_t(inflate)}, {0xa7f590, uintptr_t(end)}}};
    for (const auto& [rva, expected] : imports) {
        c.verify_original(rva, 6); require(c.read<uint16_t>(c.image + rva) == 0x25ff, "game zlib import thunk differs");
        auto displacement = c.read<int32_t>(c.image + rva + 2);
        require(c.read<uintptr_t>(uintptr_t(int64_t(c.image + rva + 6) + displacement)) == expected, "game zlib import ownership differs");
    }
    std::vector<uint8_t> expanded(32 * 1024 * 1024); ZStream stream;
    stream.next_in = compressed.data(); stream.avail_in = uint32_t(compressed.size()); stream.next_out = expanded.data(); stream.avail_out = uint32_t(expanded.size());
    int code = -1; native([&] {code = init(&stream, version(), sizeof stream);}); require(code == 0, "character import inflateInit failed code=" + std::to_string(code));
    std::string original;
    try {native([&] {code = inflate(&stream, 4);}); require(code == 1, "character import inflate failed code=" + std::to_string(code));}
    catch (const std::exception& e) {original = e.what();}
    try {native([&] {code = end(&stream);}); require(code == 0, "character import inflateEnd failed code=" + std::to_string(code));}
    catch (const std::exception& e) {if (!original.empty()) original += "; "; original += e.what();}
    require(original.empty(), original);
    require(!stream.avail_in && stream.total_in == compressed.size() && stream.total_out && stream.total_out <= expanded.size() &&
            stream.avail_out == expanded.size() - stream.total_out, "character import did not consume one complete bounded zlib stream");
    expanded.resize(stream.total_out); return records::saved_definition(expanded);
}
struct ImportImage {
    alignas(8) std::array<unsigned char, 0x20> image{};
    alignas(8) std::array<unsigned char, 0x38> borrowed{};
    bool image_created = false, borrowed_created = false, cleanup_attempted = false, image_unknown = false;
    void cleanup(Context& c) {
        require(!cleanup_attempted, "character import cleanup already attempted; replay forbidden"); cleanup_attempted = true;
        std::string original;
        if (borrowed_created) {
            try {native([&] {call<void>(c, 0xa065d0, borrowed.data());}); borrowed_created = false;}
            catch (const std::exception& e) {original = e.what();}
        }
        if (image_unknown) {if (!original.empty()) original += "; "; original += "character import image construction/encoding is unknown; unsafe destruction was skipped";}
        if (image_created && !image_unknown) {
            try {native([&] {call<void>(c, 0xa113d0, image.data());}); image_created = false;}
            catch (const std::exception& e) {if (!original.empty()) original += "; "; original += e.what();}
        }
        require(original.empty(), original);
    }
};
Json import_prefab(Context& c, const Definition& owner, const Json& args, bool& attempted) {
    auto compressed = records::compressed(records::text(args.at("compressed_record"), 60000));
    pins(c, character_import_pins); pins(c, character_rename_pins); auto expected_name = validate_import(c, compressed);
    c.verify_hash(0xd8e690, 64, "b8c87e439b35d59b00b827db8154cd2db3225e1ecef22da0beb2285497e6e017");
    c.verify_hash(0x75fa40, 64, "583454048954ee8c3c57c88fa6cffa8fc45d2c851ae21b75bf610eeffa46eb42");
    c.verify_hash(0x7f80a0, 128, "71c4b1105268ad20e522a8a6e4e93aa0afede1a62423e25dc2b8986656c1b01c");
    Text name{"info_charactertype", 18}; uintptr_t outer = 0;
    native([&] {outer = call<uintptr_t>(c, 0x81aa90, &name);});
    require(outer != 0, "native character design component is not initialized"); ap(c, outer, 0x173a7f0);
    auto view = c.read<uintptr_t>(outer + 0x4e8); ap(c, view, 0x1736770);
    auto saved = c.read<uintptr_t>(view + 0x460); auto saved_base = c.read<uintptr_t>(view + 0x458); auto pane = c.read<uintptr_t>(view + 0x3b8); ap(c, pane, 0x1731f40);
    auto notice = c.read<uintptr_t>(pane + 0x358); auto editor = c.read<uintptr_t>(view + 0x360);
    auto multi = c.read<uintptr_t>(view + 0x358); std::string page;
    if (multi) {auto active_page = c.read<uintptr_t>(multi + 0x410); if (active_page) page = c.text(active_page + 0x1b0, 1024);}
    if (saved && editor && page == "general_edit_name") require(c.text(editor + 0x2d0, 1024) == c.text(saved + 0xb0, 1024), "character name editor has unsaved input");
    auto costume_editor = c.read<uintptr_t>(view + 0x3b0);
    if (costume_editor) {
        ap(c, costume_editor, 0x1722a70); auto live = c.read<uintptr_t>(costume_editor + 0x318), draft = c.read<uintptr_t>(costume_editor + 0x320);
        if (live && draft) require(costume(c, live) == costume(c, draft), "character costume editor has unsaved input");
    }
    // The normal UI creates this named component before Show/Raise. No visible
    // window or human input is required to use its complete original importer.
    auto before = definitions(c); ImportImage input; bool presentation_changed = false; std::string original, acknowledgment;
    uintptr_t notification = 0;
    attempted = true;
    try {
        try {native([&] {call<void>(c, 0xa10c40, input.image.data(), uint32_t(512), uint32_t(512)); input.image_created = true;});}
        catch (...) {input.image_unknown = true; throw;}
        std::array<float, 4> opaque{0, 0, 0, 1};
        try {native([&] {call<void>(c, 0xa11640, input.image.data(), opaque.data()); call<void>(c, 0xa066d0, input.borrowed.data(), compressed.data(), int32_t(compressed.size())); input.borrowed_created = true; call<void>(c, 0x8814e0, input.image.data(), input.borrowed.data());});}
        catch (...) {input.image_unknown = true; throw;}
        uintptr_t store = 0; native([&] {store = call<uintptr_t>(c, 0x880c90, input.image.data());});
        require(store != 0, "character import original image roundtrip returned no Store");
        std::string roundtrip_error;
        try {
            ap(c, store, 0x1749500); auto buffer = c.read<uintptr_t>(store + 8); auto written = c.read<uintptr_t>(store + 0x28); auto read = c.read<uintptr_t>(store + 0x20);
            require(buffer && read == buffer && written >= buffer && written - buffer == compressed.size(), "character import image roundtrip extent differs");
            std::vector<uint8_t> actual(compressed.size()); c.read_bytes(buffer, actual.data(), actual.size()); require(actual == compressed, "character import image roundtrip bytes differ");
        } catch (const std::exception& e) {roundtrip_error = e.what();}
        try {native([&] {call<void>(c, 0xa06600, reinterpret_cast<void*>(store));});}
        catch (const std::exception& e) {if (!roundtrip_error.empty()) roundtrip_error += "; "; roundtrip_error += e.what();}
        require(roundtrip_error.empty(), roundtrip_error);
        // This factory does not read its receiver. It constructs an unparented
        // native NotificationPane, whose text provides the original outcome.
        native([&] {notification = call<uintptr_t>(c, 0xd8e690, static_cast<void*>(nullptr));});
        require(notification != 0, "character import private outcome pane allocation failed"); ap(c, notification, 0x171f240);
        presentation_changed = true;
        native([&] {put(pane, 0x358, notification); put(view, 0x460, owner.object);});
        native([&] {call<void>(c, 0x689510, reinterpret_cast<void*>(view), input.image.data());});
        acknowledgment = c.text(notification + 0x2c8, 4096);
    } catch (const std::exception& e) {original = e.what();}
    if (presentation_changed) {
        try {
            // Detach the owned outcome pane before any native presentation work.
            native([&] {put(pane, 0x358, notice);});
            if (saved) {
                native([&] {call<void>(c, 0x689100, reinterpret_cast<void*>(view), reinterpret_cast<void*>(saved));});
                require(c.read<uintptr_t>(view + 0x460) == saved && c.read<uintptr_t>(view + 0x458) == saved_base, "character import native selection restoration differs");
                if (multi && !page.empty()) {Text label{page.data(), page.size()}; native([&] {call<void>(c, 0x756470, reinterpret_cast<void*>(multi), &label);});}
            }
            require(c.read<uintptr_t>(pane + 0x358) == notice, "character import native notice restoration differs");
        }
        catch (const std::exception& e) {if (!original.empty()) original += "; "; original += "character import presentation restoration failed: "; original += e.what();}
    }
    if (notification) {
        try {
            require(c.read<uintptr_t>(pane + 0x358) != notification, "character import outcome pane remains referenced; its allocation was retained");
            native([&] {call<void>(c, 0xbf9d30, reinterpret_cast<void*>(notification));});
        } catch (const std::exception& e) {if (!original.empty()) original += "; "; original += "character import outcome cleanup failed: "; original += e.what();}
    }
    try {input.cleanup(c);} catch (const std::exception& e) {if (!original.empty()) original += "; "; original += e.what();}
    require(original.empty(), original);
    auto after = definitions(c); require(before.size() == after.size(), "character import changed global definition count");
    for (size_t i = 0; i < before.size(); ++i) require(before[i].object == after[i].object && before[i].uid == after[i].uid && before[i].deleted == after[i].deleted,
        "character import changed a definition lifetime");
    require(c.read<uint64_t>(owner.object + 0x18) == owner.uid && !flag(c, owner.object + 0x490), "character import retired its selected owner");
    require(acknowledgment == "{design_character_prefab_import_success}", "character import original outcome differs: " + (acknowledgment.empty() ? std::string("no acknowledgment") : acknowledgment));
    require((!expected_name || c.text(owner.object + 0xb0, 1024) == *expected_name) && flag(c, owner.object + 0x491), "character import authored name/edited readback differs");
    return {{"character", row(c, owner)}, {"appearance", costume(c, c.read<uintptr_t>(owner.object + 0xf0))}, {"abilities", ability_rows(c, owner)}, {"payload_roundtrip", true}, {"native_acknowledgment", acknowledgment}, {"presentation_restored", bool(saved)}, {"presentation_initialized", !saved}};
}
}

std::optional<Json> query(Context& c, const std::string& operation, const Json& args) {
    if (operation == "characters.list") {
        Json out = Json::array(); bool deleted = args.value("include_deleted", false);
        for (const auto& d : definitions(c)) if (deleted || !d.deleted) out.push_back(row(c, d)); return out;
    }
    if (operation != "characters.get" && operation != "characters.stats" && operation != "characters.appearance" && operation != "characters.abilities") return std::nullopt;
    c.require_world(); auto d = select(c, args.at("handle"));
    if (operation == "characters.get") return row(c, d);
    if (operation == "characters.stats") return stats(c, d);
    if (operation == "characters.abilities") return ability_rows(c, d);
    return Json{{"record", costume(c, c.read<uintptr_t>(d.object + 0xf0))}, {"weapon_categories", categories(c, d.object)}};
}

bool action(Context& c, const std::string& operation, const Json& args, bool& attempted, Json& result, std::string& cause) {
    if (!owns(operation)) return false;
    static const std::set<std::string> supported{"characters.create", "characters.create_variant", "characters.remove", "characters.rename", "characters.set_stat", "characters.set_appearance", "characters.set_weapon_category", "characters.create_ability", "characters.copy_ability", "characters.remove_ability", "characters.dock_ability", "characters.set_ability", "characters.import"};
    if (!supported.contains(operation)) return false;
    attempted = false; cause.clear();
    try {
        c.require_world(); auto owner = select(c, args.at("handle")); auto before = row(c, owner);
        if (operation == "characters.create") {
            auto kind = args.contains("kind") ? records::integer(args.at("kind")) : owner.kind;
            require(kind >= 0 && kind < 5 && !c.read<uintptr_t>(owner.object + 0x4a0), "character creation requires a base collection owner and kind 0 through 4");
            auto collection = c.read<uintptr_t>(owner.object + 0x498); ap(c, collection, 0x173a6f0);
            c.verify_hash(0x9b7a0, 770, "ea6fe59e2880c0f105429dcbb30b8319d9cc5580a009e7ac694818f68602397e");
            auto initial = definitions(c); uint64_t id = 0; attempted = true;
            native([&] {id = call<uint64_t>(c, 0x9b7a0, reinterpret_cast<void*>(collection), kind);});
            auto created_kind = int32_t(uint32_t(id)); auto created_index = int32_t(uint32_t(id >> 32));
            for (const auto& d : definitions(c)) if (d.kind == created_kind && d.index == created_index && !d.deleted) {
                require(d.kind == kind && c.read<uintptr_t>(d.object + 0x498) == collection && !c.read<uintptr_t>(d.object + 0x4a0) &&
                    std::none_of(initial.begin(), initial.end(), [&](const auto& old) {return !old.deleted && old.uid == d.uid;}), "native character creation did not publish a new owned lifetime");
                result = row(c, d); return true;
            }
            throw std::runtime_error("native character creation returned no matching global type");
        }
        if (operation == "characters.create_variant") {
            pins(c, character_membership_pins); require(!c.read<uintptr_t>(owner.object + 0x4a0), "only a base character type can create a variant");
            auto name = records::text(args.at("name"), 1024); require(!name.empty(), "character name cannot be empty"); Text input{name.data(), name.size()}; bool used = false;
            native([&] {used = call<bool>(c, 0x9e690, reinterpret_cast<void*>(c.singleton(0x13df7a0)), &input);}); require(!used, "character name is already in use");
            auto initial = definitions(c); int32_t ordinal = -1; attempted = true;
            native([&] { ordinal = call<int32_t>(c, 0x97a90, reinterpret_cast<void*>(owner.object), &input); });
            require(ordinal >= 0, "native variant creation returned a negative ordinal");
            for (const auto& d : definitions(c)) if (!d.deleted && c.read<uintptr_t>(d.object + 0x4a0) == owner.object && c.text(d.object + 0xb0, 1024) == name) {
                bool old = std::any_of(initial.begin(), initial.end(), [&](const auto& a) {return !a.deleted && a.uid == d.uid;}); require(!old, "variant readback retained an existing lifetime");
                result = row(c, d); result["variant_ordinal"] = ordinal; return true;
            }
            throw std::runtime_error("native variant creation returned without a matching new definition");
        }
        if (operation == "characters.remove") {
            pins(c, character_membership_pins); auto collection = c.read<uintptr_t>(owner.object + 0x498); ap(c, collection, 0x173a6f0);
            bool member = false;
            for (auto record : pointers(c, collection + 0x48 + size_t(owner.kind) * 0x30)) {
                ap(c, record, 0x1740fa0);
                if (c.read<int32_t>(record + 0x18) == owner.kind && c.read<int32_t>(record + 0x1c) == owner.index) member = true;
            }
            require(member, "character is absent from its current owning collection");
            attempted = true; native([&] {call<void>(c, 0x9bb90, reinterpret_cast<void*>(collection), reinterpret_cast<void*>(owner.object));});
            require(flag(c, owner.object + 0x490) && !c.read<uint64_t>(owner.object + 0x18), "native character removal did not retire the definition UID");
            result = {{"removed", true}, {"handle", args.at("handle")}}; return true;
        }
        if (operation == "characters.rename") {
            pins(c, character_rename_pins); auto name = records::text(args.at("name"), 1024); require(!name.empty(), "character name cannot be empty");
            if (name == before.at("name").get<std::string>()) {result = before; return true;}
            Text input{name.data(), name.size()}; bool used = false;
            native([&] { used = call<bool>(c, 0x9e690, reinterpret_cast<void*>(c.singleton(0x13df7a0)), &input); }); require(!used, "character name is already in use");
            attempted = true; native([&] {call<void>(c, 0x94dd0, reinterpret_cast<void*>(owner.object), &input);});
            result = row(c, owner); require(result.at("name") == name && result.at("definition_name") == name && result.at("edited") == true, "native SetName readback differs"); return true;
        }
        if (operation == "characters.set_stat") {
            verify_stat(c); auto field = records::text(args.at("field"), 16); float value = records::number(args.at("value"));
            require(value >= 0 && value <= 1000000.f, "character stat is outside native editor bounds");
            int index = field == "health" ? 0 : field == "mana" ? 1 : field == "rage" ? 2 : field == "speed" ? 3 : -1;
            require(index >= 0, "character stat must be health, mana, rage or speed");
            auto expected = stats(c, owner); std::array<uintptr_t, 2> attributes{}; uintptr_t phase = 0;
            if (index < 3 && !(index == 0 && owner.kind == 4)) {
                for (size_t i = 0; i < 2; ++i) attributes[i] = pointers(c, owner.object + (i ? 0x88 : 0x58))[size_t(index)];
                expected["attributes"][0][index] = value; expected["attributes"][1][index] = index == 0 ? value * 2.f : value;
            } else if (index == 3) expected["speed"] = value;
            else {auto phases = pointers(c, owner.object + 0x160); require(!phases.empty(), "boss health requires a native first phase"); phase = phases[0]; expected["boss_health"] = int32_t(value);}
            attempted = true; native([&] {
                if (index == 3) put(owner.object, 0x120, value);
                else if (index == 0 && owner.kind == 4) {if (phase) put(phase, 0x58, int32_t(value));}
                else for (size_t i = 0; i < 2; ++i) call<void>(c, 0x4a9f0, reinterpret_cast<void*>(attributes[i]), index == 0 && i ? value * 2.f : value);
                call<void>(c, 0x97350, reinterpret_cast<void*>(owner.object));
            });
            result = stats(c, owner); require(result == expected, "native character stat/synchronized variant readback differs"); return true;
        }
        if (operation == "characters.set_appearance") {
            auto desired = args.at("record"); records::costume(desired); pins(c, costume_edit_pins); auto live = c.read<uintptr_t>(owner.object + 0xf0); ap(c, live, 0x16c0210);
            BorrowedCostume input; attempted = true;
            native([&] {input.build(c, desired); call<void>(c, 0x94b80, reinterpret_cast<void*>(owner.object)); call<void*>(c, 0x29f40, reinterpret_cast<void*>(live), input.object.data()); call<void>(c, 0x2d020, reinterpret_cast<void*>(live)); call<void>(c, 0x94bd0, reinterpret_cast<void*>(owner.object));});
            result = costume(c, live); require(result == desired, "native costume assignment/notification readback differs"); return true;
        }
        if (operation == "characters.set_weapon_category") {
            pins(c, costume_edit_pins); auto category = records::text(args.at("category")); require(!category.empty() && args.at("enabled").is_boolean(), "weapon category/enabled input differs"); bool enabled = args.at("enabled").get<bool>();
            auto expected = categories(c, owner.object); auto found = std::find(expected.begin(), expected.end(), category);
            if (enabled && found == expected.end()) expected.push_back(category); else if (!enabled && found != expected.end()) {*found = expected.back(); expected.pop_back();}
            Text input{category.data(), category.size()}; attempted = true; native([&] {call<void>(c, enabled ? 0x91060 : 0x910e0, reinterpret_cast<void*>(owner.object + 0x38), &input);});
            auto actual = categories(c, owner.object); require(actual == expected, "native weapon category readback differs"); result = actual; return true;
        }
        if (operation == "characters.set_ability") {
            pins(c, ability_edit_pins); auto a = select_ability(c, owner, args.at("ability")); auto desired = args.at("record"); records::ability(desired); require(!desired.at("effects").empty(), "an authored ability must retain an effect");
            attempted = true; native([&] {prepare_ability(c, a.object, desired); call<void>(c, 0x14e90, reinterpret_cast<void*>(a.object)); call<void>(c, 0x97210, reinterpret_cast<void*>(owner.object));});
            result = ability_record(c, a.object); require(result == desired, "native ability normalization/owner notification readback differs"); return true;
        }
        if (operation == "characters.import") {
            result = import_prefab(c, owner, args, attempted); return true;
        }
        pins(c, ability_membership_pins); auto initial = ability_rows(c, owner); auto current = abilities(c, owner);
        int32_t level = args.contains("level") ? records::integer(args.at("level")) : 1; int32_t display = args.contains("display_slot") ? records::integer(args.at("display_slot")) : -1;
        require(level >= 1 && level <= 100 && display >= -1 && display < 12, "ability level/display slot is outside native authoring bounds");
        bool create = operation == "characters.create_ability", copy = operation == "characters.copy_ability", remove = operation == "characters.remove_ability";
        if (create || copy) require(current.size() < 12, "native character ability limit is 12");
        Ability selected{}; Json prefab;
        if (copy) {auto source_owner = select(c, args.at("ability").at("owner")); selected = select_ability(c, source_owner, args.at("ability")); prefab = ability_record(c, selected.object);}
        else if (!create) selected = select_ability(c, owner, args.at("ability"));
        auto objects = std::make_unique<PrivateAbility>(); std::string text = "NewAbility Level " + std::to_string(level); bool accepted = false; bool cleaned = false;
        attempted = true;
        try {
            native([&] {
                if (create) {put(uintptr_t(objects->view.data()), 0x340, owner.object); call<void>(c, 0x967370, objects->record.data(), text.c_str()); objects->record_constructed = true;}
                else {
                    uintptr_t prototype = selected.object;
                    if (copy) { objects->source = call<uintptr_t>(c, 0xa02670, size_t(0x210)); require(objects->source != 0, "private ability allocation failed"); call<void>(c, 0x15350, reinterpret_cast<void*>(objects->source)); objects->source_constructed = true; objects->source_uid = c.read<uint64_t>(objects->source + 0x18); prepare_ability(c, objects->source, prefab); prototype = objects->source; }
                    objects->icon = call<uintptr_t>(c, 0xa02670, size_t(0x410)); require(objects->icon != 0, "private ability Icon allocation failed");
                    call<void>(c, 0x565ad0, reinterpret_cast<void*>(objects->icon), reinterpret_cast<void*>(prototype), copy ? nullptr : reinterpret_cast<void*>(owner.object)); objects->icon_constructed = true;
                    if (!remove) {call<void>(c, 0x566940, objects->slot.data(), int32_t(96), int32_t(96), reinterpret_cast<void*>(owner.object)); objects->slot_constructed = true; put(uintptr_t(objects->slot.data()), 0x348, display); put(uintptr_t(objects->slot.data()), 0x34c, level);}
                }
                if (create) accepted = call<bool>(c, 0x651990, objects->view.data(), objects->record.data());
                else if (remove) {call<void>(c, 0x565400, reinterpret_cast<void*>(objects->icon)); accepted = true;}
                else {call<void>(c, 0x567d70, objects->slot.data(), reinterpret_cast<void*>(objects->icon), 0.f, false); accepted = true;}
            });
            native([&] {objects->cleanup(c);}); cleaned = true;
        } catch (const std::exception& failure) {
            std::string original = failure.what();
            if (!cleaned && !objects->cleanup_attempted) {try {native([&] {objects->cleanup(c);});} catch (const std::exception& cleanup) {original += "; private ability cleanup failed: "; original += cleanup.what();}}
            throw std::runtime_error(original);
        }
        require(accepted, "native ability transaction did not accept the command");
        result = ability_rows(c, owner);
        if (create || copy) {
            require(result.size() == initial.size() + 1, "native ability creation/copy count readback differs");
            for (size_t i = 0; i < initial.size(); ++i) require(result[i] == initial[i], "native ability creation/copy changed an existing slot");
            const auto& made = result.back(); auto record = made.at("record");
            require(made.at("display_slot") == -1 && record.at("level") == (create && owner.kind == 4 ? 1 : level), "native ability creation/copy level/display readback differs");
            if (copy) {
                auto title = record.at("text")[0].get<std::string>(); require(!title.empty(), "native ability copy generated an empty name");
                record["text"][0] = prefab.at("text")[0]; record["level"] = prefab.at("level");
                require(record == prefab, "native ability copy changed authored prototype values");
            }
        }
        else if (remove) {
            require(result.size() + 1 == initial.size(), "native ability removal count readback differs");
            for (const auto& a : abilities(c, owner)) require(a.uid != selected.uid, "removed ability UID remains owned");
            uintptr_t registered = 0; native([&] {registered = call<uintptr_t>(c, 0x47ce20, selected.uid);}); require(!registered, "removed ability UID remains registered");
            auto expected = initial; expected.erase(expected.begin() + selected.slot);
            for (size_t i = 0; i < expected.size(); ++i) {expected[i]["handle"]["slot"] = i; require(expected[i] == result[i], "native ability removal changed another owned prototype");}
        } else {
            auto expected = initial; expected[size_t(selected.slot)]["record"]["level"] = level; expected[size_t(selected.slot)]["display_slot"] = display;
            require(result == expected, "native ability dock changed unrelated prototype fields or slots");
        }
        return true;
    } catch (const std::exception& e) {cause = e.what(); return false;}
}
}
