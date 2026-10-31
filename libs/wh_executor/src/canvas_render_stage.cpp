// canvas_render_stage.cpp: Umsetzung zu canvas_render_stage.hpp. Rastert eine Field-Kette pro Frame in eine Canvas-Textur.
#include "whitehole/executor/canvas_render_stage.hpp"

#include <array>
#include <string_view>
#include <utility>

#include "whitehole/executor/canvas_shader.hpp"

namespace whitehole::executor {

CanvasRenderStage::CanvasRenderStage(gpu::IGpuDevice& device, CanvasStageConfig config,
                                     gpu::TextureHandle texture, std::unique_ptr<FieldPass> pass)
    : device_{device}, config_{std::move(config)}, texture_{texture}, pass_{std::move(pass)} {}

CanvasRenderStage::~CanvasRenderStage() {
    pass_.reset();  // Pipeline und Puffer zuerst, dann die Textur
    device_.destroy(texture_);
}

Result<std::unique_ptr<CanvasRenderStage>> CanvasRenderStage::create(
    gpu::IGpuDevice& device, const compiler::ISnippetLibrary& snippets,
    const values::ICpuKernelLibrary& kernels, CanvasStageConfig config) {
    if (config.extent.width == 0 || config.extent.height == 0) {
        return Error{ErrorCode::InvalidArgument, "Die Canvas-Größe darf nicht 0 sein"};
    }
    if (config.extent.depth != 1) {
        return Error{ErrorCode::Unsupported,
                     "Der Canvas-Pass rastert nur 2D; Cubes und Volumen übernimmt das Mapping"};
    }
    if (const Result<std::string> probe = canvasShader({}, config.format); !probe.ok()) {
        return probe.error();
    }

    gpu::TextureDesc textureDesc;
    textureDesc.extent = config.extent;
    textureDesc.format = config.format;
    textureDesc.usage = gpu::Usage::Storage | gpu::Usage::Sampled | gpu::Usage::CopySource;
    textureDesc.label = config.label;
    const Result<gpu::TextureHandle> texture = device.createTexture(textureDesc);
    if (!texture.ok()) {
        return texture.error();
    }

    const gpu::TextureFormat format = config.format;
    Result<std::unique_ptr<FieldPass>> pass = FieldPass::create(
        device, snippets, kernels, config.target, config.label,
        [format](std::string_view library) { return canvasShader(library, format); });
    if (!pass.ok()) {
        device.destroy(texture.value());
        return pass.error();
    }

    return std::unique_ptr<CanvasRenderStage>(new CanvasRenderStage(
        device, std::move(config), texture.value(), std::move(pass.value())));
}

void CanvasRenderStage::record(const FrameTime& time, gpu::ICommandRecorder& recorder) {
    const std::array<gpu::Binding, 1> bindings{
        gpu::Binding{kCanvasBindGroup, kCanvasTextureBinding, texture_}};
    const DispatchSize size{(config_.extent.width + kCanvasWorkgroupSize - 1U) / kCanvasWorkgroupSize,
                            (config_.extent.height + kCanvasWorkgroupSize - 1U) / kCanvasWorkgroupSize,
                            1U};
    pass_->record(time, recorder, bindings, size);
}

}  // namespace whitehole::executor
