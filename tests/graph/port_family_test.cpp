// Tests von port_family.hpp: Familien der Porttypen und die Passt-an-Port-Prüfung für die Palette.
#include <catch2/catch_test_macros.hpp>

#include "whitehole/graph/port_family.hpp"

using namespace whitehole::graph;

namespace {

NodeDescriptor node(std::vector<PortDescriptor> ports) {
    NodeDescriptor descriptor;
    descriptor.typeId = "t.node";
    descriptor.ports = std::move(ports);
    return descriptor;
}

PortDescriptor in(PortType type) { return PortDescriptor{"i", "In", type, PortDirection::Input, false}; }
PortDescriptor out(PortType type) { return PortDescriptor{"o", "Out", type, PortDirection::Output, false}; }

}  // namespace

TEST_CASE("Porttypen gehören zu fünf Familien") {
    CHECK(portFamily(PortType::Field) == PortFamily::Image);
    CHECK(portFamily(PortType::Image) == PortFamily::Image);
    for (const PortType t : {PortType::Scalar, PortType::Integer, PortType::Boolean, PortType::Trigger}) {
        CHECK(portFamily(t) == PortFamily::Value);
    }
    CHECK(portFamily(PortType::Color) == PortFamily::Color);
    CHECK(portFamily(PortType::Palette) == PortFamily::Color);
    CHECK(portFamily(PortType::AudioSpectrum) == PortFamily::Audio);
    CHECK(portFamily(PortType::AudioWave) == PortFamily::Audio);
    CHECK(portFamily(PortType::Text) == PortFamily::Other);
    CHECK(portFamily(PortType::PointCloud) == PortFamily::Other);
}

TEST_CASE("outputFamily nimmt den ersten Ausgang") {
    CHECK(outputFamily(node({in(PortType::Field), out(PortType::Field)})) == PortFamily::Image);
    CHECK(outputFamily(node({out(PortType::Scalar), out(PortType::Trigger)})) == PortFamily::Value);
    CHECK_FALSE(outputFamily(node({in(PortType::Field)})).has_value());
}

TEST_CASE("acceptsInputFrom und offersOutputFor folgen den Verbindungsregeln") {
    const NodeDescriptor effect = node({in(PortType::Field), out(PortType::Field)});
    const NodeDescriptor generator = node({out(PortType::Field)});
    const NodeDescriptor lfo = node({out(PortType::Scalar)});
    const NodeDescriptor modeNode = node({in(PortType::Integer), out(PortType::Field)});

    CHECK(acceptsInputFrom(effect, PortType::Field));
    CHECK(acceptsInputFrom(effect, PortType::Scalar));  // Skalar -> konstantes Feld
    CHECK_FALSE(acceptsInputFrom(effect, PortType::Text));
    CHECK_FALSE(acceptsInputFrom(generator, PortType::Field));  // kein Eingang

    CHECK(acceptsInputFrom(modeNode, PortType::Scalar));  // gerundet
    CHECK(acceptsInputFrom(modeNode, PortType::Integer));
    CHECK_FALSE(acceptsInputFrom(modeNode, PortType::Boolean));

    CHECK(offersOutputFor(lfo, PortType::Integer));  // Scalar -> Integer
    CHECK(offersOutputFor(lfo, PortType::Field));
    CHECK_FALSE(offersOutputFor(lfo, PortType::Color));
    CHECK(offersOutputFor(generator, PortType::Image));
    CHECK_FALSE(offersOutputFor(effect, PortType::Scalar));
}
