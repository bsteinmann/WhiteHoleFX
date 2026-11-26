// field_pass.cpp: Umsetzung zu field_pass.hpp. Der gemeinsame Ablauf aller Pässe, die ein Feld auswerten.
#include "whitehole/executor/field_pass.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "whitehole/graph/output_merge.hpp"

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
    cancelPending();
    cancelPrefetches();
    for (const Cached& cached : cache_) {
        device_.destroy(cached.pipeline);
    }
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

static gpu::ComputePipelineDesc FieldPassDesc(std::string shader, const std::string& label) {
    gpu::ComputePipelineDesc pipelineDesc;
    pipelineDesc.shader.language = gpu::ShaderLanguage::Wgsl;
    pipelineDesc.shader.code = std::move(shader);
    pipelineDesc.shader.entryPoint = "main";
    pipelineDesc.label = label + ".pipeline";
    return pipelineDesc;
}

Result<void> FieldPass::rebuildPipeline(const compiler::CompiledField& compiled) {
    Result<std::string> shader = entry_(compiled.wgsl);
    if (!shader.ok()) {
        return shader.error();
    }
    const Result<gpu::ComputePipelineHandle> pipeline =
        device_.createComputePipeline(FieldPassDesc(std::move(shader.value()), label_));
    if (!pipeline.ok()) {
        return pipeline.error();
    }
    return installPipeline(pipeline.value(), compiled, true);
}

Result<void> FieldPass::installPipeline(gpu::ComputePipelineHandle pipeline, const compiler::CompiledField& compiled,
                                        bool fresh) {
    // Der Parameterpuffer wird nur ersetzt, wenn sich seine Größe ändert.
    std::optional<gpu::BufferHandle> newBuffer;
    if (!parameterBuffer_.has_value() || compiled.layout.sizeBytes != layout_.sizeBytes) {
        gpu::BufferDesc bufferDesc;
        bufferDesc.sizeBytes = compiled.layout.sizeBytes;
        bufferDesc.usage = gpu::Usage::Uniform | gpu::Usage::CopyDestination;
        bufferDesc.label = label_ + ".params";
        const Result<gpu::BufferHandle> buffer = device_.createBuffer(bufferDesc);
        if (!buffer.ok()) {
            device_.destroy(pipeline);
            return buffer.error();
        }
        newBuffer = buffer.value();
    }

    // Erst jetzt, da alles Neue steht, wird das Alte freigegeben (oder aufgehoben). Das Backend hält
    // Ressourcen, die ein eingereichter Frame noch nutzt, bis dieser fertig ist.
    if (pipeline_.has_value()) {
        if (background_) {
            remember(structureHash_, *pipeline_);
        } else {
            device_.destroy(*pipeline_);
        }
    }
    pipeline_ = pipeline;
    if (newBuffer.has_value()) {
        if (parameterBuffer_.has_value()) {
            device_.destroy(*parameterBuffer_);
        }
        parameterBuffer_ = newBuffer;
    }
    if (fresh) {
        ++stats_.pipelinesBuilt;
    }
    return {};
}

std::optional<gpu::ComputePipelineHandle> FieldPass::takeCached(std::uint64_t hash) noexcept {
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->hash == hash) {
            const gpu::ComputePipelineHandle found = it->pipeline;
            cache_.erase(it);
            return found;
        }
    }
    return std::nullopt;
}

void FieldPass::remember(std::uint64_t hash, gpu::ComputePipelineHandle pipeline) noexcept {
    for (const Cached& cached : cache_) {
        if (cached.hash == hash) {
            device_.destroy(pipeline);  // schon da (z. B. vorab übersetzt und zugleich benutzt)
            return;
        }
    }
    cache_.push_back(Cached{hash, pipeline});
    if (cache_.size() > kCacheSize) {
        device_.destroy(cache_.front().pipeline);
        cache_.erase(cache_.begin());
    }
}

bool FieldPass::known(std::uint64_t hash) const noexcept {
    if (pipeline_.has_value() && hash == structureHash_) {
        return true;
    }
    if (pending_ && pending_->compiled.structureHash == hash) {
        return true;
    }
    const auto same = [hash](const auto& entry) { return entry.hash == hash; };
    return std::ranges::any_of(cache_, same) || std::ranges::any_of(prefetches_, same);
}

