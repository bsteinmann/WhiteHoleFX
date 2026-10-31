// Tests der Node-Presets: Text hin und zurück, Einfügen als normale Nodes, Namen und Ordner.
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <variant>
#include <vector>

#include "fake_gpu_device.hpp"
#include "recording_sink.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/node_preset.hpp"
#include "whitehole/runtime/player.hpp"

using namespace whitehole;
using whitehole::test::FakeGpuDevice;
using whitehole::testing::RecordingSink;
namespace fs = std::filesystem;

namespace {

const char* const kProject = R"(whitehole-project 1
destination 0 10.0.0.5
fixture Panel
  matrix 4 2
  patch artnet destination 0 universe 1
end
node plasma wh.generator.plasma
  set scale 5
  at 100 40
end
node spiegel wh.transform.mirror
  at 360 40
end
node rot wh.transform.rotate
  set angle 33
  at 620 40
end
connect plasma.out -> spiegel.source
connect spiegel.out -> rot.source
output rot.out
)";

struct Rig {
    std::unique_ptr<nodes::BuiltinLibrary> library = nodes::BuiltinLibrary::create().value();
    FakeGpuDevice device;
    RecordingSink recording;
    runtime::GatedPacketSink gate{recording};
    runtime::Player player{&device, *library, gate};

    Rig() { REQUIRE(player.loadText(kProject).ok()); }

    [[nodiscard]] NodeId id(const std::string& typeId) const {
        for (const auto& node : player.project()->graph.nodes()) {
            if (node.typeId == typeId) {
                return node.id;
            }
        }
        FAIL("Node nicht gefunden: " << typeId);
        return {};
    }
    [[nodiscard]] runtime::NodeClipboard twoNodes() const {
        const std::vector<NodeId> selection{id("wh.generator.plasma"), id("wh.transform.mirror")};
        return runtime::copyNodes(player.project()->graph, selection);
    }
};

/// Ein frischer Ordner für den Test, der am Ende wieder verschwindet.
struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() / ("whfx_preset_test_" + std::to_string(std::random_device{}()));
        fs::remove_all(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

}  // namespace

TEST_CASE("Preset: Text enthält Nodes und Kabel, aber kein Layout und keinen Ausgang", "[preset]") {
    Rig rig;
    const auto text = runtime::presetToText(rig.twoNodes(), rig.library->registry);
    REQUIRE(text.ok());
    CHECK(text.value().find("wh.generator.plasma") != std::string::npos);
    CHECK(text.value().find("wh.transform.mirror") != std::string::npos);
    CHECK(text.value().find("connect ") != std::string::npos);
    CHECK(text.value().find("wh.transform.rotate") == std::string::npos);  // nicht ausgewählt
    CHECK(text.value().find("fixture") == std::string::npos);
    CHECK(text.value().find("output ") == std::string::npos);
}

TEST_CASE("Preset: Text hin und zurück erhält Typen, Parameter, Lage und Kabel", "[preset]") {
    Rig rig;
    const runtime::NodeClipboard original = rig.twoNodes();
    const auto text = runtime::presetToText(original, rig.library->registry);
    REQUIRE(text.ok());
    const auto back = runtime::presetFromText(text.value(), rig.library->registry);
    REQUIRE(back.ok());
    REQUIRE(back.value().nodes.size() == 2);
    for (std::size_t i = 0; i < 2; ++i) {
        CHECK(back.value().nodes[i].typeId == original.nodes[i].typeId);
        CHECK(back.value().nodes[i].position == original.nodes[i].position);
    }
    REQUIRE(back.value().wires.size() == 1);
    CHECK(back.value().wires[0].toPort == "source");
    bool scale = false;
    for (const auto& [key, value] : back.value().nodes[0].parameters) {
        if (key == "scale") {
            scale = true;
            CHECK(std::get<double>(value) == 5.0);
        }
    }
    CHECK(scale);
}

TEST_CASE("Preset: wird als normale Nodes eingefügt, ein Schritt für Rückgängig", "[preset]") {
    Rig rig;
    const auto text = runtime::presetToText(rig.twoNodes(), rig.library->registry);
    REQUIRE(text.ok());
    const auto clip = runtime::presetFromText(text.value(), rig.library->registry);
    REQUIRE(clip.ok());
    const std::size_t nodesBefore = rig.player.project()->graph.nodes().size();
    const std::size_t wiresBefore = rig.player.project()->graph.connections().size();
    const auto pasted = rig.player.pasteNodes(clip.value(), Vec2{0.0F, 200.0F});
    REQUIRE(pasted.ok());
    CHECK(rig.player.project()->graph.nodes().size() == nodesBefore + 2);
    CHECK(rig.player.project()->graph.connections().size() == wiresBefore + 1);
    REQUIRE(rig.player.undo().ok());
    CHECK(rig.player.project()->graph.nodes().size() == nodesBefore);
}

