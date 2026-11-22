// Projektion auf einem echten Gerät: Ein hohler Würfel aus sechs Matrizen mit Projektion "box" zeigt auf jeder Fläche
// dasselbe Bild wie eine einzelne flache Matrix mit der alten Raumabbildung, auch für einen flächenbasierten Effekt
// (Labyrinth mit Zellen und Raum auf Automatik). Warum diese Datei: Erst im Zusammenspiel von p (Layout::bake), @columns/
// @rows/@aspect/@volume (Abtast-Stage) und dem Node stimmt das Bild; die Teile einzeln zu testen genügt dafür nicht.
// Ohne Adapter übersprungen.
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>
#include <memory>
#include <vector>

#include "builtin_lab.hpp"
#include "gpu_stage_support.hpp"
#include "whitehole/executor/sampling_render_stage.hpp"
#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#include "whitehole/mapping/layout.hpp"

using namespace whitehole;
using namespace whitehole::gpu;
using whitehole::test::Lab;
using namespace whitehole::gputest;

namespace {

constexpr std::uint32_t kN = 8;

std::vector<ColorRgba> renderMap(WebGpuDevice& device, const mapping::LedMap& map) {
    Lab lab;
    const NodeId maze = lab.add("wh.generator.maze");
    lab.set(maze, "runners", std::int64_t{4});
    lab.set(maze, "speed", 4.0);
    lab.set(maze, "trail", 8.0);  // space und cells bleiben auf Automatik

    executor::SamplingStageConfig config;
    config.target = graph::PortRef{maze, "out"};
    config.ledCapacity = 1024;
    auto created = executor::SamplingRenderStage::create(device, lab.snippets, lab.kernels, config);
    REQUIRE(created.ok());
    std::unique_ptr<executor::SamplingRenderStage> stage = std::move(created.value());
    REQUIRE(stage->setMap(map).ok());
    REQUIRE(stage->setGraph(lab.graph).ok());

    const BufferHandle target = readbackBuffer(device, stage->outputSizeBytes());
    REQUIRE(device.submit([&](ICommandRecorder& r) {
                      stage->record(frameAt(0, 1.7), r);
                      r.copyBufferToBuffer(stage->output(), target, stage->outputSizeBytes());
                  }).ok());
    const std::vector<std::byte> bytes = readBack(device, target);
    device.destroy(target);
    std::vector<ColorRgba> colors(map.size());
    REQUIRE(bytes.size() >= colors.size() * sizeof(ColorRgba));
    std::memcpy(colors.data(), bytes.data(), colors.size() * sizeof(ColorRgba));
    return colors;
}

bool same(const ColorRgba& a, const ColorRgba& b) {
    return std::abs(a.r - b.r) < 1e-4F && std::abs(a.g - b.g) < 1e-4F && std::abs(a.b - b.b) < 1e-4F &&
           std::abs(a.a - b.a) < 1e-4F;
}

}  // namespace

TEST_CASE("Projektion: jede Würfelfläche (box) zeigt das Bild einer flachen Matrix", "[gpu_device][stage][projection]") {
    const std::unique_ptr<WebGpuDevice> device = makeDevice();
    if (!device) {
        SKIP("Kein WebGPU-Adapter verfügbar");
    }
    mapping::MatrixShape m;
    m.columns = kN;
    m.rows = kN;

    mapping::Layout flat;
    REQUIRE(flat.add("Flach", Vec3{}, m).ok());
    const std::vector<ColorRgba> reference = renderMap(*device, flat.bake().value());
    REQUIRE(reference.size() == static_cast<std::size_t>(kN) * kN);
    bool varied = false;  // sonst verglichen wir nur ein einfarbiges Bild
    for (const ColorRgba& c : reference) {
        varied = varied || !same(c, reference.front());
    }
    REQUIRE(varied);

    mapping::Layout cube;
    const auto add = [&](const char* name, const Vec3& origin, const Vec3& rotation) {
        const auto id = cube.add(name, origin, m);
        REQUIRE(id.ok());
        REQUIRE(cube.rotate(id.value(), rotation).ok());
        REQUIRE(cube.setProjection(id.value(), mapping::Projection::Box).ok());
    };
    const auto e = static_cast<float>(kN);
    add("Vorne", Vec3{0, 0, 0}, Vec3{0, 0, 0});
    add("Hinten", Vec3{0, 0, e}, Vec3{0, 180, 0});
    add("Links", Vec3{-e / 2, 0, e / 2}, Vec3{0, 90, 0});
    add("Rechts", Vec3{e / 2, 0, e / 2}, Vec3{0, -90, 0});
    add("Oben", Vec3{0, e / 2, e / 2}, Vec3{90, 0, 0});
    add("Unten", Vec3{0, -e / 2, e / 2}, Vec3{-90, 0, 0});
    const mapping::LedMap map = cube.bake().value();
    const std::vector<ColorRgba> colors = renderMap(*device, map);
    REQUIRE(colors.size() == 6U * kN * kN);

    // Die Flächen sind gegeneinander gedreht, also gleiche p, aber eine andere Kabelreihenfolge: Pro Fläche ordnen wir
    // die LEDs nach ihrer 2D-Koordinate und vergleichen mit der flachen Matrix an derselben Koordinate.
    for (const mapping::FixtureRange& range : map.ranges) {
        for (std::uint32_t k = 0; k < range.count; ++k) {
            const Vec3& p = map.unit[range.first + k];
            const auto cx = static_cast<std::uint32_t>(std::lround(p.x * static_cast<float>(kN) - 0.5F));
            const auto cy = static_cast<std::uint32_t>(std::lround(p.y * static_cast<float>(kN) - 0.5F));
            // In der flachen Matrix liegen die LEDs zeilenweise ab (0,0); finde die mit derselben Zelle.
            const mapping::LedMap flatMap = [&] {
                mapping::Layout l;
                REQUIRE(l.add("Flach", Vec3{}, m).ok());
                return l.bake().value();
            }();
            std::size_t match = flatMap.size();
            for (std::size_t i = 0; i < flatMap.size(); ++i) {
                if (std::lround(flatMap.unit[i].x * static_cast<float>(kN) - 0.5F) == cx &&
                    std::lround(flatMap.unit[i].y * static_cast<float>(kN) - 0.5F) == cy) {
                    match = i;
                    break;
                }
            }
            REQUIRE(match < flatMap.size());
            INFO("Zelle " << cx << "," << cy << " Fixture " << range.fixture.value());
            CHECK(same(colors[range.first + k], reference[match]));
        }
    }
}