Result<FieldPass::Compiled> FieldPass::compile(const graph::Graph& graph, std::span<const graph::PortRef> extras) const {
    // Mehrere Output-Nodes werden hier unsichtbar über Add-Nodes zusammengefasst (siehe output_merge.hpp).
    std::optional<graph::MergedOutputs> merged = graph::mergeOutputs(graph, target_);
    const graph::PortRef& effectiveTarget = merged ? merged->target : target_;
    Result<compiler::CompiledField> compiled =
        compiler::compileField(merged ? merged->graph : graph, snippets_, effectiveTarget, extras);
    if (!compiled.ok()) {
        return compiled.error();
    }
    return Compiled{std::move(compiled.value()), merged ? std::move(merged->graph) : graph};
}

Result<void> FieldPass::prefetch(const graph::Graph& graph, std::span<const graph::PortRef> extras) {
    if (!background_ || !pipeline_.has_value()) {
        return {};
    }
    Result<Compiled> compiled = compile(graph, extras);
    if (!compiled.ok()) {
        return compiled.error();
    }
    const std::uint64_t hash = compiled.value().field.structureHash;
    if (known(hash)) {
        return {};
    }
    Result<std::string> shader = entry_(compiled.value().field.wgsl);
    if (!shader.ok()) {
        return shader.error();
    }
    Result<gpu::PipelineTicket> ticket = device_.prefetchComputePipeline(FieldPassDesc(std::move(shader.value()), label_));
    if (!ticket.ok()) {
        return ticket.error();
    }
    prefetches_.push_back(Prefetch{hash, ticket.value()});
    pollPrefetches();  // Geräte ohne Hintergrund sind sofort fertig
    return {};
}

void FieldPass::cancelPrefetches() noexcept {
    for (const Prefetch& open : prefetches_) {
        device_.cancelComputePipeline(open.ticket);
    }
    prefetches_.clear();
}

void FieldPass::pollPrefetches() {
    for (auto it = prefetches_.begin(); it != prefetches_.end();) {
        std::optional<Result<gpu::ComputePipelineHandle>> done = device_.takeComputePipeline(it->ticket);
        if (!done) {
            ++it;
            continue;
        }
        if (done->ok()) {
            remember(it->hash, done->value());
        }
        // Ein Fehler hier ist kein Fehler des laufenden Bilds; kommt die Struktur wirklich, meldet setGraph ihn
        it = prefetches_.erase(it);
    }
}

void FieldPass::cancelPending() noexcept {
    if (pending_) {
        device_.cancelComputePipeline(pending_->ticket);
        pending_.reset();
    }
}

void FieldPass::pollPending() {
    if (!pending_) {
        return;
    }
    std::optional<Result<gpu::ComputePipelineHandle>> done = device_.takeComputePipeline(pending_->ticket);
    if (!done) {
        return;  // der Treiber übersetzt noch: der bisherige Zustand rendert weiter
    }
    Pending finished = std::move(*pending_);
    pending_.reset();
    if (!done->ok()) {
        graphError_ = done->error();  // der letzte gute Zustand läuft weiter
        return;
    }
    if (const Result<void> installed = installPipeline(done->value(), finished.compiled, true); !installed.ok()) {
        graphError_ = installed.error();
        return;
    }
    structureHash_ = finished.compiled.structureHash;
    layout_ = finished.compiled.layout;
    phases_.retain(layout_);
    graph_ = std::move(finished.graph);
    graphError_.reset();
}

