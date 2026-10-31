// Tests des Punkte-Imports: CSV (Trenner, Kopfzeile, Dezimalkomma, Fehler) und JSON (Listen, Objekte,
// Fehler) sowie Spiegeln, Maßstab und Verschieben.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "whitehole/mapping/point_import.hpp"

using namespace whitehole;
using namespace whitehole::mapping;
using Catch::Approx;

namespace {

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

ImportOptions raw() {
    ImportOptions options;
    options.toOrigin = false;
    return options;
}

}  // namespace

TEST_CASE("CSV: zwei oder drei Werte, Kabelreihenfolge bleibt", "[mapping][import]") {
    const auto points = parsePointsCsv("1,2\n3,4\n5,6\n", raw());
    REQUIRE(points.ok());
    REQUIRE(points.value().size() == 3);
    CHECK(points.value()[0].x == Approx(1.0F));
    CHECK(points.value()[2].y == Approx(6.0F));
    CHECK(points.value()[1].z == Approx(0.0F));

    const auto volume = parsePointsCsv("1,2,3\n4,5,6\n", raw());
    REQUIRE(volume.ok());
    CHECK(volume.value()[1].z == Approx(6.0F));
}

TEST_CASE("CSV: Trenner Semikolon mit Dezimalkomma, Tabulator und Leerraum", "[mapping][import]") {
    const auto semicolon = parsePointsCsv("1,5;2,25\n3;4\n", raw());
    REQUIRE(semicolon.ok());
    CHECK(semicolon.value()[0].x == Approx(1.5F));
    CHECK(semicolon.value()[0].y == Approx(2.25F));

    const auto tab = parsePointsCsv("1,5\t2\n", raw());
    REQUIRE(tab.ok());
    CHECK(tab.value()[0].x == Approx(1.5F));

    const auto spaces = parsePointsCsv("1.5   2.5  3\n  4 5 6  \n", raw());
    REQUIRE(spaces.ok());
    CHECK(spaces.value().size() == 2);
    CHECK(spaces.value()[0].z == Approx(3.0F));
}

TEST_CASE("CSV: Kommentare, leere Zeilen, Byte-Order-Mark und Windows-Zeilenenden", "[mapping][import]") {
    const auto points = parsePointsCsv("\xEF\xBB\xBF# Lage\r\n\r\n1,2\r\n\r\n3,4\r\n", raw());
    REQUIRE(points.ok());
    CHECK(points.value().size() == 2);
}

TEST_CASE("CSV: Kopfzeile ordnet Spalten und ignoriert weitere", "[mapping][import]") {
    const auto points = parsePointsCsv("index,Z,Y,X,name\n0,30,20,10,a\n1,60,50,40,b\n", raw());
    REQUIRE(points.ok());
    REQUIRE(points.value().size() == 2);
    CHECK(points.value()[0].x == Approx(10.0F));
    CHECK(points.value()[0].y == Approx(20.0F));
    CHECK(points.value()[0].z == Approx(30.0F));
    CHECK(points.value()[1].x == Approx(40.0F));

    const auto noZ = parsePointsCsv("x;y\n1;2\n", raw());
    REQUIRE(noZ.ok());
    CHECK(noZ.value()[0].z == Approx(0.0F));
}

TEST_CASE("CSV: Fehler nennen Zeile und Grund", "[mapping][import]") {
    CHECK(contains(parsePointsCsv("").error().message, "keine Punkte"));
    CHECK(contains(parsePointsCsv("# nur Kommentar\n").error().message, "keine Punkte"));
    CHECK(contains(parsePointsCsv("1,2\n3,abc\n").error().message, "Zeile 2"));
    CHECK(contains(parsePointsCsv("1,2\n3,abc\n").error().message, "y ist keine Zahl"));
    CHECK(contains(parsePointsCsv("1,2\n3\n").error().message, "Zeile 2"));
    CHECK(contains(parsePointsCsv("1,2,3,4\n").error().message, "Kopfzeile"));
    CHECK(contains(parsePointsCsv("name,wert\n1,2\n").error().message, "x und y"));
    CHECK(contains(parsePointsCsv("1,2\n1e999,3\n").error().message, "keine Zahl"));
    CHECK(contains(parsePointsCsv("1,2\n3,nan\n").error().message, "keine Zahl"));
}

