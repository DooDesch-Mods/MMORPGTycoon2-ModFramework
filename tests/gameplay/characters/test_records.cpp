#include "../../../adapters/mt2/gameplay/characters/records.h"
#include <functional>
#include <iostream>
#include <limits>

using namespace mt2fw::mt2::gameplay::characters::records;
namespace {
int failures = 0;
void expect(bool value, const char* name) {if (!value) {std::cerr << "error: " << name << '\n'; ++failures;}}
void rejects(const std::function<void()>& value, const char* name) {
    bool rejected = false;
    try {value();} catch (const std::exception& e) {rejected = !std::string(e.what()).empty();}
    expect(rejected, name);
}
Json ability_record() {
    return {{"text", std::array<std::string, 8>{"Strike", "", "", "", "", "", "", ""}}, {"item_value", 0}, {"item_kind", 0}, {"item_flag", false},
        {"selectors", std::array<int32_t, 5>{}}, {"color", std::array<float, 4>{1, 1, 1, 1}}, {"timing", std::array<float, 3>{}}, {"flags", std::array<bool, 5>{}},
        {"final_values", std::array<float, 2>{}}, {"level", 1}, {"final_flag", false}, {"costs", Json::array()},
        {"effects", Json::array({{{"integers", {1, 0, 0}}, {"values", {5, 0}}, {"count", 1}}})}};
}
std::vector<uint8_t> definition() {
    // Complete RecordV2 with one authored string property named name.
    return {0,8,'R','e','c','o','r','d','V','2',1,0,22,'m','m','o','C','h','a','r','a','c','t','e','r','D','e','f','i','n','i','t','i','o','n',
        0,0,0,0,0,0,0,1,1,0,4,'n','a','m','e',0,0,0,1,2,0,7,'F','i','x','t','u','r','e',0,0,0,0};
}
std::vector<uint8_t> legacy_definition() {
    std::vector<uint8_t> out;
    auto scalar = [&](uint32_t value, int bytes) {for (int i = bytes - 1; i >= 0; --i) out.push_back(uint8_t(value >> (i * 8)));};
    auto string = [&](const std::string& value) {scalar(uint32_t(value.size()), 2); out.insert(out.end(), value.begin(), value.end());};
    string("RecordV1"); scalar(3, 4); string("mmoCharacterDefinition"); string("name"); string("Fixture");
    scalar(1, 1); scalar(0, 4); scalar(0, 4); scalar(1, 4);
    scalar(1, 1); scalar(1, 4); scalar(1, 4); scalar(2, 1); scalar(2, 4); scalar(0, 4);
    return out;
}
}
int main() {
    auto record = ability_record(); ability(record);
    auto bad = record; bad["text"] = {"short"}; rejects([&] {ability(bad);}, "ability requires all eight authored strings");
    bad = record; bad["effects"][0]["integers"][0] = 8; rejects([&] {ability(bad);}, "ability effect enum bound");
    bad = record; bad["effects"][0]["values"][0] = std::numeric_limits<double>::infinity(); rejects([&] {ability(bad);}, "ability non-finite effect");
    rejects([&] {integer(Json(uint64_t(UINT64_MAX)));}, "unsigned integer cannot wrap into int32");
    rejects([&] {text(Json(std::string("a\0b", 3)));}, "embedded NUL text");
    expect(compressed("00abff") == std::vector<uint8_t>({0,171,255}), "hex byte conversion");
    rejects([&] {compressed("AB");}, "uppercase import hex rejected");
    rejects([&] {compressed("abc");}, "odd import hex rejected");
    auto raw = definition(); expect(saved_definition(raw) == "Fixture", "import name is extracted from complete RecordV2");
    expect(saved_definition(legacy_definition()) == "Fixture", "import string table is read from complete RecordV1");
    auto appended = raw; appended.push_back(0); rejects([&] {saved_definition(appended);}, "trailing import bytes rejected");
    for (size_t i = 0; i < raw.size(); ++i) {
        auto truncated = std::vector<uint8_t>(raw.begin(), raw.begin() + i);
        rejects([&] {saved_definition(truncated);}, "truncated import rejected");
    }
    auto foreign = raw; foreign[13] = 'x'; rejects([&] {saved_definition(foreign);}, "foreign RTTI root rejected");
    auto unnamed = raw; unnamed[46] = 'x'; expect(!saved_definition(unnamed), "valid unnamed Record does not invent expected authored text");
    Json costume_record = {{"name", "test"}, {"skeleton", "humanoid"}, {"flag", false}, {"parts", Json::array()}}; costume(costume_record);
    costume_record["parts"] = Json::array({{{"bone", "root"}, {"model", "test"}, {"pose", {{"rotation", {0,0,0,1}}, {"position", {0,0,0}}, {"scale", {1,1,1}}}}, {"model_scale", 1}, {"attachments", Json::array()}, {"colors", {{1,1,1,1}}}}}); costume(costume_record);
    costume_record["parts"][0]["pose"]["rotation"] = {0,0,0}; rejects([&] {costume(costume_record);}, "costume quaternion extent");
    std::cout << "character record checks failures=" << failures << '\n'; return failures ? 1 : 0;
}
