#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "test_cpu_nodes.hpp"
#include "whitehole/values/cpu_kernel.hpp"

using namespace whitehole;
using namespace whitehole::values;
using whitehole::test::outPort;

namespace {

class NullKernel final : public ICpuKernel {
public:
    void evaluate(const KernelContext&, KernelOutputs&) override {}
};

KernelFactory nullFactory() {
    return [] { return std::make_unique<NullKernel>(); };
}

CpuNodeDefinition valid(const std::string& typeId) {
    CpuNodeDefinition definition;
    definition.descriptor.typeId = typeId;
    definition.descriptor.domain = graph::NodeDomain::Cpu;
    definition.descriptor.ports = {outPort("out", graph::PortType::Scalar)};
    definition.factory = nullFactory();
    return definition;
}

}  // namespace

TEST_CASE("KernelInputs: Zugriffe liefern den Wert oder den Rückfallwert") {
    KernelInputs inputs;
    inputs.set("real", 2.5);
    inputs.set("int", std::int64_t{4});
    inputs.set("flag", true);
    inputs.set("color", ColorRgba{0.1F, 0.2F, 0.3F, 0.4F});
    inputs.set("text", std::string{"x"});

    CHECK(inputs.has("real"));
    CHECK_FALSE(inputs.has("missing"));

    CHECK(inputs.number("real") == 2.5);
    CHECK(inputs.number("int") == 4.0);       // int64 als Zahl
    CHECK(inputs.number("flag") == 1.0);      // bool als Zahl
    CHECK(inputs.number("color", -1.0) == -1.0);
    CHECK(inputs.number("missing", 9.0) == 9.0);

    CHECK(inputs.integer("int") == 4);
    CHECK(inputs.integer("real", 7) == 7);    // double ist keine Ganzzahl
    CHECK(inputs.boolean("flag"));
    CHECK_FALSE(inputs.boolean("real"));
    CHECK(inputs.boolean("real", true));
    CHECK(inputs.color("color").g == 0.2F);
    CHECK(inputs.color("real", ColorRgba{9.0F, 0.0F, 0.0F, 1.0F}).r == 9.0F);
}

TEST_CASE("KernelInputs: erneutes Setzen ersetzt den Wert") {
    KernelInputs inputs;
    inputs.set("a", 1.0);
    inputs.set("a", 2.0);
    CHECK(inputs.number("a") == 2.0);
}

TEST_CASE("KernelOutputs: Setzen ersetzt und sammelt") {
    KernelOutputs outputs;
    outputs.set("a", 1.0);
    outputs.set("b", true);
    outputs.set("a", 5.0);

    REQUIRE(outputs.values().size() == 2U);
    CHECK(std::get<double>(outputs.values().at("a")) == 5.0);
    CHECK(std::get<bool>(outputs.values().at("b")));
}

TEST_CASE("CpuKernelLibrary: Anlegen, Finden, Erzeugen") {
    CpuKernelLibrary library;
    CHECK_FALSE(library.contains("k"));
    CHECK(library.create("k") == nullptr);

    REQUIRE(library.add("k", nullFactory()).ok());
    CHECK(library.contains("k"));
    CHECK(library.create("k") != nullptr);

    SECTION("doppelte Typ-ID") {
        const Result<void> again = library.add("k", nullFactory());
        REQUIRE_FALSE(again.ok());
        CHECK(again.error().code == ErrorCode::AlreadyExists);
    }
    SECTION("leere Fabrik") {
        const Result<void> empty = library.add("other", KernelFactory{});
        REQUIRE_FALSE(empty.ok());
        CHECK(empty.error().code == ErrorCode::InvalidArgument);
        CHECK_FALSE(library.contains("other"));
    }
}

TEST_CASE("CpuKernelLibrary: jeder Aufruf von create liefert eine eigene Instanz") {
    CpuKernelLibrary library;
    REQUIRE(library.add("k", nullFactory()).ok());
    const auto first = library.create("k");
    const auto second = library.create("k");
    CHECK(first.get() != second.get());
}

TEST_CASE("registerCpuDefinitions: gültige Definitionen landen in Registry und Bibliothek") {
    graph::NodeRegistry registry;
    CpuKernelLibrary library;
    const std::vector<CpuNodeDefinition> definitions{valid("a.one"), valid("a.two")};

    REQUIRE(registerCpuDefinitions(definitions, registry, library).ok());
    CHECK(registry.find("a.one") != nullptr);
    CHECK(registry.find("a.two") != nullptr);
    CHECK(library.contains("a.one"));
    CHECK(library.contains("a.two"));
}

