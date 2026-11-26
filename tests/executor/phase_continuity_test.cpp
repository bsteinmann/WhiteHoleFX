// FieldPass und @phase: Verstellt man einen Zeit-Regler (hier die Geschwindigkeit des Labyrinths), springt die
// Animation nicht mehr, sondern läuft vom aktuellen Stand weiter. Rückmeldung vom 26.11.: Beim Anfassen des
// Speed-Reglers tauchten die Läufer an neuen Positionen auf, weil der Shader `Zeit * Speed` rechnete.
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "stage_fixture.hpp"
#include "whitehole/compiler/parameter_layout.hpp"
#include "whitehole/executor/sampling_render_stage.hpp"
#include "whitehole/mapping/layout.hpp"

using namespace whitehole;
using namespace whitehole::test;

namespace {

mapping::LedMap bake(std::uint32_t cols, std::uint32_t rows) {
    mapping::MatrixShape matrix;
    matrix.columns = cols;
    matrix.rows = rows;
    mapping::Layout layout;
    REQUIRE(layout.add("Panel", Vec3{}, matrix).ok());
    return layout.bake().value();
}

float floatAt(const std::vector<std::byte>& bytes, std::uint32_t offset) {
    float value = 0.0F;
    std::memcpy(&value, bytes.data() + offset, sizeof value);
    return value;
}

/// Labyrinth -> Rotate (Ziel). Rotate ist nur da, damit sich die Struktur später ändern lässt (Mix dazwischen).
struct MazeFixture {
    Lab lab;
    FakeGpuDevice device;
    NodeId maze;
    NodeId rotate;
    std::unique_ptr<executor::SamplingRenderStage> stage;
    std::uint64_t frame{0};

    MazeFixture() {
        maze = lab.add("wh.generator.maze");
        rotate = lab.add("wh.transform.rotate");
        lab.connect(maze, "out", rotate, "source");
        executor::SamplingStageConfig config;
        config.target = graph::PortRef{rotate, "out"};
        config.ledCapacity = 64;
        auto created = executor::SamplingRenderStage::create(device, lab.snippets, lab.kernels, config);
        REQUIRE(created.ok());
        stage = std::move(created.value());
        REQUIRE(stage->setMap(bake(8, 8)).ok());
        REQUIRE(stage->setGraph(lab.graph).ok());
    }

    /// Rendert einen Frame zur Zeit `seconds` und liefert die Phase des Speed-Reglers von `node` aus dem Parameterpuffer.
    float renderPhase(NodeId node, double seconds) {
        FrameTime time;
        time.frame = frame++;
        time.time = Seconds{seconds};
        REQUIRE(device.submit([&](gpu::ICommandRecorder& r) { stage->record(time, r); }).ok());
        const auto compiled =
            compiler::compileField(lab.graph, lab.snippets, graph::PortRef{rotate, "out"});
        REQUIRE(compiled.ok());
        for (const compiler::ParameterSlot& slot : compiled.value().layout.slots) {
            if (slot.node == node && slot.key == compiler::phaseSlotKey("speed")) {
                const auto params = device.bufferBytes(bufferAt(device.dispatches().back(), 0, 1));
                return floatAt(params, slot.offsetBytes);
            }
        }
        FAIL("kein Phase-Slot für speed gefunden");
        return 0.0F;
    }
};

}  // namespace

TEST_CASE("Labyrinth: unberührter Speed ergibt die Phase Zeit mal Speed wie bisher") {
    MazeFixture fx;  // speed 3
    CHECK(fx.renderPhase(fx.maze, 0.0) == 0.0F);
    CHECK(fx.renderPhase(fx.maze, 2.0) == 6.0F);
    CHECK(fx.renderPhase(fx.maze, 10.0) == 30.0F);
}

TEST_CASE("Labyrinth: den Speed-Regler zu verstellen lässt die Läufer nicht springen") {
    MazeFixture fx;
    CHECK(fx.renderPhase(fx.maze, 10.0) == 30.0F);

    fx.lab.set(fx.maze, "speed", 12.0);
    REQUIRE(fx.stage->setParameters(fx.lab.graph).ok());
    CHECK(fx.renderPhase(fx.maze, 10.0) == 30.0F);  // derselbe Augenblick: dieselbe Phase, also dieselben Positionen
    CHECK(fx.renderPhase(fx.maze, 11.0) == 42.0F);  // ab hier läuft es mit 12 Zellen pro Sekunde weiter

    fx.lab.set(fx.maze, "speed", 0.0);  // anhalten
    REQUIRE(fx.stage->setParameters(fx.lab.graph).ok());
    CHECK(fx.renderPhase(fx.maze, 11.0) == 42.0F);
    CHECK(fx.renderPhase(fx.maze, 15.0) == 42.0F);
}

TEST_CASE("Labyrinth: die Phase bleibt auch erhalten, wenn sich die Struktur des Graphen ändert") {
    MazeFixture fx;
    fx.lab.set(fx.maze, "speed", 5.0);
    REQUIRE(fx.stage->setParameters(fx.lab.graph).ok());
    CHECK(fx.renderPhase(fx.maze, 4.0) == 20.0F);
    fx.lab.set(fx.maze, "speed", 10.0);
    REQUIRE(fx.stage->setParameters(fx.lab.graph).ok());
    CHECK(fx.renderPhase(fx.maze, 4.0) == 20.0F);

    // Struktur ändern: Mix(Labyrinth, Plasma) statt Labyrinth direkt an Rotate. Das Labyrinth bleibt derselbe Node.
    const graph::Connection* old = fx.lab.graph.findInputConnection(fx.rotate, "source");
    REQUIRE(old != nullptr);
    REQUIRE(fx.lab.graph.disconnect(old->id).ok());
    const NodeId plasma = fx.lab.add("wh.generator.plasma");
    const NodeId mix = fx.lab.add("wh.blend.mix");
    fx.lab.connect(fx.maze, "out", mix, "a");
    fx.lab.connect(plasma, "out", mix, "b");
    fx.lab.connect(mix, "out", fx.rotate, "source");
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());

    CHECK(fx.renderPhase(fx.maze, 4.0) == 20.0F);
    CHECK(fx.renderPhase(fx.maze, 5.0) == 30.0F);
}

TEST_CASE("Labyrinth: ein neu eingesetzter Node beginnt wie bisher bei Zeit mal Speed") {
    MazeFixture fx;
    CHECK(fx.renderPhase(fx.maze, 10.0) == 30.0F);
    fx.lab.set(fx.maze, "speed", 12.0);
    REQUIRE(fx.stage->setParameters(fx.lab.graph).ok());
    CHECK(fx.renderPhase(fx.maze, 11.0) == 33.0F);  // Regler erst bei t = 11 verstellt: dort bleibt es bei 3 * 11
    CHECK(fx.renderPhase(fx.maze, 12.0) == 45.0F);

    // Wie beim Live-Betrieb: der Look wird mit einem neuen Labyrinth-Node (neue Nummer) eingesetzt.
    REQUIRE(fx.lab.graph.removeNode(fx.maze).ok());
    const NodeId again = fx.lab.add("wh.generator.maze");
    fx.lab.set(again, "speed", 4.0);
    fx.lab.connect(again, "out", fx.rotate, "source");
    REQUIRE(fx.stage->setGraph(fx.lab.graph).ok());
    CHECK(fx.renderPhase(again, 11.0) == 44.0F);
}
