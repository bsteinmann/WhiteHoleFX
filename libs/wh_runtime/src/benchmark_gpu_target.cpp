// benchmark_gpu_target.cpp: Stufen aufbauen und Frames synchron messen (siehe benchmark_gpu_target.hpp).
#include "whitehole/runtime/benchmark_gpu_target.hpp"

#include <algorithm>
#include <cmath>
#include <thread>

#include "whitehole/executor/canvas_render_stage.hpp"
#include "whitehole/executor/sampling_render_stage.hpp"
#include "whitehole/mapping/layout.hpp"
#include "whitehole/runtime/benchmark_scenes.hpp"

namespace whitehole::runtime {

namespace {

using Clock = std::chrono::steady_clock;
constexpr int kGpuTimePolls = 50;  // Geräte ohne Zeitstempel kostet das nur ein paar leere poll()-Aufrufe

double msSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

/// Matrix mit genau `leds` LEDs, möglichst quadratisch (1 000 = 40 × 25, 1 Mio. = 1000 × 1000).
Result<mapping::LedMap> matrixOf(std::uint64_t leds) {
    auto columns = static_cast<std::uint32_t>(std::sqrt(static_cast<double>(leds)));
    while (columns > 1 && leds % columns != 0) {
        --columns;
    }
    mapping::MatrixShape shape;
    shape.columns = std::max<std::uint32_t>(columns, 1);
    shape.rows = static_cast<std::uint32_t>(leds / shape.columns);
    mapping::Layout layout;
    if (auto added = layout.add("Benchmark", Vec3{}, shape); !added.ok()) {
        return added.error();
    }
    return layout.bake();
}

}  // namespace

GpuBenchmarkTarget::GpuBenchmarkTarget(gpu::IGpuDevice& device, const nodes::BuiltinLibrary& library,
                                       GpuBenchmarkOptions options)
    : device_{device}, library_{library}, options_{std::move(options)} {}

GpuBenchmarkTarget::~GpuBenchmarkTarget() {
    release();
}

void GpuBenchmarkTarget::release() noexcept {
    canvas_.reset();
    sampling_.reset();
    for (gpu::BufferHandle* handle : {&fence_, &readback_}) {
        if (handle->valid()) {
            device_.destroy(*handle);
            *handle = {};
        }
    }
    copySource_ = {};
    readbackBytes_ = 0;
}

Result<void> GpuBenchmarkTarget::prepare(const BenchmarkStage& stage) {
    release();
    auto scene = makeBenchmarkScene(stage.scene, library_.registry);
    if (!scene.ok()) {
        return scene.error();
    }
    if (stage.kind == BenchmarkKind::Projection) {
        executor::CanvasStageConfig config;
        config.extent = Extent3{stage.width, stage.height, 1};
        config.target = scene.value().output;
        config.label = "wh.benchmark.canvas";
        auto created = executor::CanvasRenderStage::create(device_, library_.snippets, library_.kernels, config);
        if (!created.ok()) {
            return created.error();
        }
        canvas_ = std::move(created.value());
        if (auto set = canvas_->setGraph(scene.value().graph); !set.ok()) {
            release();
            return set.error();
        }
        gpu::BufferDesc fence;
        fence.sizeBytes = 16;
        fence.usage = gpu::Usage::Storage | gpu::Usage::CopySource;
        fence.label = "wh.benchmark.fence";
        auto made = device_.createBuffer(fence);
        if (!made.ok()) {
            release();
            return made.error();
        }
        fence_ = made.value();
        copySource_ = fence_;
        readbackBytes_ = fence.sizeBytes;
    } else {
        auto map = matrixOf(stage.leds);
        if (!map.ok()) {
            return map.error();
        }
        executor::SamplingStageConfig config;
        config.target = scene.value().output;
        config.ledCapacity = static_cast<std::uint32_t>(map.value().size());
        config.label = "wh.benchmark.leds";
        auto created = executor::SamplingRenderStage::create(device_, library_.snippets, library_.kernels, config);
        if (!created.ok()) {
            return created.error();
        }
        sampling_ = std::move(created.value());
        if (auto set = sampling_->setMap(map.value()); !set.ok()) {
            release();
            return set.error();
        }
        if (auto set = sampling_->setGraph(scene.value().graph); !set.ok()) {
            release();
            return set.error();
        }
        copySource_ = sampling_->output();
        readbackBytes_ = sampling_->mainSizeBytes();
    }
    gpu::BufferDesc read;
    read.sizeBytes = readbackBytes_;
    read.usage = gpu::Usage::MapRead | gpu::Usage::CopyDestination;
    read.label = "wh.benchmark.readback";
    auto made = device_.createBuffer(read);
    if (!made.ok()) {
        release();
        return made.error();
    }
    readback_ = made.value();
    frameIndex_ = 0;
    seconds_ = 0.0;
    return {};
}

Result<BenchmarkFrame> GpuBenchmarkTarget::frame() {
    if (!readback_.valid()) {
        return Error{ErrorCode::Unsupported, "Benchmark: keine Stufe aufgebaut"};
    }
    const auto start = Clock::now();
    FrameTime time;
    time.frame = frameIndex_++;
    time.time = Seconds{seconds_};
    time.delta = Seconds{1.0 / 30.0};
    seconds_ += 1.0 / 30.0;  // Szenen laufen wie bei 30 fps weiter, unabhängig von der gemessenen Zeit
    (void)device_.takeGpuMilliseconds();  // ein verspäteter Wert des vorigen Frames gehört nicht zu diesem
    const auto submitted = device_.submit([&](gpu::ICommandRecorder& recorder) {
        if (canvas_) {
            canvas_->record(time, recorder);
        } else {
            sampling_->record(time, recorder);
        }
        recorder.copyBufferToBuffer(copySource_, readback_, readbackBytes_);
    });
    if (!submitted.ok()) {
        return submitted.error();
    }
    const std::optional<Error>& frameError = canvas_ ? canvas_->frameError() : sampling_->frameError();
    if (frameError) {
        return *frameError;
    }
    BenchmarkFrame result;
    result.cpuMs = msSince(start);

    // Geteilter Zustand statt Verweisen auf den Stapel: Nach einer Zeitüberschreitung kann der Rückruf noch später
    // eintreffen (oder beim Abbau des Puffers mit „abgebrochen“).
    struct Pending {
        bool done{false};
        std::optional<Error> error;
    };
    const auto pending = std::make_shared<Pending>();
    device_.readbackAsync(readback_, [pending](Result<std::span<const std::byte>> r) {
        pending->done = true;
        if (!r.ok()) pending->error = r.error();
    });
    const auto deadline = start + options_.readbackTimeout;
    while (!pending->done) {
        device_.poll();
        if (pending->done) break;
        if (Clock::now() >= deadline) {
            release();  // die Stufe ist nicht mehr verlässlich; prepare baut neu auf
            return Error{ErrorCode::GpuError, "GPU antwortet nicht"};
        }
        if (options_.waitStep) {
            options_.waitStep();
        } else {
            std::this_thread::yield();
        }
    }
    if (pending->error) {
        return *pending->error;
    }
    result.totalMs = msSince(start);
    // Die Zeitstempel werden nach dem Frame gemappt; meist sind sie mit dem Readback fertig, sonst kurz nachfragen.
    for (int i = 0; i < kGpuTimePolls; ++i) {
        if ((result.gpuMs = device_.takeGpuMilliseconds())) {
            break;
        }
        device_.poll();
    }
    return result;
}

}  // namespace whitehole::runtime