TEST_CASE("registerCpuDefinitions: ungültige Definitionen werden gar nicht erst eingetragen") {
    graph::NodeRegistry registry;
    CpuKernelLibrary library;

    SECTION("falsche Domäne") {
        CpuNodeDefinition definition = valid("bad.domain");
        definition.descriptor.domain = graph::NodeDomain::Field;
        const std::vector<CpuNodeDefinition> definitions{definition};
        const Result<void> result = registerCpuDefinitions(definitions, registry, library);
        REQUIRE_FALSE(result.ok());
        CHECK(result.error().code == ErrorCode::InvalidArgument);
        CHECK(registry.find("bad.domain") == nullptr);
    }
    SECTION("fehlende Fabrik") {
        CpuNodeDefinition definition = valid("bad.factory");
        definition.factory = nullptr;
        const std::vector<CpuNodeDefinition> definitions{definition};
        const Result<void> result = registerCpuDefinitions(definitions, registry, library);
        REQUIRE_FALSE(result.ok());
        CHECK(result.error().code == ErrorCode::InvalidArgument);
        CHECK(registry.find("bad.factory") == nullptr);
    }
    SECTION("Ausgang, der keinen Einzelwert liefert") {
        for (const graph::PortType type : {graph::PortType::Image, graph::PortType::Field,
                                           graph::PortType::AudioSpectrum, graph::PortType::PointCloud,
                                           graph::PortType::Palette}) {
            CpuNodeDefinition definition = valid("bad.output");
            definition.descriptor.ports = {outPort("out", type)};
            const std::vector<CpuNodeDefinition> definitions{definition};
            const Result<void> result = registerCpuDefinitions(definitions, registry, library);
            REQUIRE_FALSE(result.ok());
            CHECK(result.error().code == ErrorCode::InvalidArgument);
            CHECK(registry.find("bad.output") == nullptr);
        }
    }
    SECTION("alle Einzelwert-Typen sind als Ausgang erlaubt") {
        int index = 0;
        for (const graph::PortType type :
             {graph::PortType::Scalar, graph::PortType::Integer, graph::PortType::Boolean,
              graph::PortType::Trigger, graph::PortType::Color, graph::PortType::Text}) {
            CpuNodeDefinition definition = valid("ok." + std::to_string(index++));
            definition.descriptor.ports = {outPort("out", type)};
            const std::vector<CpuNodeDefinition> definitions{definition};
            CHECK(registerCpuDefinitions(definitions, registry, library).ok());
        }
    }
}

TEST_CASE("registerCpuDefinitions: Doppelte und Registry-Fehler") {
    graph::NodeRegistry registry;
    CpuKernelLibrary library;
    const std::vector<CpuNodeDefinition> first{valid("dup")};
    REQUIRE(registerCpuDefinitions(first, registry, library).ok());

    SECTION("zweites Eintragen") {
        const Result<void> again = registerCpuDefinitions(first, registry, library);
        REQUIRE_FALSE(again.ok());
        CHECK(again.error().code == ErrorCode::AlreadyExists);
    }
    SECTION("Registry lehnt ab: die Bibliothek bleibt unberührt") {
        CpuNodeDefinition broken = valid("broken");
        broken.descriptor.ports = {outPort("kein gültiger Schlüssel", graph::PortType::Scalar)};
        const std::vector<CpuNodeDefinition> definitions{broken};
        const Result<void> result = registerCpuDefinitions(definitions, registry, library);
        REQUIRE_FALSE(result.ok());
        CHECK_FALSE(library.contains("broken"));
    }
    SECTION("bricht beim ersten Fehler ab, Früheres bleibt bestehen") {
        CpuNodeDefinition broken = valid("later");
        broken.factory = nullptr;
        const std::vector<CpuNodeDefinition> definitions{valid("before"), broken, valid("after")};
        REQUIRE_FALSE(registerCpuDefinitions(definitions, registry, library).ok());
        CHECK(library.contains("before"));
        CHECK_FALSE(library.contains("later"));
        CHECK_FALSE(library.contains("after"));
    }
}
