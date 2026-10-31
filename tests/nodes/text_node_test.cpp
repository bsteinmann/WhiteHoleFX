// text_node_test.cpp: der Text-Node kompiliert: Der Text steht im Shader (@text, @textlen), ein neuer Text
// ändert die Struktur, ein neues Tempo nicht, und Fehler (kein Textparameter) werden gemeldet.
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "builtin_lab.hpp"

using namespace whitehole;
using whitehole::test::Lab;

namespace {

compiler::CompiledField compileText(Lab& lab, NodeId node) {
    const auto compiled = compiler::compileField(lab.graph, lab.snippets, graph::PortRef{node, "out"});
    REQUIRE(compiled.ok());
    return compiled.value();
}

}  // namespace

TEST_CASE("Text-Node: der Text steht als Codepunkte im Shader", "[nodes][text]") {
    Lab lab;
    const NodeId text = lab.add("wh.generator.text");
    lab.set(text, "text", std::string{"Hi \xC3\xA4"});  // "Hi ä"
    const compiler::CompiledField compiled = compileText(lab, text);
    CHECK(compiled.wgsl.find("array<u32, 4>(72u, 105u, 32u, 228u)") != std::string::npos);
    CHECK(compiled.wgsl.find("i32(4u)") != std::string::npos);
    CHECK(compiled.wgsl.find("@text") == std::string::npos);  // Platzhalter vollständig ersetzt
}

TEST_CASE("Text-Node: leerer Text bekommt ein Feld der Länge 1 und die Länge 0", "[nodes][text]") {
    Lab lab;
    const NodeId text = lab.add("wh.generator.text");
    lab.set(text, "text", std::string{});
    const compiler::CompiledField compiled = compileText(lab, text);
    CHECK(compiled.wgsl.find("array<u32, 1>(0u)") != std::string::npos);
    CHECK(compiled.wgsl.find("i32(0u)") != std::string::npos);
}

TEST_CASE("Text-Node: zu lange Texte werden auf 256 Zeichen gekürzt", "[nodes][text]") {
    Lab lab;
    const NodeId text = lab.add("wh.generator.text");
    lab.set(text, "text", std::string(1000, 'x'));
    const compiler::CompiledField compiled = compileText(lab, text);
    CHECK(compiled.wgsl.find("array<u32, 256>(") != std::string::npos);
    CHECK(compiled.wgsl.find("i32(256u)") != std::string::npos);
}

TEST_CASE("Text-Node: neuer Text ändert die Struktur, neues Tempo nur den Puffer", "[nodes][text]") {
    Lab lab;
    const NodeId text = lab.add("wh.generator.text");
    lab.set(text, "text", std::string{"eins"});
    const std::uint64_t first = compileText(lab, text).structureHash;
    lab.set(text, "speed", 40.0);
    lab.set(text, "color", ColorRgba{1.0F, 0.0F, 0.0F, 1.0F});
    CHECK(compileText(lab, text).structureHash == first);
    lab.set(text, "text", std::string{"zwei"});
    CHECK(compileText(lab, text).structureHash != first);
}

TEST_CASE("Text-Node: zwei Text-Nodes teilen sich Schrift und Hilfsfunktionen", "[nodes][text]") {
    Lab lab;
    const NodeId a = lab.add("wh.generator.text");
    const NodeId b = lab.add("wh.generator.text");
    lab.set(a, "text", std::string{"A"});
    lab.set(b, "text", std::string{"B"});
    const NodeId mix = lab.add("wh.blend.add");
    lab.connect(a, "out", mix, "a");
    lab.connect(b, "out", mix, "b");
    const auto compiled = compiler::compileField(lab.graph, lab.snippets, graph::PortRef{mix, "out"});
    REQUIRE(compiled.ok());
    const std::string& wgsl = compiled.value().wgsl;
    const auto first = wgsl.find("fn wh_text_glyph");
    REQUIRE(first != std::string::npos);
    CHECK(wgsl.find("fn wh_text_glyph", first + 1) == std::string::npos);  // nur einmal definiert
}
