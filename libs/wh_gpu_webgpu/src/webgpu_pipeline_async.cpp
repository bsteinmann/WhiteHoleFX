// webgpu_pipeline_async.cpp: Compute-Pipelines im Hintergrund übersetzen (Teil von WebGpuDevice).
//
// Warum es diese Datei gibt: Der Treiber braucht für einen großen Shader bis zu Hunderte Millisekunden (Vulkan: SPIR-V
// in Maschinencode, D3D12: HLSL über DXC). Im Live-Betrieb entsteht bei jedem Vorladen eines Looks ein neuer Shader aus
// beiden Decks; synchron übersetzt stand dabei die ganze Ausgabe still (Rückmeldung vom 25.11.: etwa 0,3 s). Hier
// prüft der Engine-Thread nur den WGSL-Text (billig, Fehler sofort), das Übersetzen läuft auf einem eigenen Thread, und
// der Aufrufer rendert solange mit der alten Pipeline weiter.
//
// Threading: wgpu-Geräte sind thread-sicher; der Arbeits-Thread ruft nur wgpuDeviceCreateComputePipeline und die
// Error-Scope-Funktionen. Alle Tabellen (pipelines, compiling) gehören dem Engine-Thread. Fehler fängt ein eigener
// Error-Scope des Threads (wgpu-native ruft den Rückruf von PopErrorScope sofort auf). Hinweis: Der Scope gilt für das
// ganze Gerät; ein Fehler eines anderen Aufrufs in genau diesem Zeitfenster landet hier statt im allgemeinen Fehlerweg.
#include <utility>

#include "webgpu_impl.hpp"

namespace whitehole::gpu {

namespace {

void onPopErrorScope(WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView message, void* userdata1,
                     void* /*userdata2*/) {
    auto* error = static_cast<std::string*>(userdata1);
    if (status == WGPUPopErrorScopeStatus_Success && type != WGPUErrorType_NoError) {
        *error = toString(message);
        if (error->empty()) {
            *error = "Fehler beim Anlegen der Pipeline";
        }
    }
}

void releaseCompiled(CompilingPipeline& state) noexcept {
    if (state.pipeline != nullptr) {
        wgpuComputePipelineRelease(state.pipeline);
        state.pipeline = nullptr;
    }
    releasePrepared(state.prepared, false);
}

bool isDone(CompilingPipeline& state) {
    const std::lock_guard<std::mutex> lock{state.mutex};
    return state.done;
}

}  // namespace

Result<PipelineTicket> WebGpuDevice::beginComputePipeline(const ComputePipelineDesc& desc) {
    Impl& impl = *impl_;
    Result<PreparedPipeline> prepared = preparePipeline(impl, desc);
    if (!prepared.ok()) {
        return prepared.error();
    }
    impl.pump(false);
    if (const std::vector<std::string> errors = impl.drainErrors(); !errors.empty() || prepared.value().module == nullptr) {
        releasePrepared(prepared.value(), false);
        std::string text = "Der Shader ist ungültig";
        for (const std::string& e : errors) {
            text += ": " + e;
        }
        return Error{ErrorCode::ShaderError, text};
    }
    auto state = std::make_shared<CompilingPipeline>();
    state->prepared = std::move(prepared.value());
    WGPUDevice device = impl.device;
    state->worker = std::thread{[state, device]() {
        wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
        WGPUComputePipeline pipeline = compilePrepared(device, state->prepared);
        std::string error;
        WGPUPopErrorScopeCallbackInfo info{};
        info.mode = WGPUCallbackMode_AllowSpontaneous;
        info.callback = onPopErrorScope;
        info.userdata1 = &error;
        (void)wgpuDevicePopErrorScope(device, info);
        const std::lock_guard<std::mutex> lock{state->mutex};
        state->pipeline = pipeline;
        state->error = std::move(error);
        state->done = true;
    }};
    const PipelineTicket ticket{++impl.nextTicket};
    impl.compiling.emplace(ticket.value(), std::move(state));
    return ticket;
}

std::optional<Result<ComputePipelineHandle>> WebGpuDevice::takeComputePipeline(PipelineTicket ticket) {
    Impl& impl = *impl_;
    const auto it = impl.compiling.find(ticket.value());
    if (it == impl.compiling.end()) {
        return Result<ComputePipelineHandle>{Error{ErrorCode::NotFound, "Unbekannte Pipeline-Marke"}};
    }
    CompilingPipeline& state = *it->second;
    if (!isDone(state)) {
        return std::nullopt;
    }
    state.worker.join();
    std::shared_ptr<CompilingPipeline> owned = std::move(it->second);
    impl.compiling.erase(it);
    releasePrepared(state.prepared, true);  // Modul und Pipeline-Layout braucht die fertige Pipeline nicht mehr
    if (state.pipeline == nullptr || !state.error.empty()) {
        const std::string message = state.error.empty() ? std::string{"Die Compute-Pipeline ließ sich nicht anlegen"}
                                                        : "Die Compute-Pipeline ließ sich nicht anlegen: " + state.error;
        releaseCompiled(state);
        return Result<ComputePipelineHandle>{Error{ErrorCode::ShaderError, message}};
    }
    const ComputePipelineHandle handle = registerPipeline(impl, state.pipeline, state.prepared);
    state.pipeline = nullptr;
    return Result<ComputePipelineHandle>{handle};
}

void WebGpuDevice::cancelComputePipeline(PipelineTicket ticket) noexcept {
    Impl& impl = *impl_;
    const auto it = impl.compiling.find(ticket.value());
    if (it == impl.compiling.end()) {
        return;
    }
    impl.abandoned.push_back(std::move(it->second));  // nicht warten: poll() räumt auf, sobald der Thread fertig ist
    impl.compiling.erase(it);
    impl.reapAbandoned();
}

void WebGpuDevice::Impl::reapAbandoned() noexcept {
    for (auto it = abandoned.begin(); it != abandoned.end();) {
        if (isDone(**it)) {
            (*it)->worker.join();
            releaseCompiled(**it);
            it = abandoned.erase(it);
        } else {
            ++it;
        }
    }
}

void WebGpuDevice::Impl::joinCompiling() noexcept {
    for (auto& [id, state] : compiling) {
        abandoned.push_back(std::move(state));
    }
    compiling.clear();
    for (const std::shared_ptr<CompilingPipeline>& state : abandoned) {
        if (state->worker.joinable()) {
            state->worker.join();
        }
        releaseCompiled(*state);
    }
    abandoned.clear();
}

}  // namespace whitehole::gpu