Result<void> FieldPass::setGraph(const graph::Graph& graph, std::span<const graph::PortRef> extras) {
    const auto fail = [this](const Error& error) -> Result<void> {
        graphError_ = error;
        return error;
    };

    Result<Compiled> compiled = compile(graph, extras);
    if (!compiled.ok()) {
        return fail(compiled.error());
    }
    compiler::CompiledField& field = compiled.value().field;
    const std::uint64_t hash = field.structureHash;
    if (pipeline_.has_value() && hash == structureHash_) {
        cancelPending();  // zurück zur laufenden Struktur: die wartende wird nicht mehr gebraucht
    } else if (pending_ && pending_->compiled.structureHash == hash) {
        pending_->compiled = std::move(field);  // dieselbe Struktur wartet schon: nur Werte nachziehen
        pending_->graph = std::move(compiled.value().graph);
        return {};
    } else if (background_ && pipeline_.has_value()) {
        cancelPending();
        if (const std::optional<gpu::ComputePipelineHandle> cached = takeCached(hash)) {
            if (const Result<void> installed = installPipeline(*cached, field, false); !installed.ok()) {
                return fail(installed.error());
            }
        } else {
            std::optional<gpu::PipelineTicket> ticket;
            const auto ordered = std::ranges::find_if(prefetches_, [hash](const Prefetch& open) { return open.hash == hash; });
            if (ordered != prefetches_.end()) {
                ticket = ordered->ticket;  // schon vorab bestellt: vorziehen statt ein zweites Mal übersetzen
                prefetches_.erase(ordered);
                device_.promoteComputePipeline(*ticket);
            } else {
                Result<std::string> shader = entry_(field.wgsl);
                if (!shader.ok()) {
                    return fail(shader.error());
                }
                Result<gpu::PipelineTicket> begun = device_.beginComputePipeline(FieldPassDesc(std::move(shader.value()), label_));
                if (!begun.ok()) {
                    return fail(begun.error());
                }
                ticket = begun.value();
            }
            pending_.emplace(Pending{*ticket, std::move(field), std::move(compiled.value().graph)});
            graphError_.reset();
            pollPending();  // Geräte ohne Hintergrund sind sofort fertig
            return {};
        }
    } else {
        cancelPending();
        if (const Result<void> built = rebuildPipeline(field); !built.ok()) {
            return fail(built.error());
        }
    }
    structureHash_ = hash;
    layout_ = field.layout;
    phases_.retain(layout_);
    graph_ = std::move(compiled.value().graph);
    graphError_.reset();
    return {};
}

Result<void> FieldPass::setParameters(const graph::Graph& graph) {
    if (!ready()) {
        return setGraph(graph);
    }
    if (pending_) {
        // Die neuen Werte gehören zur wartenden Struktur; bis sie fertig ist, läuft der bisherige Graph mit seinen Werten
        if (std::optional<graph::MergedOutputs> merged = graph::mergeOutputs(graph, target_)) {
            pending_->graph = std::move(merged->graph);
        } else {
            pending_->graph = graph;
        }
        return {};
    }
    // Die Parameter hängen an den Node-Nummern; die Adder von setGraph müssen also auch hier wieder dazukommen.
    if (std::optional<graph::MergedOutputs> merged = graph::mergeOutputs(graph, target_)) {
        graph_ = std::move(merged->graph);
    } else {
        graph_ = graph;
    }
    return {};
}

bool FieldPass::record(const FrameTime& time, gpu::ICommandRecorder& recorder,
                       std::span<const gpu::Binding> consumerBindings, const DispatchSize& size) {
    frameError_.reset();
    pollPending();
    pollPrefetches();
    if (!ready()) {
        ++stats_.framesSkipped;
        return false;
    }

    // 1. CPU-Nodes. Ein Fehler hier lässt den Frame mit den gespeicherten Werten weiterlaufen.
    if (const Result<void> evaluated = evaluator_.evaluate(*graph_, time, store_); !evaluated.ok()) {
        frameError_ = evaluated.error();
    }

    // 2. Parameterpuffer. Die Phase-Slots (@phase) laufen mit dem Verfolger weiter, wenn ein Regler verstellt wird.
    const std::vector<std::byte> parameters = compiler::packParameters(
        *graph_, layout_, &store_, compiler::PhaseSource{&phases_, time.time.count()});
    if (const Result<void> written = device_.writeBuffer(*parameterBuffer_, 0, parameters); !written.ok()) {
        frameError_ = written.error();
        ++stats_.framesSkipped;
        return false;
    }

    // 3. Frame-Daten
    const compiler::FrameUniforms frame{static_cast<float>(time.time.count()),
                                        static_cast<float>(time.delta.count()),
                                        static_cast<std::uint32_t>(time.frame), volume_ ? 1U : 0U,
                                        aspect_, aspectZ_, columns_, rows_};
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
