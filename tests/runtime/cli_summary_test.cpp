// Tests des Prüfberichts, der Kommandozeile und der mitgelieferten Beispieldatei.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "cli_options.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/session.hpp"
#include "whitehole/runtime/summary.hpp"

using namespace whitehole;

namespace {

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

cli::Options options(std::vector<std::string> args) {
    auto r = cli::parseOptions(args);
    REQUIRE(r.ok());
    return r.value();
}

std::string optionError(std::vector<std::string> args) {
    auto r = cli::parseOptions(args);
    REQUIRE_FALSE(r.ok());
    return r.error().message;
}

}  // namespace

TEST_CASE("Kommandozeile: gültige Aufrufe", "[cli]") {
    const cli::Options a = options({"p.whfx"});
    CHECK(a.projectPath == "p.whfx");
    CHECK_FALSE(a.check);
    CHECK_FALSE(a.frames.has_value());

    const cli::Options b = options({"--check", "p.whfx", "--frames", "120", "--fps", "25.5", "--software"});
    CHECK(b.check);
    CHECK(b.software);
    CHECK(b.frames == 120U);
    CHECK(b.fps == 25.5);

    CHECK(options({"--help"}).help);

    const cli::Options c = options({"p.whfx", "--snapshot", "bild.png", "--size", "1280x720"});
    CHECK(c.snapshotPath == "bild.png");
    CHECK(c.snapshotWidth == 1280);
    CHECK(c.snapshotHeight == 720);
    CHECK(options({"p.whfx", "--snapshot", "bild.png"}).snapshotWidth == 960);
}

TEST_CASE("Kommandozeile: Fehler nennen die Ursache", "[cli]") {
    CHECK(contains(optionError({}), "Projektdatei"));
    CHECK(contains(optionError({"a", "b"}), "Zweite Projektdatei"));
    CHECK(contains(optionError({"a", "--bunt"}), "Unbekannte Option --bunt"));
    CHECK(contains(optionError({"a", "--frames"}), "--frames braucht"));
    CHECK(contains(optionError({"a", "--frames", "0"}), "ab 1"));
    CHECK(contains(optionError({"a", "--frames", "5x"}), "ab 1"));
    CHECK(contains(optionError({"a", "--fps", "0"}), "größer 0"));
    CHECK(contains(optionError({"a", "--fps", "nan"}), "größer 0"));
    CHECK(contains(optionError({"a", "--fps", "5000"}), "höchstens 1000"));
    CHECK(contains(optionError({"a", "--snapshot"}), "Dateinamen"));
    CHECK(contains(optionError({"a", "--snapshot", "x.png", "--size"}), "Breite und Höhe"));
    CHECK(contains(optionError({"a", "--snapshot", "x.png", "--size", "100"}), "Breite x Höhe"));
    CHECK(contains(optionError({"a", "--snapshot", "x.png", "--size", "10x500"}), "16 bis 8192"));
    CHECK(contains(optionError({"a", "--snapshot", "x.png", "--size", "9000x500"}), "16 bis 8192"));
    CHECK(contains(optionError({"a", "--snapshot", "x.png", "--size", "axb"}), "16 bis 8192"));
    CHECK(contains(optionError({"a", "--size", "640x480"}), "gehört zu --snapshot"));
}

TEST_CASE("Beispiel: examples/wand.whfx ist gültig und der Bericht stimmt", "[cli]") {
    auto library = nodes::BuiltinLibrary::create().value();
    const auto text = project::readTextFile((std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() / "examples" / "wand.whfx").string());
    REQUIRE(text.ok());
    const auto parsed = project::parseProject(text.value(), library->registry);
    REQUIRE(parsed.ok());
    const auto report = runtime::summarize(parsed.value());
    REQUIRE(report.ok());
    INFO(report.value());
    CHECK(contains(report.value(), "Fixtures: 2, LEDs: 188"));
    CHECK(contains(report.value(), "2 Pakete je Frame"));
    CHECK(contains(report.value(), "Art-Net -> 127.0.0.1:6454, Universe 0, 384 Bytes"));
    CHECK(contains(report.value(), "Universe 1, 180 Bytes"));
    CHECK(contains(report.value(), "gesetzt"));
}

