#include "whitehole/executor/field_pass.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace whitehole::executor {

FieldPass::FieldPass(gpu::IGpuDevice& device, const compiler::ISnippetLibrary& snippets,
                     const values::ICpuKernelLibrary& kernels, graph::PortRef target,
                     std::string label, EntryFn entry, gpu::BufferHandle frameBuffer)
    : device_{device},
      snippets_{snippets},
      evaluator_{kernels},
      target_{std::move(target)},
      label_{std::move(label)},
      entry_{std::move(entry)},
      frameBuffer_{frameBuffer} {}

FieldPass::~FieldPass() {
    releasePipeline();
    device_.destroy(frameBuffer_);
}

Result<std::unique_ptr<FieldPass>> FieldPass::create(gpu::IGpuDevice& device,
                                                     const compiler::ISnippetLibrary& snippets,
                                                     const values::ICpuKernelLibrary& kernels,
                                                     graph::PortRef target, std::string label,
                                                     EntryFn entry) {
    gpu::BufferDesc bufferDesc;
    bufferDesc.sizeBytes = sizeof(compiler::FrameUniforms);
    bufferDesc.usage = gpu::Usage::Uniform | gpu::Usage::CopyDestination;
    bufferDesc.label = label + ".frame";
    const Result<gpu::BufferHandle> frameBuffer = device.createBuffer(bufferDesc);
    if (!frameBuffer.ok()) {
        return frameBuffer.error();
    }
    return std::unique_ptr<FieldPass>(new FieldPass(device, snippets, kernels, std::move(target),
                                                    std::move(label), std::move(entry),
                                                    frameBuffer.value()));
}

void FieldPass::releasePipeline() noexcept {
    if (pipeline_.has_value()) {
        device_.destroy(*pipeline_);
        pipeline_.reset();
    }
    if (parameterBuffer_.has_value()) {
        device_.destroy(*parameterBuffer_);
        parameterBuffer_.reset();
    }
}

Result<void> FieldPass::rebuildPipeline(const compiler::CompiledField& compiled) {
    Result<std::string> shader = entry_(compiled.wgsl);
    if (!shader.ok()) {
        return shader.error();
    }

    gpu::ComputePipelineDesc pipelineDesc;
    pipelineDesc.shader.language = gpu::ShaderLanguage::Wgsl;
    pipelineDesc.shader.code = std::move(shader.value());
    pipelineDesc.shader.entryPoint = "main";
    pipelineDesc.label = label_ + ".pipeline";
    const Result<gpu::ComputePipelineHandle> pipeline = device_.createComputePipeline(pipelineDesc);
    if (!pipeline.ok()) {
        return pipeline.error();
    }

    // Der Parameterpuffer wird nur ersetzt, wenn sich seine Größe ändert.
    std::optional<gpu::BufferHandle> newBuffer;
    if (!parameterBuffer_.has_value() || compiled.layout.sizeBytes != layout_.sizeBytes) {
        gpu::BufferDesc bufferDesc;
        bufferDesc.sizeBytes = compiled.layout.sizeBytes;
        bufferDesc.usage = gpu::Usage::Uniform | gpu::Usage::CopyDestination;
        bufferDesc.label = label_ + ".params";
        const Result<gpu::BufferHandle> buffer = device_.createBuffer(bufferDesc);
        if (!buffer.ok()) {
            device_.destroy(pipeline.value());
            return buffer.error();
        }
        newBuffer = buffer.value();
    }

    // Erst jetzt, da alles Neue steht, wird das Alte freigegeben. Das Backend hält
    // Ressourcen, die ein eingereichter Frame noch nutzt, bis dieser fertig ist.
    if (pipeline_.has_value()) {
        device_.destroy(*pipeline_);
    }
    pipeline_ = pipeline.value();
    if (newBuffer.has_value()) {
        if (parameterBuffer_.has_value()) {
            device_.destroy(*parameterBuffer_);
        }
        parameterBuffer_ = newBuffer;
    }
    ++stats_.pipelinesBuilt;
    return {};
}

Result<void> FieldPass::setGraph(const graph::Graph& graph) {
    const auto fail = [this](const Error& error) -> Result<void> {
        graphError_ = error;
        return error;
    };

    Result<compiler::CompiledField> compiled = compiler::compileField(graph, snippets_, target_);
    if (!compiled.ok()) {
        return fail(compiled.error());
    }

    if (!pipeline_.has_value() || compiled.value().structureHash != structureHash_) {
        if (const Result<void> built = rebuildPipeline(compiled.value()); !built.ok()) {
            return fail(built.error());
        }
        structureHash_ = compiled.value().structureHash;
    }

    layout_ = compiled.value().layout;
    graph_ = graph;
    graphError_.reset();
    return {};
}

bool FieldPass::record(const FrameTime& time, gpu::ICommandRecorder& recorder,
                       std::span<const gpu::Binding> consumerBindings, const DispatchSize& size) {
    frameError_.reset();
    if (!ready()) {
        ++stats_.framesSkipped;
        return false;
    }

    // 1. CPU-Nodes. Ein Fehler hier lässt den Frame mit den gespeicherten Werten weiterlaufen.
    if (const Result<void> evaluated = evaluator_.evaluate(*graph_, time, store_); !evaluated.ok()) {
        frameError_ = evaluated.error();
    }

    // 2. Parameterpuffer
    const std::vector<std::byte> parameters = compiler::packParameters(*graph_, layout_, &store_);
    if (const Result<void> written = device_.writeBuffer(*parameterBuffer_, 0, parameters); !written.ok()) {
        frameError_ = written.error();
        ++stats_.framesSkipped;
        return false;
    }

    // 3. Frame-Daten
    const compiler::FrameUniforms frame{static_cast<float>(time.time.count()),
                                        static_cast<float>(time.delta.count()),
                                        static_cast<std::uint32_t>(time.frame), 0U};
    if (const Result<void> written =
            device_.writeBuffer(frameBuffer_, 0, std::as_bytes(std::span{&frame, std::size_t{1}}));
        !written.ok()) {
        frameError_ = written.error();
        ++stats_.framesSkipped;
        return false;
    }

    // 4. Dispatch: Gruppe 0 gehört der Feldbibliothek, der Rest dem Konsumenten.
    bindings_.clear();
    bindings_.push_back(gpu::Binding{compiler::kFieldBindGroup, compiler::kFrameBinding, frameBuffer_});
    bindings_.push_back(
        gpu::Binding{compiler::kFieldBindGroup, compiler::kParametersBinding, *parameterBuffer_});
    bindings_.insert(bindings_.end(), consumerBindings.begin(), consumerBindings.end());

    gpu::DispatchDesc dispatch;
    dispatch.pipeline = *pipeline_;
    dispatch.bindings = bindings_;
    dispatch.groupsX = size.x;
    dispatch.groupsY = size.y;
    dispatch.groupsZ = size.z;
    recorder.dispatch(dispatch);
    ++stats_.framesRecorded;
    return true;
}

}  // namespace whitehole::executor
