// Tests des festen Live-Rasters im Player: Übernahme, neuer Look im freien Feld, Umbenennen, Löschen, Größe.
#include <catch2/catch_test_macros.hpp>

#include <string>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;

namespace {

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
end
node plasma wh.generator.plasma
end
output plasma.out
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};
    Rig() {
        REQUIRE(player.loadText(kProject).ok());
        REQUIRE(player.start().ok());
        REQUIRE(player.step().ok());
    }
};

}  // namespace

TEST_CASE("Live-Raster: entsteht aus den Looks, ändert den Änderungsstand nicht", "[live][livegrid]") {
    Rig rig;
    CHECK_FALSE(rig.player.liveGrid().defined());
    REQUIRE(rig.player.saveLook("A", 1.0).ok());
    REQUIRE(rig.player.saveLook("B", 1.0).ok());
    REQUIRE(rig.player.setLookTrigger(1, project::LookTrigger::Flash, 0.5).ok());
    const bool modifiedBefore = rig.player.modified();

    REQUIRE(rig.player.ensureLiveGrid(4, 2).ok());
    const project::LiveGrid& grid = rig.player.liveGrid();
    REQUIRE(grid.defined());
    CHECK(grid.cells[0].look == "A");
    CHECK(grid.cells[1].look == "B");
    CHECK(grid.cells[1].trigger == project::LookTrigger::Flash);
    CHECK(grid.cells[1].returnSeconds == 0.5);
    CHECK(rig.player.modified() == modifiedBefore);

    // ein zweiter Aufruf mit anderer Größe ändert nichts
    REQUIRE(rig.player.ensureLiveGrid(2, 2).ok());
    CHECK(rig.player.liveGrid().columns == 4);
}

TEST_CASE("Live-Raster: neuer Look kommt ins erste freie Feld, Überschreiben nicht", "[live][livegrid]") {
    Rig rig;
    REQUIRE(rig.player.ensureLiveGrid(2, 1).ok());
    REQUIRE(rig.player.saveLook("A", 1.0).ok());
    REQUIRE(rig.player.saveLook("B", 1.0).ok());
    CHECK(rig.player.liveGrid().cells[0].look == "A");
    CHECK(rig.player.liveGrid().cells[1].look == "B");
    REQUIRE(rig.player.saveLook("C", 1.0).ok());  // kein freies Feld: nur gespeichert
    CHECK(rig.player.looks().size() == 3);
    CHECK(rig.player.liveGrid().cells.size() == 2);
    REQUIRE(rig.player.saveLook("A", 1.0).ok());  // überschreiben legt kein zweites Feld an
    CHECK(rig.player.liveGrid().cells[0].look == "A");
    CHECK(rig.player.liveGrid().cells[1].look == "B");
}

TEST_CASE("Live-Raster: Umbenennen und Löschen ziehen die Felder nach", "[live][livegrid]") {
    Rig rig;
    REQUIRE(rig.player.saveLook("A", 1.0).ok());
    REQUIRE(rig.player.saveLook("B", 1.0).ok());
    REQUIRE(rig.player.ensureLiveGrid(3, 1).ok());
    REQUIRE(rig.player.renameLook(0, "Neu").ok());
    CHECK(rig.player.liveGrid().cells[0].look == "Neu");
    REQUIRE(rig.player.removeLook(1).ok());
    CHECK(rig.player.liveGrid().cells[1].empty());
    CHECK(rig.player.liveGrid().cells[0].look == "Neu");
}

TEST_CASE("Live-Raster: Größe ändern und im Projekt behalten", "[live][livegrid]") {
    Rig rig;
    CHECK_FALSE(rig.player.setLiveGridSize(2, 2).ok());  // noch kein Raster
    REQUIRE(rig.player.saveLook("A", 1.0).ok());
    REQUIRE(rig.player.ensureLiveGrid(2, 2).ok());
    REQUIRE(rig.player.setLiveGridSize(3, 3).ok());
    CHECK(rig.player.liveGrid().columns == 3);
    CHECK(rig.player.liveGrid().cells[0].look == "A");
    CHECK_FALSE(rig.player.setLiveGridSize(0, 3).ok());
    CHECK_FALSE(rig.player.setLiveGridSize(3, 17).ok());

    const auto text = project::writeProject(*rig.player.project());
    REQUIRE(text.ok());
    CHECK(text.value().find("livegrid 3 3\ncell 1 1 look A\n") != std::string::npos);
}

