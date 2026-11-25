// Tests des festen Live-Rasters in der Projektdatei: Lesen, Schreiben, Rundlauf, Fehler, Hilfsfunktionen, Übernahme.
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "whitehole/compiler/field_compiler.hpp"
#include "whitehole/graph/node_registry_impl.hpp"
#include "whitehole/nodes/builtin_nodes.hpp"
#include "whitehole/project/project.hpp"

using namespace whitehole;
using project::LiveGrid;
using project::LookTrigger;

namespace {

struct Library {
    std::shared_ptr<graph::NodeRegistry> registry = std::make_shared<graph::NodeRegistry>();
    compiler::SnippetLibrary snippets;
    values::CpuKernelLibrary kernels;
    Library() { REQUIRE(nodes::registerBuiltinNodes(*registry, snippets, kernels).ok()); }
};

const std::string kHead = R"(whitehole-project 1
destination 0 127.0.0.1
fixture Wand
  matrix 4 2
  patch artnet destination 0 universe 0
end
node a wh.generator.plasma
end
output a.out

look Nebel fade 1 trigger flash in 0.5 return 1.5
node n wh.generator.noise
end
output n.out
endlook

look "Bunte Welle" fade 3
node w wh.generator.plasma
end
output w.out
endlook
)";

}  // namespace

TEST_CASE("livegrid: Felder werden gelesen, derselbe Look darf mehrfach liegen", "[project][livegrid]") {
    Library lib;
    const auto parsed = project::parseProject(kHead + R"(
livegrid 3 2
cell 1 1 look Nebel
cell 1 3 look Nebel flash in 0.25 return 2
cell 2 2 look "Bunte Welle"
)",
                                              lib.registry);
    REQUIRE(parsed.ok());
    const LiveGrid& grid = parsed.value().liveGrid;
    REQUIRE(grid.defined());
    CHECK(grid.columns == 3);
    CHECK(grid.rows == 2);
    REQUIRE(grid.cells.size() == 6);
    CHECK(grid.cells[0].look == "Nebel");
    CHECK(grid.cells[0].trigger == LookTrigger::Toggle);
    CHECK(grid.cells[1].empty());
    CHECK(grid.cells[2].look == "Nebel");
    CHECK(grid.cells[2].trigger == LookTrigger::Flash);
    CHECK(grid.cells[2].flashInSeconds == 0.25);
    CHECK(grid.cells[2].returnSeconds == 2.0);
    CHECK(grid.cells[4].look == "Bunte Welle");
    CHECK(project::firstFreeLiveCell(grid) == 1);
}

TEST_CASE("livegrid: Rundlauf Schreiben und Lesen", "[project][livegrid]") {
    Library lib;
    const auto parsed = project::parseProject(kHead + "\nlivegrid 2 2\ncell 2 1 look Nebel flash return 1\ncell 1 2 look \"Bunte Welle\"\n", lib.registry);
    REQUIRE(parsed.ok());
    const auto text = project::writeProject(parsed.value());
    REQUIRE(text.ok());
    CHECK(text.value().find("livegrid 2 2\n") != std::string::npos);
    CHECK(text.value().find("cell 1 2 look \"Bunte Welle\"\n") != std::string::npos);
    CHECK(text.value().find("cell 2 1 look Nebel flash return 1\n") != std::string::npos);
    const auto again = project::parseProject(text.value(), lib.registry);
    REQUIRE(again.ok());
    const LiveGrid& a = parsed.value().liveGrid;
    const LiveGrid& b = again.value().liveGrid;
    REQUIRE(a.cells.size() == b.cells.size());
    for (std::size_t i = 0; i < a.cells.size(); ++i) {
        CHECK(a.cells[i].look == b.cells[i].look);
        CHECK(a.cells[i].trigger == b.cells[i].trigger);
        CHECK(a.cells[i].flashInSeconds == b.cells[i].flashInSeconds);
        CHECK(a.cells[i].returnSeconds == b.cells[i].returnSeconds);
    }
}

TEST_CASE("livegrid: ältere Dateien ohne Raster bleiben gültig und schreiben keines", "[project][livegrid]") {
    Library lib;
    const auto parsed = project::parseProject(kHead, lib.registry);
    REQUIRE(parsed.ok());
    CHECK_FALSE(parsed.value().liveGrid.defined());
    const auto text = project::writeProject(parsed.value());
    REQUIRE(text.ok());
    CHECK(text.value().find("livegrid") == std::string::npos);
}

