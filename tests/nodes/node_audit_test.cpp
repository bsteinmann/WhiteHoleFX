// node_audit_test.cpp: Regeln für alle eingebauten Nodes, damit die Palette und das Parameterfenster einheitlich bleiben.
//
// Warum es den Test gibt: Die Nodes wachsen (Palette, Suche, Presets, Looks verlassen sich auf Namen und Typ-IDs).
// Hier stehen die Absprachen als Prüfung, damit ein neuer Node nicht mit leerem Namen, doppelter Bezeichnung,
// unsinnigem Bereich oder einer Auswahl ohne Auswahl erscheint.
#include <catch2/catch_test_macros.hpp>

#include <cctype>
#include <cstdint>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include "whitehole/nodes/builtin_nodes.hpp"

using namespace whitehole;

namespace {

std::vector<graph::NodeDescriptor> allDescriptors() {
    std::vector<graph::NodeDescriptor> result;
    for (const compiler::NodeDefinition& definition : nodes::builtinNodeDefinitions()) {
        result.push_back(definition.descriptor);
    }
    for (const values::CpuNodeDefinition& definition : nodes::builtinCpuDefinitions()) {
        result.push_back(definition.descriptor);
    }
    return result;
}

// Schlüssel sind lowerCamelCase: beginnen klein, danach Buchstaben und Ziffern.
bool isIdentifier(const std::string& text) {
    if (text.empty() || std::islower(static_cast<unsigned char>(text.front())) == 0) {
        return false;
    }
    for (const char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0) {
            return false;
        }
    }
    return true;
}

}  // namespace

TEST_CASE("Node-Regeln: Typ-ID, Name und Kategorie", "[nodes][audit]") {
    std::set<std::string> ids;
    std::set<std::string> names;
    for (const graph::NodeDescriptor& node : allDescriptors()) {
        INFO(node.typeId);
        CHECK(node.typeId.rfind("wh.", 0) == 0);
        CHECK(ids.insert(node.typeId).second);
        CHECK_FALSE(node.displayName.empty());
        CHECK_FALSE(node.category.empty());
        // gleiche Namen in der Palette verwirren
        INFO("doppelter Name: " << node.displayName);
        CHECK(names.insert(node.displayName).second);
        CHECK(node.displayName == node.displayName.substr(node.displayName.find_first_not_of(' ')));  // keine führenden Leerzeichen
    }
}

TEST_CASE("Node-Regeln: Parameter haben Schlüssel, Namen und sinnvolle Bereiche", "[nodes][audit]") {
    for (const graph::NodeDescriptor& node : allDescriptors()) {
        std::set<std::string> keys;
        std::set<std::string> shown;
        for (const graph::ParameterDescriptor& parameter : node.parameters) {
            INFO(node.typeId << "." << parameter.key);
            CHECK(isIdentifier(parameter.key) == true);
            CHECK_FALSE(parameter.key.empty());
            CHECK(keys.insert(parameter.key).second);
            CHECK_FALSE(parameter.displayName.empty());
            INFO("doppelte Beschriftung: " << parameter.displayName);
            CHECK(shown.insert(parameter.displayName).second);
            if (parameter.minValue && parameter.maxValue) {
                CHECK(*parameter.minValue < *parameter.maxValue);
            }
            if (!parameter.choices.empty()) {
                const auto* index = std::get_if<std::int64_t>(&parameter.defaultValue);
                REQUIRE(index != nullptr);
                CHECK(parameter.choices.size() >= 2);
                CHECK(*index >= 0);
                CHECK(*index < static_cast<std::int64_t>(parameter.choices.size()));
                std::set<std::string> options(parameter.choices.begin(), parameter.choices.end());
                CHECK(options.size() == parameter.choices.size());
            }
        }
    }
}

TEST_CASE("Node-Regeln: Ports haben eindeutige Schlüssel und Namen", "[nodes][audit]") {
    for (const graph::NodeDescriptor& node : allDescriptors()) {
        std::set<std::string> keys;
        std::size_t outputs = 0;
        for (const graph::PortDescriptor& port : node.ports) {
            INFO(node.typeId << "." << port.key);
            CHECK(keys.insert(port.key).second);
            CHECK_FALSE(port.displayName.empty());
            if (port.direction == graph::PortDirection::Output) {
                ++outputs;
            }
        }
        INFO(node.typeId);
        CHECK(outputs >= 1);
    }
}