TEST_CASE("Live-Raster: Verkleinern lässt Looks nachrücken und fragt vorher, was verloren ginge", "[live][livegrid]") {
    Rig rig;
    REQUIRE(rig.player.saveLook("A", 1.0).ok());
    REQUIRE(rig.player.saveLook("B", 1.0).ok());
    REQUIRE(rig.player.saveLook("C", 1.0).ok());
    REQUIRE(rig.player.ensureLiveGrid(3, 1).ok());  // A B C in einer Zeile
    CHECK(rig.player.liveGridCellsLost(3, 1) == 0);
    CHECK(rig.player.liveGridCellsLost(2, 1) == 1);  // C hat keinen Platz mehr
    CHECK(rig.player.liveGridCellsLost(2, 2) == 0);  // C rückt in das erste freie Feld der zweiten Zeile
    CHECK(rig.player.liveGridCellsLost(0, 1) == 0);  // ungültige Größe: nichts zu melden

    // ohne Erlaubnis zu verlieren ändert sich nichts
    const auto refused = rig.player.setLiveGridSize(2, 1, false);
    REQUIRE_FALSE(refused.ok());
    CHECK(rig.player.liveGrid().columns == 3);
    CHECK(rig.player.liveGrid().cells[2].look == "C");

    // nichts geht verloren: erlaubt auch ohne allowDrop, C rückt nach
    REQUIRE(rig.player.setLiveGridSize(2, 2, false).ok());
    CHECK(rig.player.liveGrid().columns == 2);
    CHECK(rig.player.liveGrid().cells[0].look == "A");
    CHECK(rig.player.liveGrid().cells[1].look == "B");
    CHECK(rig.player.liveGrid().cells[2].look == "C");

    // mit Erlaubnis fällt weg, was nicht mehr passt; die Looks bleiben im Projekt
    REQUIRE(rig.player.setLiveGridSize(2, 1, true).ok());
    CHECK(rig.player.liveGrid().cells.size() == 2);
    CHECK(rig.player.looks().size() == 3);
}

TEST_CASE("Live-Raster: Felder belegen und prüfen", "[live][livegrid]") {
    Rig rig;
    REQUIRE(rig.player.saveLook("A", 1.0).ok());
    project::LiveCell plain;
    plain.look = "A";
    CHECK_FALSE(rig.player.setLiveCell(0, plain).ok());  // noch kein Raster
    REQUIRE(rig.player.ensureLiveGrid(2, 2).ok());
    // der Look ist durch das Anlegen schon auf Feld 0; ihn zusätzlich auf Feld 3 legen, als Flash
    project::LiveCell flash;
    flash.look = "A";
    flash.trigger = project::LookTrigger::Flash;
    flash.flashInSeconds = 0.25;
    flash.returnSeconds = 2.0;
    REQUIRE(rig.player.setLiveCell(3, flash).ok());
    CHECK(rig.player.liveGrid().cells[0].look == "A");
    CHECK(rig.player.liveGrid().cells[3].trigger == project::LookTrigger::Flash);
    CHECK(rig.player.liveGrid().cells[3].returnSeconds == 2.0);
    CHECK(rig.player.modified());

    CHECK_FALSE(rig.player.setLiveCell(9, flash).ok());
    project::LiveCell unknown;
    unknown.look = "Gibtsnicht";
    CHECK_FALSE(rig.player.setLiveCell(1, unknown).ok());
    flash.returnSeconds = 700.0;
    CHECK_FALSE(rig.player.setLiveCell(1, flash).ok());

    project::LiveCell toggle;
    toggle.look = "A";
    toggle.flashInSeconds = 1.0;  // entfällt beim Umschalten
    REQUIRE(rig.player.setLiveCell(1, toggle).ok());
    CHECK_FALSE(rig.player.liveGrid().cells[1].flashInSeconds.has_value());

    REQUIRE(rig.player.setLiveCell(1, project::LiveCell{}).ok());  // leeren
    CHECK(rig.player.liveGrid().cells[1].empty());
}
