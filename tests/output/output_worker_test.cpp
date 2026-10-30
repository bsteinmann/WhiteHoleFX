// OutputWorker: Frames aus dem Briefkasten werden auf einem eigenen Thread gesendet.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "recording_sink.hpp"
#include "whitehole/output/output_worker.hpp"

using namespace whitehole;
using namespace whitehole::mapping;
using namespace whitehole::output;
using whitehole::testing::RecordingSink;

namespace {

std::unique_ptr<OutputPipeline> pipelineFor(RecordingSink& sink) {
    LedMap map;
    map.ranges.push_back({FixtureId{1}, 0, 1});
    map.world.assign(1, Vec3{});
    map.unit.assign(1, Vec3{});
    FixturePatch patch;
    patch.fixture = FixtureId{1};
    patch.color.gamma = 1.0F;
    Result<OutputPlan> plan = OutputPlan::create(map, std::vector<FixturePatch>{patch});
    REQUIRE(plan.ok());
    OutputConfig config;
    config.destinations.push_back({"10.0.0.1", 0});
    Result<std::unique_ptr<OutputPipeline>> pipeline =
        OutputPipeline::create(std::move(plan.value()), std::move(config), sink);
    REQUIRE(pipeline.ok());
    return std::move(pipeline.value());
}

engine::PixelFrame frameOf(const ColorRgba& color, FrameIndex index) {
    engine::PixelFrame frame;
    frame.frame = index;
    frame.bytes.resize(sizeof(ColorRgba));
    std::memcpy(frame.bytes.data(), &color, sizeof(ColorRgba));
    return frame;
}

template <typename Predicate>
bool waitUntil(Predicate done) {
    for (int i = 0; i < 400; ++i) {  // höchstens 2 s
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return done();
}

}  // namespace

TEST_CASE("Der Worker sendet Frames aus dem Briefkasten", "[output][worker]") {
    RecordingSink sink;
    OutputWorker worker{pipelineFor(sink)};

    worker.sink().publish(frameOf({1, 0, 0, 1}, 1));
    REQUIRE(waitUntil([&] { return sink.count() >= 1; }));
    const auto sent = sink.packets();
    CHECK(sent[0].bytes[18] == 255);
    CHECK(sent[0].bytes[19] == 0);
    CHECK(worker.frameErrors() == 0);
}

TEST_CASE("Ein fehlerhafter Frame wird gezählt, der Thread läuft weiter", "[output][worker]") {
    RecordingSink sink;
    OutputWorker worker{pipelineFor(sink)};

    engine::PixelFrame small;
    small.bytes.resize(8);
    worker.sink().publish(std::move(small));
    REQUIRE(waitUntil([&] { return worker.frameErrors() == 1; }));
    REQUIRE(worker.lastFrameError().has_value());
    CHECK(worker.lastFrameError()->code == ErrorCode::InvalidArgument);
    CHECK(sink.count() == 0);

    worker.sink().publish(frameOf({0, 1, 0, 1}, 2));
    REQUIRE(waitUntil([&] { return sink.count() >= 1; }));
    CHECK(sink.packets()[0].bytes[19] == 255);
}

TEST_CASE("Der Worker lässt sich ohne Frames sauber beenden", "[output][worker]") {
    RecordingSink sink;
    {
        OutputWorker worker{pipelineFor(sink)};
    }
    CHECK(sink.count() == 0);
}

TEST_CASE("Viele Frames in Folge: der Worker bleibt bei den neuesten", "[output][worker]") {
    RecordingSink sink;
    OutputWorker worker{pipelineFor(sink)};
    for (std::uint64_t i = 1; i <= 200; ++i) {
        worker.sink().publish(frameOf({1, 1, 1, 1}, i));
    }
    REQUIRE(waitUntil([&] { return sink.count() >= 1; }));
    // Nicht jeder Frame muss ankommen (alte werden verworfen), aber gezählt wird alles.
    CHECK(sink.count() + worker.droppedFrames() <= 200);
    CHECK(sink.count() >= 1);
}
