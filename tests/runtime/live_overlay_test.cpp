// Flash als Overlay (Wunsch vom 27.11.): ein Flash-Feld mit `overlay` legt sich über den laufenden Look, statt ihn zu
// ersetzen. Geprüft werden die Decks (Modus und Deckkraft des Mischers), der Player (Drücken/Loslassen, Stapel, Umschalten
// beendet das Overlay) und die Projektdatei (`cell … flash overlay`). Ohne GPU: der Mischer zeigt den Zustand in seinen
// Parametern `mode` und `amount`, beim Player die Deckkraft in liveStatus().
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <string>

#include "builtin_lab.hpp"
#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/live_decks.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using Catch::Approx;
using whitehole::test::FakeGpuDevice;
using whitehole::test::Lab;
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

look Eins fade 3
node a wh.generator.plasma
end
output a.out
endlook

look Zwei fade 0.5
node b wh.generator.noise
end
output b.out
endlook

look Drei fade 1
node c wh.generator.plasma
end
output c.out
endlook

livegrid 4 2
cell 1 1 look Eins
cell 1 2 look Zwei flash in 1 return 2 overlay
cell 1 3 look Drei flash in 0 return 0 overlay
cell 1 4 look Drei flash in 0 return 0
cell 2 1 look Drei
)";

std::int64_t modeOf(const graph::Graph& graph, NodeId mixer) {
    return std::get<std::int64_t>(graph.findNode(mixer)->parameters.at("mode"));
}

double amountOf(const graph::Graph& graph, NodeId mixer) {
    return std::get<double>(graph.findNode(mixer)->parameters.at("amount"));
}

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};
    double clock{0.0};

    Rig() {
        REQUIRE(player.loadText(kProject).ok());
        player.setLiveClock([this] { return clock; });
        REQUIRE(player.start().ok());
        REQUIRE(player.startLive(0).ok());
        REQUIRE(player.step().ok());
    }
    void run(double seconds) {
        clock += seconds;
        REQUIRE(player.step().ok());
    }
    [[nodiscard]] double level() const { return player.liveStatus().overlayLevel; }
};

}  // namespace

TEST_CASE("Overlay: Decks blenden die Deckkraft ein und aus, der sichtbare Look bleibt", "[live][overlay]") {
    Lab lab;
    auto parsed = project::parseProject(kProject, lab.registry);
    REQUIRE(parsed.ok());
    const project::Project& project = parsed.value();
    runtime::LiveDecks decks{lab.registry};
    REQUIRE(decks.start(project.looks[0], 0).ok());
    const NodeId mixer = decks.target().node;

    CHECK_FALSE(decks.goOverlay(1.0).ok());  // nichts vorgeladen
    REQUIRE(decks.arm(project.looks[1], 1, 1).ok());
    REQUIRE(decks.goOverlay(2.0).ok());
    CHECK(modeOf(decks.graph(), mixer) == 2);  // A sichtbar: B liegt darüber
    CHECK(amountOf(decks.graph(), mixer) == Approx(0.0));
    runtime::LiveStatus st = decks.status();
    CHECK(st.overlay);
    CHECK(st.current == 0);
    CHECK(st.overlayLook == 1);
    CHECK(st.overlayCell == 1);
    CHECK(st.armed == -1);
    CHECK_FALSE(st.fading);
    CHECK(decks.previewDeck() == 1U);

    CHECK(decks.advance(1.0));
    CHECK(amountOf(decks.graph(), mixer) == Approx(0.5));
    CHECK(decks.advance(5.0));
    CHECK(decks.status().overlayLevel == Approx(1.0));
    CHECK_FALSE(decks.advance(1.0));  // voll da: nichts zu tun

    CHECK(decks.releaseOverlay(4.0));
    CHECK(decks.advance(1.0));
    CHECK(amountOf(decks.graph(), mixer) == Approx(0.75));
    CHECK(decks.advance(10.0));
    CHECK_FALSE(decks.status().overlay);
    CHECK(modeOf(decks.graph(), mixer) == 0);
    CHECK(amountOf(decks.graph(), mixer) == Approx(0.0));
    CHECK(decks.status().current == 0);
    CHECK_FALSE(decks.releaseOverlay(1.0));  // kein Overlay mehr

    // Loslassen, bevor überhaupt etwas eingeblendet ist: sofort weg
    REQUIRE(decks.arm(project.looks[1], 1).ok());
    REQUIRE(decks.goOverlay(2.0).ok());
    CHECK(decks.releaseOverlay(2.0));
    CHECK_FALSE(decks.status().overlay);

    // Liegt der sichtbare Look in Deck B, legt sich Deck A darüber
    REQUIRE(decks.arm(project.looks[2], 2).ok());
    REQUIRE(decks.go(0.0).ok());
    CHECK_FALSE(decks.showsDeckA());
    REQUIRE(decks.arm(project.looks[1], 1).ok());
    REQUIRE(decks.goOverlay(0.0).ok());
    CHECK(modeOf(decks.graph(), mixer) == 3);
    CHECK(amountOf(decks.graph(), mixer) == Approx(1.0));

    // Ein neues arm() beendet das Overlay; ein direkt folgendes goOverlay beginnt bei der erreichten Deckkraft
    REQUIRE(decks.arm(project.looks[0], 0).ok());
    CHECK_FALSE(decks.status().overlay);
    REQUIRE(decks.goOverlay(3.0).ok());
    CHECK(decks.status().overlayLevel == Approx(1.0));

    // endOverlay: sofort weg, der Mischer zeigt wieder nur das sichtbare Deck
    decks.endOverlay();
    CHECK_FALSE(decks.status().overlay);
    CHECK(modeOf(decks.graph(), mixer) == 0);
    CHECK(amountOf(decks.graph(), mixer) == Approx(1.0));
    CHECK(decks.status().current == 2);
}