TEST_CASE("Preset: Fehlerfälle", "[preset]") {
    Rig rig;
    CHECK_FALSE(runtime::presetToText(runtime::NodeClipboard{}, rig.library->registry).ok());

    const auto garbage = runtime::presetFromText("whitehole-project 1\nblubb\n", rig.library->registry);
    REQUIRE_FALSE(garbage.ok());
    CHECK(garbage.error().message.find("Zeile 2") != std::string::npos);

    const auto noNodes = runtime::presetFromText("whitehole-project 1\n", rig.library->registry);
    REQUIRE_FALSE(noNodes.ok());

    const auto unknown = runtime::presetFromText("whitehole-project 1\nnode a wh.gibt.es.nicht\nend\n", rig.library->registry);
    CHECK_FALSE(unknown.ok());
}

TEST_CASE("Preset: Namen müssen als Dateiname taugen", "[preset]") {
    CHECK(runtime::isValidPresetName("Plasma mit Rand"));
    CHECK(runtime::isValidPresetName("Größe & Farbe (2)"));
    CHECK_FALSE(runtime::isValidPresetName(""));
    CHECK_FALSE(runtime::isValidPresetName("../x"));
    CHECK_FALSE(runtime::isValidPresetName("a/b"));
    CHECK_FALSE(runtime::isValidPresetName("a\\b"));
    CHECK_FALSE(runtime::isValidPresetName("a:b"));
    CHECK_FALSE(runtime::isValidPresetName("was?"));
    CHECK_FALSE(runtime::isValidPresetName(".versteckt"));
    CHECK_FALSE(runtime::isValidPresetName("endet.mit.punkt."));
    CHECK_FALSE(runtime::isValidPresetName(" Leerzeichen"));
    CHECK_FALSE(runtime::isValidPresetName(std::string(65, 'x')));
    CHECK_FALSE(runtime::isValidPresetName(std::string{"zeile\n"}));
}

TEST_CASE("Preset-Ordner: speichern, auflisten, laden, ersetzen, löschen", "[preset]") {
    TempDir dir;
    const runtime::PresetStore store{dir.path / "presets"};  // den Ordner gibt es noch nicht
    CHECK(store.list().empty());

    REQUIRE(store.save("beta", "B").ok());   // legt den Ordner an
    REQUIRE(store.save("Alpha", "A").ok());
    REQUIRE(store.save("Grüße", "G").ok());
    const std::vector<std::string> names = store.list();
    REQUIRE(names.size() == 3);
    CHECK(names[0] == "Alpha");                // ohne Rücksicht auf Groß-/Kleinschreibung
    CHECK(names[1] == "beta");
    CHECK(names[2] == "Grüße");
    CHECK(store.contains("beta"));
    CHECK_FALSE(store.contains("gamma"));

    CHECK(store.load("Alpha").value() == "A");
    CHECK(store.load("Grüße").value() == "G");
    REQUIRE(store.save("Alpha", "A2").ok());   // ersetzt
    CHECK(store.load("Alpha").value() == "A2");
    CHECK(store.list().size() == 3);

    REQUIRE(store.remove("beta").ok());
    CHECK(store.list().size() == 2);
    CHECK_FALSE(store.contains("beta"));
    CHECK(store.remove("beta").error().code == ErrorCode::NotFound);
    CHECK(store.load("beta").error().code == ErrorCode::NotFound);
}

TEST_CASE("Preset-Ordner: ungültige Namen werden abgelehnt, fremde Dateien ignoriert", "[preset]") {
    TempDir dir;
    const runtime::PresetStore store{dir.path};
    CHECK(store.save("../boese", "x").error().code == ErrorCode::InvalidArgument);
    CHECK(store.load("a/b").error().code == ErrorCode::InvalidArgument);
    CHECK(store.remove("").error().code == ErrorCode::InvalidArgument);

    REQUIRE(store.save("gut", "x").ok());
    std::ofstream{dir.path / "notiz.txt"} << "kein Preset";
    fs::create_directory(dir.path / "ordner.whfxnodes");  // ein Ordner mit passender Endung zählt nicht
    CHECK(store.list() == std::vector<std::string>{"gut"});
    CHECK(fs::exists(dir.path.parent_path()));            // und es wurde nichts außerhalb angelegt
}

TEST_CASE("Preset-Ordner: ganzer Weg Player -> Datei -> Player", "[preset]") {
    Rig rig;
    TempDir dir;
    const runtime::PresetStore store{dir.path};
    const auto text = runtime::presetToText(rig.twoNodes(), rig.library->registry);
    REQUIRE(text.ok());
    REQUIRE(store.save("Plasma gespiegelt", text.value()).ok());

    Rig other;  // ein zweites Projekt: das Preset gilt projektübergreifend
    const auto loaded = store.load("Plasma gespiegelt");
    REQUIRE(loaded.ok());
    const auto clip = runtime::presetFromText(loaded.value(), other.library->registry);
    REQUIRE(clip.ok());
    const std::size_t before = other.player.project()->graph.nodes().size();
    REQUIRE(other.player.pasteNodes(clip.value(), Vec2{0.0F, 0.0F}).ok());
    CHECK(other.player.project()->graph.nodes().size() == before + 2);
}
