// node_catalog_test.cpp: Text für --list-nodes und --describe-node.
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "cli_options.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/node_catalog.hpp"

using namespace whitehole;

namespace {
bool contains(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }
}  // namespace

TEST_CASE("Node-Verzeichnis: --list-nodes nennt jeden eingebauten Typ", "[cli][catalog]") {
    auto library = nodes::BuiltinLibrary::create().value();
    const std::string list = runtime::listNodes(*library->registry);
    CHECK(contains(list, "wh.generator.gradient"));
    CHECK(contains(list, "wh.transform.twist"));
    CHECK(contains(list, "wh.value.lfo"));
    std::size_t lines = 0;
    for (const char c : list) {
        lines += c == '\n' ? 1U : 0U;
    }
    CHECK(lines == library->registry->all().size());
}

TEST_CASE("Node-Verzeichnis: --describe-node zeigt Ports, Parameter, Bereiche und Auswahlfelder", "[cli][catalog]") {
    auto library = nodes::BuiltinLibrary::create().value();
    const auto text = runtime::describeNode(*library->registry, "wh.generator.ripples");
    REQUIRE(text.ok());
    INFO(text.value());
    CHECK(contains(text.value(), "Ausgang out"));
    CHECK(contains(text.value(), "rate  "));
    CHECK(contains(text.value(), "Auswahl: 0=Flat"));
    CHECK(contains(text.value(), "1=Volume"));
    CHECK(contains(text.value(), "Farbe (r g b a)"));
    CHECK(contains(text.value(), "(steuert den gleichnamigen Parameter)"));

    const auto twist = runtime::describeNode(*library->registry, "wh.transform.twist");
    REQUIRE(twist.ok());
    CHECK(contains(twist.value(), "Eingang source"));
    CHECK(contains(twist.value(), "(Pflicht)"));
    CHECK(contains(twist.value(), "Bereich -1440 bis 1440"));
}

TEST_CASE("Node-Verzeichnis: unbekannte Typen schlagen Ähnliches vor", "[cli][catalog]") {
    auto library = nodes::BuiltinLibrary::create().value();
    const auto missing = runtime::describeNode(*library->registry, "gradient");
    REQUIRE_FALSE(missing.ok());
    CHECK(missing.error().code == ErrorCode::NotFound);
    CHECK(contains(missing.error().message, "wh.generator.gradient"));
    CHECK_FALSE(runtime::describeNode(*library->registry, "").ok());
}

TEST_CASE("Kommandozeile: --list-nodes und --describe-node brauchen kein Projekt", "[cli][catalog]") {
    const auto list = cli::parseOptions({"--list-nodes"});
    REQUIRE(list.ok());
    CHECK(list.value().listNodes);
    const auto describe = cli::parseOptions({"--describe-node", "wh.value.lfo"});
    REQUIRE(describe.ok());
    CHECK(describe.value().describeNode == "wh.value.lfo");
    CHECK_FALSE(cli::parseOptions({"--describe-node"}).ok());
}

TEST_CASE("Node-Verzeichnis: jeder Typ lässt sich beschreiben", "[cli][catalog]") {
    auto library = nodes::BuiltinLibrary::create().value();
    for (const graph::NodeDescriptor* type : library->registry->all()) {
        INFO(type->typeId);
        CHECK(runtime::describeNode(*library->registry, type->typeId).ok());
    }
}