TEST_CASE("livegrid: Fehlermeldungen", "[project][livegrid]") {
    Library lib;
    const auto bad = [&](const std::string& tail) { return project::parseProject(kHead + tail, lib.registry); };
    CHECK_FALSE(bad("cell 1 1 look Nebel\n").ok());                                      // vor livegrid
    CHECK_FALSE(bad("livegrid 0 2\n").ok());
    CHECK_FALSE(bad("livegrid 17 2\n").ok());
    CHECK_FALSE(bad("livegrid 2 17\n").ok());
    CHECK_FALSE(bad("livegrid 2 2\nlivegrid 2 2\n").ok());                               // zweimal
    CHECK_FALSE(bad("livegrid 2 2\ncell 3 1 look Nebel\n").ok());                        // Zeile außerhalb
    CHECK_FALSE(bad("livegrid 2 2\ncell 1 3 look Nebel\n").ok());                        // Spalte außerhalb
    CHECK_FALSE(bad("livegrid 2 2\ncell 0 1 look Nebel\n").ok());                        // zählt ab 1
    CHECK_FALSE(bad("livegrid 2 2\ncell 1 1 look Nebel\ncell 1 1 look Nebel\n").ok());   // doppelt
    CHECK_FALSE(bad("livegrid 2 2\ncell 1 1\n").ok());                                   // ohne look
    CHECK_FALSE(bad("livegrid 2 2\ncell 1 1 look Nebel in 1\n").ok());                   // in ohne flash
    CHECK_FALSE(bad("livegrid 2 2\ncell 1 1 look Nebel flash in 601\n").ok());           // Zeit zu groß
    CHECK_FALSE(bad("livegrid 2 2\ncell 1 1 look Nebel flash dazu\n").ok());             // unbekannte Einstellung
    const auto missing = bad("livegrid 2 2\ncell 1 1 look Gibtsnicht\n");
    REQUIRE_FALSE(missing.ok());
    CHECK(missing.error().code == ErrorCode::NotFound);
    CHECK(project::parseProject(kHead + "livegrid 2 2\n", lib.registry).ok());  // ein leeres Raster ist gültig
}

TEST_CASE("livegrid: Hilfsfunktionen", "[project][livegrid]") {
    LiveGrid grid = project::makeEmptyLiveGrid(3, 2);
    CHECK(grid.cellCount() == 6);
    grid.cells[0].look = "A";
    grid.cells[4].look = "B";  // Zeile 2, Spalte 2
    CHECK(project::firstFreeLiveCell(grid) == 1);

    project::renameLookInLiveGrid(grid, "A", "A2");
    CHECK(grid.cells[0].look == "A2");
    project::clearLookInLiveGrid(grid, "B");
    CHECK(grid.cells[4].empty());

    grid.cells[4].look = "B";
    project::resizeLiveGrid(grid, 2, 3);  // schmaler und höher: Zeile 2/Spalte 2 bleibt dort
    CHECK(grid.columns == 2);
    CHECK(grid.rows == 3);
    CHECK(grid.cells[0].look == "A2");
    CHECK(grid.cells[3].look == "B");
    project::resizeLiveGrid(grid, 1, 1);  // Felder außerhalb fallen weg
    CHECK(grid.cells.size() == 1);
    CHECK(grid.cells[0].look == "A2");

    LiveGrid full = project::makeEmptyLiveGrid(1, 1);
    full.cells[0].look = "X";
    CHECK(project::firstFreeLiveCell(full) == -1);
    CHECK(project::makeEmptyLiveGrid(99, 0).columns == project::kMaxLiveGridSide);
}

TEST_CASE("livegrid: Übernahme aus den Looks alter Projekte", "[project][livegrid]") {
    Library lib;
    const auto parsed = project::parseProject(kHead, lib.registry);
    REQUIRE(parsed.ok());
    const LiveGrid grid = project::makeLiveGridFromLooks(parsed.value().looks, 4, 4);
    REQUIRE(grid.defined());
    CHECK(grid.columns == 4);
    CHECK(grid.rows == 4);
    CHECK(grid.cells[0].look == "Nebel");
    CHECK(grid.cells[0].trigger == LookTrigger::Flash);  // Trigger und Zeiten stammen vom Look
    CHECK(grid.cells[0].flashInSeconds == 0.5);
    CHECK(grid.cells[0].returnSeconds == 1.5);
    CHECK(grid.cells[1].look == "Bunte Welle");
    CHECK(grid.cells[1].trigger == LookTrigger::Toggle);
    CHECK(grid.cells[2].empty());

    // Mehr Looks als Felder: Zeilen kommen dazu
    const LiveGrid narrow = project::makeLiveGridFromLooks(parsed.value().looks, 1, 1);
    CHECK(narrow.rows == 2);
    CHECK(narrow.cells[1].look == "Bunte Welle");
    CHECK_FALSE(project::makeLiveGridFromLooks(parsed.value().looks, 0, 4).defined());
}