TEST_CASE("Beispiele: alle Dateien in examples/ lassen sich lesen und prüfen", "[cli]") {
    auto library = nodes::BuiltinLibrary::create().value();
    const auto dir = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() / "examples";
    std::size_t found = 0;
    for (const auto& entry : std::filesystem::directory_iterator{dir}) {
        if (entry.path().extension() != ".whfx") {
            continue;
        }
        ++found;
        INFO(entry.path().string());
        const auto text = project::readTextFile(entry.path().string());
        REQUIRE(text.ok());
        const auto parsed = project::parseProject(text.value(), library->registry);
        REQUIRE(parsed.ok());
        CHECK(parsed.value().target.has_value());
        CHECK(runtime::summarize(parsed.value()).ok());
    }
    CHECK(found >= 2);
}

TEST_CASE("Bericht: Multicast, eigener Port und DDP werden benannt", "[cli]") {
    auto library = nodes::BuiltinLibrary::create().value();
    const auto parsed = project::parseProject(
        "whitehole-project 1\n"
        "destination 0 multicast\ndestination 1 10.0.0.9 7000\ndestination 2 10.0.0.10\n"
        "fixture A\n  strip 200 from 0 0 0 to 1 0 0\n  patch sacn destination 0 universe 5\nend\n"
        "fixture B\n  matrix 2 2\n  patch artnet destination 1 universe 2\nend\n"
        "fixture C\n  matrix 2 2\n  patch ddp destination 2 offset 36\nend\n",
        library->registry);
    REQUIRE(parsed.ok());
    const auto report = runtime::summarize(parsed.value());
    REQUIRE(report.ok());
    INFO(report.value());
    CHECK(contains(report.value(), "sACN -> Multicast, Universe 5, 510 Bytes"));
    CHECK(contains(report.value(), "sACN -> Multicast, Universe 6"));  // 200 LEDs laufen über
    CHECK(contains(report.value(), "Art-Net -> 10.0.0.9:7000, Universe 2, 12 Bytes"));
    CHECK(contains(report.value(), "DDP -> 10.0.0.10:4048, Offset 36, 12 Bytes"));
    CHECK(contains(report.value(), "FEHLT"));
}

TEST_CASE("Bericht: Konfigurationsfehler kommen als Fehler an", "[cli]") {
    auto library = nodes::BuiltinLibrary::create().value();
    const auto parsed = project::parseProject(
        "whitehole-project 1\nfixture A\n  matrix 2 2\nend\n", library->registry);
    REQUIRE(parsed.ok());
    CHECK_FALSE(runtime::summarize(parsed.value()).ok());
}

TEST_CASE("Kommandozeile: --dmx-in wählt die Protokolle", "[cli][dmx]") {
    CHECK_FALSE(options({"p"}).dmxArtnet);
    CHECK_FALSE(options({"p"}).dmxSacn);
    const cli::Options art = options({"p", "--dmx-in", "artnet"});
    CHECK(art.dmxArtnet);
    CHECK_FALSE(art.dmxSacn);
    const cli::Options sacn = options({"p", "--dmx-in", "sacn"});
    CHECK_FALSE(sacn.dmxArtnet);
    CHECK(sacn.dmxSacn);
    const cli::Options both = options({"p", "--dmx-in", "sacn,artnet"});
    CHECK(both.dmxArtnet);
    CHECK(both.dmxSacn);
    CHECK(contains(optionError({"p", "--dmx-in"}), "braucht"));
    CHECK(contains(optionError({"p", "--dmx-in", "midi"}), "midi"));
    CHECK(contains(optionError({"p", "--dmx-in", "artnet,"}), "nur artnet und sacn"));
}

TEST_CASE("Bericht: DMX-Nodes nennen die gelesenen Universen", "[cli][dmx]") {
    auto library = nodes::BuiltinLibrary::create().value();
    const std::string base =
        "whitehole-project 1\n"
        "destination 0 127.0.0.1\n"
        "fixture A\n  matrix 2 2\n  patch artnet destination 0 universe 0\nend\n"
        "node g wh.generator.plasma\nend\n"
        "output g.out\n";
    const auto without = project::parseProject(base, library->registry);
    REQUIRE(without.ok());
    CHECK_FALSE(contains(runtime::summarize(without.value()).value(), "DMX-Eingang"));

    const auto with = project::parseProject(
        base + "node d wh.value.dmx\n  set source 1\n  set universe 7\nend\nnode e wh.value.dmx\nend\n", library->registry);
    REQUIRE(with.ok());
    const auto report = runtime::summarize(with.value());
    REQUIRE(report.ok());
    INFO(report.value());
    CHECK(contains(report.value(), "DMX-Eingang"));
    CHECK(contains(report.value(), "Art-Net, Universe 1\n"));
    CHECK(contains(report.value(), "sACN, Universe 7\n"));
}
