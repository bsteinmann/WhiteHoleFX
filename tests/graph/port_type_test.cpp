// Tests der Porttypen: welche Ausgänge mit welchen Eingängen verbunden werden dürfen.
#include <catch2/catch_test_macros.hpp>

#include "whitehole/graph/port_type.hpp"

using whitehole::graph::isConnectable;
using whitehole::graph::PortType;

// Die Regeln sind constexpr und damit schon zur Compilezeit prüfbar.
static_assert(isConnectable(PortType::Color, PortType::Color));
static_assert(isConnectable(PortType::Field, PortType::Image));
static_assert(!isConnectable(PortType::Text, PortType::Image));

TEST_CASE("Gleiche Porttypen sind immer verbindbar") {
    CHECK(isConnectable(PortType::Image, PortType::Image));
    CHECK(isConnectable(PortType::Trigger, PortType::Trigger));
    CHECK(isConnectable(PortType::PointCloud, PortType::PointCloud));
}

TEST_CASE("Spektrum und Wellenform tragen beide 32 Werte, lassen sich aber nicht vertauschen") {
    CHECK(whitehole::graph::carriesBands(PortType::AudioSpectrum));
    CHECK(whitehole::graph::carriesBands(PortType::AudioWave));
    CHECK_FALSE(whitehole::graph::carriesBands(PortType::Scalar));
    CHECK(isConnectable(PortType::AudioWave, PortType::AudioWave));
    CHECK_FALSE(isConnectable(PortType::AudioWave, PortType::AudioSpectrum));
    CHECK_FALSE(isConnectable(PortType::AudioSpectrum, PortType::AudioWave));
}

TEST_CASE("Field und Image lassen sich ineinander überführen") {
    CHECK(isConnectable(PortType::Field, PortType::Image));
    CHECK(isConnectable(PortType::Image, PortType::Field));
    CHECK(isConnectable(PortType::Scalar, PortType::Field));
    CHECK(isConnectable(PortType::Color, PortType::Field));
}

TEST_CASE("Einfache Konvertierungen sind erlaubt, unverträgliche Typen nicht") {
    CHECK(isConnectable(PortType::Integer, PortType::Scalar));
    CHECK(isConnectable(PortType::Boolean, PortType::Scalar));
    CHECK(isConnectable(PortType::Trigger, PortType::Boolean));

    CHECK_FALSE(isConnectable(PortType::Scalar, PortType::Integer));
    CHECK_FALSE(isConnectable(PortType::Text, PortType::Color));
    CHECK_FALSE(isConnectable(PortType::Image, PortType::Scalar));
    CHECK_FALSE(isConnectable(PortType::Color, PortType::Scalar));
}