TEST_CASE("Import: Maßstab, y spiegeln und auf den Ursprung schieben", "[mapping][import]") {
    ImportOptions options;
    options.scale = 0.1F;
    options.flipY = true;
    options.toOrigin = true;
    const auto points = parsePointsCsv("100,100\n300,200\n", options);
    REQUIRE(points.ok());
    // nach Maßstab (10,10) und (30,20), y gespiegelt (10,-10) und (30,-20), dann Minimum auf 0
    CHECK(points.value()[0].x == Approx(0.0F));
    CHECK(points.value()[0].y == Approx(10.0F));
    CHECK(points.value()[1].x == Approx(20.0F));
    CHECK(points.value()[1].y == Approx(0.0F));

    ImportOptions broken;
    broken.scale = 0.0F;
    CHECK(contains(parsePointsCsv("1,2\n", broken).error().message, "Maßstab"));
}

TEST_CASE("JSON: Listen, Objekte und Wrapper-Felder", "[mapping][import]") {
    const auto lists = parsePointsJson("[[1,2],[3,4,5]]", raw());
    REQUIRE(lists.ok());
    CHECK(lists.value().size() == 2);
    CHECK(lists.value()[1].z == Approx(5.0F));

    const auto objects = parsePointsJson(R"([{"x":1.5,"y":-2},{"x":3,"y":4,"z":7}])", raw());
    REQUIRE(objects.ok());
    CHECK(objects.value()[0].x == Approx(1.5F));
    CHECK(objects.value()[0].y == Approx(-2.0F));
    CHECK(objects.value()[1].z == Approx(7.0F));

    for (const char* key : {"points", "leds", "pixels", "positions"}) {
        const std::string text = std::string{R"({"name":"Wand","count":2,")"} + key + R"(":[[0,0],[1,1]]})";
        const auto wrapped = parsePointsJson(text, raw());
        REQUIRE(wrapped.ok());
        CHECK(wrapped.value().size() == 2);
    }
    const auto bom = parsePointsJson("\xEF\xBB\xBF [ [1, 2] ]\n", raw());
    CHECK(bom.ok());
}

TEST_CASE("JSON: Fehler sind lesbar", "[mapping][import]") {
    CHECK(contains(parsePointsJson("").error().message, "JSON"));
    CHECK(contains(parsePointsJson("[[1,2]").error().message, "erwartet"));
    CHECK(contains(parsePointsJson("[[1,2]] x").error().message, "Nach dem JSON-Wert"));
    CHECK(contains(parsePointsJson(R"({"a":1})").error().message, "Feld"));
    CHECK(contains(parsePointsJson("5").error().message, "Liste"));
    CHECK(contains(parsePointsJson("[[1,2],[3]]").error().message, "Eintrag 2"));
    CHECK(contains(parsePointsJson(R"([{"x":1}])").error().message, "Eintrag 1"));
    CHECK(contains(parsePointsJson(R"([["a","b"]])").error().message, "Eintrag 1"));
    CHECK(contains(parsePointsJson("[]").error().message, "keine Punkte"));
    CHECK(contains(parsePointsJson("[1e999]").error().message, "Zahl"));
    // zu tief verschachtelt, ohne den Stapel zu sprengen
    CHECK(contains(parsePointsJson(std::string(5000, '[')).error().message, "tief"));
}

TEST_CASE("parsePoints wählt das Format nach der Endung", "[mapping][import]") {
    CHECK(parsePoints("1,2", ".CSV", raw()).ok());
    CHECK(parsePoints("1 2", ".txt", raw()).ok());
    CHECK(parsePoints("[[1,2]]", ".json", raw()).ok());
    CHECK(contains(parsePoints("1,2", ".dxf", raw()).error().message, "Unbekanntes Format"));
}
