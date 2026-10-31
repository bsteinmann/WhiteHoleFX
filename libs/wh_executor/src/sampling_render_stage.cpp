// sampling_render_stage.cpp: Umsetzung zu sampling_render_stage.hpp. Tastet eine Field-Kette an den Positionen der LEDs ab.
#include "whitehole/executor/sampling_render_stage.hpp"

#include <array>
#include <cstring>
#include <utility>

#include "whitehole/executor/sampling_shader.hpp"
#include "whitehole/mapping/layout.hpp"

namespace whitehole::executor {

std::vector<std::byte> packPositions(std::span<const Vec3> positions) {
    std::vector<std::byte> bytes(positions.size() * kBytesPerLed);
    std::size_t offset = 0;
    for (const Vec3& p : positions) {
        const std::array<float, 4> v{p.x, p.y, p.z, 0.0F};
        std::memcpy(bytes.data() + offset, v.data(), sizeof v);
        offset += kBytesPerLed;
    }
    return bytes;
}

SamplingRenderStage::SamplingRenderStage(gpu::IGpuDevice& device, std::uint64_t capacity,
                                         gpu::BufferHandle positions, gpu::BufferHandle colors,
                                         gpu::BufferHandle info, std::unique_ptr<FieldPass> pass)
    : device_{device},
      capacity_{capacity},
      positions_{positions},
      colors_{colors},
      info_{info},
      pass_{std::move(pass)} {}

SamplingRenderStage::~SamplingRenderStage() {
    pass_.reset();  // Pipeline und Puffer des Passes zuerst
    device_.destroy(info_);
    device_.destroy(colors_);
    device_.destroy(positions_);
}

Result<std::unique_ptr<SamplingRenderStage>> SamplingRenderStage::create(
    gpu::IGpuDevice& device, const compiler::ISnippetLibrary& snippets,
    const values::ICpuKernelLibrary& kernels, SamplingStageConfig config) {
    if (config.ledCapacity == 0U) {
        return Error{ErrorCode::InvalidArgument, "Die LED-Kapazität darf nicht 0 sein"};
    }
    if (config.ledCapacity > mapping::kMaxLedsPerLayout) {
        return Error{ErrorCode::InvalidArgument, "Die LED-Kapazität übersteigt die Obergrenze"};
    }

    std::vector<gpu::BufferHandle> created;  // zum Aufräumen bei einem Fehler
    const auto makeBuffer = [&](std::uint64_t size, gpu::Usage usage,
                                const std::string& suffix) -> Result<gpu::BufferHandle> {
        gpu::BufferDesc desc;
        desc.sizeBytes = size;
        desc.usage = usage;
        desc.label = config.label + suffix;
        Result<gpu::BufferHandle> buffer = device.createBuffer(desc);
        if (buffer.ok()) {
            created.push_back(buffer.value());
        }
        return buffer;
    };
    const auto rollback = [&] {
        for (const gpu::BufferHandle handle : created) {
            device.destroy(handle);
        }
    };

    const std::uint64_t bytes = std::uint64_t{config.ledCapacity} * kBytesPerLed;
    const Result<gpu::BufferHandle> positions =
        makeBuffer(bytes, gpu::Usage::Storage | gpu::Usage::CopyDestination, ".positions");
    if (!positions.ok()) {
        rollback();
        return positions.error();
    }
    const Result<gpu::BufferHandle> colors =
        makeBuffer(bytes, gpu::Usage::Storage | gpu::Usage::CopySource, ".colors");
    if (!colors.ok()) {
        rollback();
        return colors.error();
    }
    const Result<gpu::BufferHandle> info =
        makeBuffer(16, gpu::Usage::Uniform | gpu::Usage::CopyDestination, ".info");
    if (!info.ok()) {
        rollback();
        return info.error();
    }

    Result<std::unique_ptr<FieldPass>> pass = FieldPass::create(
        device, snippets, kernels, config.target, config.label,
        [](std::string_view library) -> Result<std::string> { return samplingShader(library); });
    if (!pass.ok()) {
        rollback();
        return pass.error();
    }

    return std::unique_ptr<SamplingRenderStage>(
        new SamplingRenderStage(device, config.ledCapacity, positions.value(), colors.value(),
                                info.value(), std::move(pass.value())));
}

Result<void> SamplingRenderStage::setMap(const mapping::LedMap& map) {
    if (map.unit.empty()) {
        return Error{ErrorCode::InvalidArgument, "Die LED-Karte ist leer"};
    }
    if (map.unit.size() > capacity_) {
        return Error{ErrorCode::InvalidArgument,
                     "Die LED-Karte hat mehr LEDs als die Kapazität der Stage; "
                     "eine neue Stage mit größerer Kapazität anlegen"};
    }

    if (const Result<void> written = device_.writeBuffer(positions_, 0, packPositions(map.unit));
        !written.ok()) {
        return written;
    }
    const auto count = static_cast<std::uint32_t>(map.unit.size());
    const std::array<std::uint32_t, 4> info{count, 0U, 0U, 0U};
    if (const Result<void> written = device_.writeBuffer(info_, 0, std::as_bytes(std::span{info}));
        !written.ok()) {
        return written;
    }
    ledCount_ = count;
    return {};
}

void SamplingRenderStage::record(const FrameTime& time, gpu::ICommandRecorder& recorder) {
    if (ledCount_ == 0U) {
        ++framesWithoutMap_;
        return;
    }
    const std::array<gpu::Binding, 3> bindings{
        gpu::Binding{kSamplingBindGroup, kSamplingPositionsBinding, positions_},
        gpu::Binding{kSamplingBindGroup, kSamplingColorsBinding, colors_},
        gpu::Binding{kSamplingBindGroup, kSamplingInfoBinding, info_}};
    pass_->record(time, recorder, bindings, samplingDispatch(ledCount_));
}

StageStats SamplingRenderStage::stats() const noexcept {
    StageStats stats = pass_->stats();
    stats.framesSkipped += framesWithoutMap_;
    return stats;
}

}  // namespace whitehole::executor