TEST_CASE("Overlay: Feld drücken legt den Look über den laufenden, Loslassen blendet aus", "[live][overlay]") {
    Rig rig;
    REQUIRE(rig.player.liveCellDown(1).ok());  // Zwei als Overlay, Einblenden 1 s
    runtime::LiveStatus st = rig.player.liveStatus();
    CHECK(st.overlay);
    CHECK(st.current == 0);  // Eins läuft weiter und bleibt aktiv
    CHECK(st.currentCell == 0);
    CHECK(st.overlayLook == 1);
    CHECK(st.overlayCell == 1);
    CHECK_FALSE(st.fading);
    rig.run(0.0);
    rig.run(0.5);
    CHECK(rig.level() == Approx(0.5));
    rig.run(1.0);
    CHECK(rig.level() == Approx(1.0));

    REQUIRE(rig.player.liveCellUp(1).ok());  // Zurückblenden 2 s
    rig.run(0.0);
    rig.run(1.0);
    CHECK(rig.level() == Approx(0.5));
    rig.run(1.5);
    st = rig.player.liveStatus();
    CHECK_FALSE(st.overlay);
    CHECK(st.current == 0);
    CHECK(rig.player.liveFlashHeld().empty());
}

TEST_CASE("Overlay: nur das zuletzt gehaltene ist zu sehen, darunter gehaltene kehren zurück", "[live][overlay]") {
    Rig rig;
    REQUIRE(rig.player.liveCellDown(1).ok());  // Zwei
    REQUIRE(rig.player.liveCellDown(2).ok());  // Drei (sofort)
    CHECK(rig.player.liveStatus().overlayLook == 2);
    CHECK(rig.player.liveStatus().current == 0);
    REQUIRE(rig.player.liveCellUp(2).ok());  // zurück zum Overlay darunter
    CHECK(rig.player.liveStatus().overlay);
    CHECK(rig.player.liveStatus().overlayLook == 1);
    REQUIRE(rig.player.liveCellUp(1).ok());
    rig.run(0.0);
    rig.run(5.0);
    CHECK_FALSE(rig.player.liveStatus().overlay);
    CHECK(rig.player.liveStatus().current == 0);
}

