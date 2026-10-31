// Tests des ValueStores: Werte je Node und Ausgang setzen, lesen und ersetzen.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>

#include "whitehole/values/value_store.hpp"

using namespace whitehole;
using whitehole::values::ValueStore;

TEST_CASE("ValueStore: leerer Speicher liefert nichts") {
    const ValueStore store;
    CHECK(store.size() == 0U);
    CHECK_FALSE(store.value(NodeId{1}, "out").has_value());
}

TEST_CASE("ValueStore: setzen und lesen, je Node und Ausgang getrennt") {
    ValueStore store;
    store.set(NodeId{1}, "out", 1.5);
    store.set(NodeId{1}, "other", std::int64_t{7});
    store.set(NodeId{2}, "out", true);
    store.set(NodeId{3}, "out", ColorRgba{0.1F, 0.2F, 0.3F, 0.4F});
    store.set(NodeId{4}, "out", std::string{"hallo"});

    CHECK(store.size() == 5U);
    CHECK(std::get<double>(*store.value(NodeId{1}, "out")) == 1.5);
    CHECK(std::get<std::int64_t>(*store.value(NodeId{1}, "other")) == 7);
    CHECK(std::get<bool>(*store.value(NodeId{2}, "out")));
    CHECK(std::get<ColorRgba>(*store.value(NodeId{3}, "out")).b == 0.3F);
    CHECK(std::get<std::string>(*store.value(NodeId{4}, "out")) == "hallo");

    CHECK_FALSE(store.value(NodeId{1}, "missing").has_value());
    CHECK_FALSE(store.value(NodeId{9}, "out").has_value());
}

TEST_CASE("ValueStore: Setzen ersetzt einen vorhandenen Wert und zählt ihn nicht doppelt") {
    ValueStore store;
    store.set(NodeId{1}, "out", 1.0);
    store.set(NodeId{1}, "out", 2.0);
    store.set(NodeId{1}, "out", std::string{"anderer Typ"});

    CHECK(store.size() == 1U);
    CHECK(std::get<std::string>(*store.value(NodeId{1}, "out")) == "anderer Typ");
}

TEST_CASE("ValueStore: clear entfernt alles") {
    ValueStore store;
    store.set(NodeId{1}, "out", 1.0);
    store.set(NodeId{2}, "out", 2.0);
    store.clear();

    CHECK(store.size() == 0U);
    CHECK_FALSE(store.value(NodeId{1}, "out").has_value());

    store.set(NodeId{1}, "out", 3.0);  // danach wieder benutzbar
    CHECK(store.size() == 1U);
}

TEST_CASE("ValueStore: Lookup über die Schnittstelle") {
    ValueStore store;
    store.set(NodeId{5}, "out", 0.25);

    const graph::IValueLookup& lookup = store;
    CHECK(std::get<double>(*lookup.value(NodeId{5}, "out")) == 0.25);
}
