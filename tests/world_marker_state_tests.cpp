#include "world_marker_state.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mt2fw::mt2::markers;
#define CHECK(value) do { if (!(value)) throw std::runtime_error("Check failed: " #value); } while (false)
template<class F> void rejects(F callback) { bool rejected = false; try { callback(); } catch (const std::exception&) { rejected = true; } CHECK(rejected); }
Json request(const char* id, Json epoch = "42") {
    return {{"action", "set"}, {"id", id}, {"epoch", epoch}, {"world", {1,2,3}}, {"name", "Peer"}, {"color", {1,0.5,0,1}}};
}
int main() {
    try {
        Store s; rejects([&] { s.request("a", request("peer")); });
        s.context(42, true);
        auto first = s.request("a", request("peer")); CHECK(first["status"] == "accepted");
        CHECK(s.entries.at({"a","peer"}).draws == 0 && !s.entries.at({"a","peer"}).visible);
        s.request("b", request("peer", 42)); CHECK(s.entries.size() == 2);
        auto bad = request("peer"); bad["world"][0] = std::numeric_limits<double>::infinity();
        rejects([&] { s.request("a", bad); }); CHECK(s.entries.at({"a","peer"}).revision == first["revision"]);
        bad = request("peer"); bad["color"][1] = 2; rejects([&] { s.request("a", bad); });
        bad = request("peer"); bad["world"][0] = 1e100; rejects([&] { s.request("a", bad); });
        for (Json value : {Json(-1), Json(1.5), Json("042"), Json("18446744073709551616"), Json("42x"), Json("+42")})
            rejects([&] { s.request("a", request("peer", value)); });
        CHECK(epoch_value(Json("18446744073709551615")) == UINT64_MAX);
        CHECK(epoch_value(Json(UINT64_MAX)) == UINT64_MAX);
        bad = request("peer"); bad["name"] = "Peer\nname"; rejects([&] { s.request("a", bad); });
        rejects([&] { s.request("a", request("../peer")); });
        rejects([&] { s.request("a", request("peer", 41)); });
        s.request("a", request("2")); s.request("a", request("3")); s.request("a", request("4"));
        rejects([&] { s.request("a", request("5")); });
        s.presented({"a","peer"}, true); CHECK(s.entries.at({"a","peer"}).draws == 1);
        CHECK(s.entries.at({"a","peer"}).drawn_revision == first["revision"]);
        s.request("a", {{"action","hide"},{"id","peer"}}); CHECK(!s.entries.at({"a","peer"}).enabled);
        auto next = s.request("a", request("peer")); CHECK(next["revision"] > first["revision"]);
        CHECK(s.entries.at({"a","peer"}).enabled && !s.entries.at({"a","peer"}).visible);
        CHECK(s.request("b", {{"action","status"}})["markers"].size() == 1);
        s.request("a", {{"action","retire"},{"id","peer"}}); CHECK(!s.entries.contains({"a","peer"}) && s.entries.contains({"b","peer"}));
        s.retire("a"); CHECK(s.entries.size() == 1);
        s.context(43, true); CHECK(s.entries.empty());
        s.request("a", request("peer", 43)); s.context(43, false); CHECK(s.entries.empty());
        int originals = 0, attached = 0, cleanups = 0;
        auto prepare = [&] { attached = 2; };
        auto cleanup = [&]() noexcept { attached = 0; ++cleanups; };
        transient_draw(prepare, [&] { CHECK(attached == 2); ++originals; }, cleanup);
        CHECK(originals == 1 && cleanups == 1 && attached == 0);
        rejects([&] { transient_draw(prepare, [&] { ++originals; throw std::runtime_error("original cause"); }, cleanup); });
        CHECK(originals == 2 && cleanups == 2 && attached == 0);
        rejects([&] { transient_draw([&] { attached = 1; throw std::runtime_error("prepare cause"); }, [&] { ++originals; }, cleanup); });
        CHECK(originals == 3 && cleanups == 3 && attached == 0);
        std::cout << "info: marker ownership, validation, epoch, receipts and transient cleanup passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "error: " << error.what() << '\n'; return 1; }
}