TEST_CASE("Overlay: Umschalten und ersetzendes Flash beenden das Overlay", "[live][overlay]") {
    SECTION("Umschalten") {
        Rig rig;
        REQUIRE(rig.player.liveCellDown(2).ok());
        CHECK(rig.player.liveStatus().overlay);
        REQUIRE(rig.player.liveCellDown(4).ok());  // Drei umschalten
        CHECK_FALSE(rig.player.liveStatus().overlay);
        CHECK(rig.player.liveFlashHeld().empty());
        REQUIRE(rig.player.liveCellUp(2).ok());  // spätes Loslassen ändert nichts mehr
        CHECK_FALSE(rig.player.liveStatus().overlay);
    }
    SECTION("Ersetzendes Flash darüber, dann alles loslassen: zurück zum Look von vorher") {
        Rig rig;
        REQUIRE(rig.player.liveCellDown(2).ok());  // Overlay
        REQUIRE(rig.player.liveCellDown(3).ok());  // ersetzendes Flash: das Overlay endet
        CHECK_FALSE(rig.player.liveStatus().overlay);
        CHECK(rig.player.liveStatus().current == 2);
        REQUIRE(rig.player.liveCellUp(2).ok());
        REQUIRE(rig.player.liveCellUp(3).ok());
        rig.run(0.0);
        CHECK(rig.player.liveStatus().current == 0);
        CHECK(rig.player.liveStatus().currentCell == 0);
    }
    SECTION("Overlay über einem ersetzenden Flash") {
        Rig rig;
        REQUIRE(rig.player.liveCellDown(3).ok());  // Drei ersetzt Eins
        REQUIRE(rig.player.liveCellDown(1).ok());  // Zwei als Overlay darüber
        CHECK(rig.player.liveStatus().overlay);
        CHECK(rig.player.liveStatus().current == 2);
        REQUIRE(rig.player.liveCellUp(1).ok());  // nur ausblenden, das ersetzende Flash bleibt
        rig.run(0.0);
        rig.run(5.0);
        CHECK_FALSE(rig.player.liveStatus().overlay);
        CHECK(rig.player.liveStatus().current == 2);
        REQUIRE(rig.player.liveCellUp(3).ok());
        rig.run(0.0);
        CHECK(rig.player.liveStatus().current == 0);
    }
    SECTION("Ersetzendes Flash losgelassen, während das Overlay noch gehalten wird") {
        Rig rig;
        REQUIRE(rig.player.liveCellDown(3).ok());
        REQUIRE(rig.player.liveCellDown(2).ok());  // Overlay über dem ersetzenden Flash
        REQUIRE(rig.player.liveCellUp(3).ok());    // darunter: das Bild bleibt
        CHECK(rig.player.liveStatus().overlay);
        REQUIRE(rig.player.liveCellUp(2).ok());    // zurück zum Look von vorher
        rig.run(0.0);
        CHECK_FALSE(rig.player.liveStatus().overlay);
        CHECK(rig.player.liveStatus().current == 0);
    }
}

TEST_CASE("Overlay: setLiveCell und Projektdatei", "[live][overlay][project]") {
    Rig rig;
    project::LiveCell cell = rig.player.liveGrid().cells[1];
    CHECK(cell.flashOverlay);
    cell.trigger = project::LookTrigger::Toggle;  // Umschalten kennt kein Overlay
    REQUIRE(rig.player.setLiveCell(1, cell).ok());
    CHECK_FALSE(rig.player.liveGrid().cells[1].flashOverlay);

    Lab lab;
    auto parsed = project::parseProject(kProject, lab.registry);
    REQUIRE(parsed.ok());
    const auto written = project::writeProject(parsed.value());
    REQUIRE(written.ok());
    CHECK(written.value().find("cell 1 2 look Zwei flash overlay in 1 return 2") != std::string::npos);
    auto again = project::parseProject(written.value(), lab.registry);
    REQUIRE(again.ok());
    CHECK(again.value().liveGrid.cells[1].flashOverlay);
    CHECK_FALSE(again.value().liveGrid.cells[3].flashOverlay);

    const std::string bad = std::string{kProject} + "cell 2 2 look Eins overlay\n";
    CHECK_FALSE(project::parseProject(bad, lab.registry).ok());  // overlay nur zusammen mit flash
}
