// loadPointsFile / pointsFixtureName: der Weg von der Datei zu den Punkten, mit Dateinamen in Fehlern.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "whitehole/runtime/point_file.hpp"

using namespace whitehole;

namespace {

std::string write(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / ("whfx_points_" + name);
    std::ofstream out{path, std::ios::binary};
    out << content;
    return path.string();
}

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

}  // namespace

TEST_CASE("loadPointsFile liest CSV und JSON nach der Endung", "[runtime][import]") {
    const std::string csv = write("a.csv", "0,0\n1,0\n2,0\n");
    const auto fromCsv = runtime::loadPointsFile(csv);
    REQUIRE(fromCsv.ok());
    CHECK(fromCsv.value().size() == 3);

    const std::string json = write("b.json", "[[0,0,0],[0,1,0]]");
    const auto fromJson = runtime::loadPointsFile(json);
    REQUIRE(fromJson.ok());
    CHECK(fromJson.value().size() == 2);

    std::filesystem::remove(csv);
    std::filesystem::remove(json);
}

TEST_CASE("loadPointsFile: Fehlermeldungen nennen die Datei", "[runtime][import]") {
    const std::string bad = write("c.csv", "1,2\nx,y\n");
    const auto broken = runtime::loadPointsFile(bad);
    REQUIRE_FALSE(broken.ok());
    CHECK(contains(broken.error().message, "whfx_points_c.csv"));
    CHECK(contains(broken.error().message, "Zeile 2"));

    const std::string other = write("d.dxf", "0\nSECTION\n");
    CHECK(contains(runtime::loadPointsFile(other).error().message, "Unbekanntes Format"));

    CHECK_FALSE(runtime::loadPointsFile((std::filesystem::temp_directory_path() / "whfx_gibt_es_nicht.csv").string()).ok());
    std::filesystem::remove(bad);
    std::filesystem::remove(other);
}

TEST_CASE("Beispiel: examples/daten/bogen.csv lässt sich importieren", "[runtime][import]") {
    const auto file = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() / "examples" / "daten" / "bogen.csv";
    const auto points = runtime::loadPointsFile(file.string());
    REQUIRE(points.ok());
    CHECK(points.value().size() == 40);
    CHECK(points.value().front().x < 1.0F);   // links
    CHECK(points.value().back().x > 99.0F);   // rechts (Radius 50, Breite 100)
}

TEST_CASE("pointsFixtureName: Dateiname ohne Ordner und Endung", "[runtime][import]") {
    CHECK(runtime::pointsFixtureName("/a/b/Dach links.csv") == "Dach links");
    CHECK(runtime::pointsFixtureName("C:\\Daten\\wand.json") == "wand");
    CHECK(runtime::pointsFixtureName("ohne_endung") == "ohne_endung");
    CHECK(runtime::pointsFixtureName(".csv") == ".csv");
    CHECK(runtime::pointsFixtureName("") == "Punkte");
}
