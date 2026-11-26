// webgpu_pipeline_async.cpp: Compute-Pipelines im Hintergrund übersetzen (Teil von WebGpuDevice).
//
// Warum es diese Datei gibt: Der Treiber braucht für einen großen Shader bis zu Hunderte Millisekunden (Vulkan: SPIR-V
// in Maschinencode, D3D12: HLSL über DXC). Im Live-Betrieb entsteht bei jedem Vorladen eines Looks ein neuer Shader aus
// beiden Decks; synchron übersetzt stand dabei die ganze Ausgabe still (Rückmeldung vom 25.11.: etwa 0,3 s). Hier
// prüft der Engine-Thread nur den WGSL-Text (billig, Fehler sofort), das Übersetzen läuft auf einem eigenen Thread, und
// der Aufrufer rendert solange mit der alten Pipeline weiter.
//
// Ein einziger Übersetzer-Thread arbeitet eine Warteschlange ab (CompileQueue in webgpu_impl.hpp): dringende Aufträge
// (beginComputePipeline, ein Look wird gerade gebraucht) vor vorab bestellten (prefetchComputePipeline, die
// Flash-Kombinationen des laufenden Looks, Rückmeldung vom 26.11.). Ein verworfener Auftrag, der noch wartet, kostet
// nichts mehr.
//
// Threading: wgpu-Geräte sind thread-sicher; der Thread ruft nur wgpuDeviceCreateComputePipeline und die
// Error-Scope-Funktionen. Alle Tabellen (pipelines, compiling) gehören dem Engine-Thread. Fehler fängt ein eigener
// Error-Scope des Threads (wgpu-native ruft den Rückruf von PopErrorScope sofort auf). Hinweis: Der Scope gilt für das
// ganze Gerät; ein Fehler eines anderen Aufrufs in genau diesem Zeitfenster landet hier statt im allgemeinen Fehlerweg.
// Darum übersetzt immer nur ein Auftrag zur Zeit.
#include <algorithm>
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

/// Der Übersetzer-Thread: nimmt den vordersten Auftrag, übersetzt ihn, meldet ihn fertig, bis stopping.
void runCompiler(CompileQueue& queue, WGPUDevice device) {
    for (;;) {
        std::shared_ptr<CompilingPipeline> job;
        {
            std::unique_lock<std::mutex> lock{queue.mutex};
            queue.wake.wait(lock, [&queue] { return queue.stopping || !queue.waiting.empty(); });
            if (queue.stopping) {
                return;
            }
            job = std::move(queue.waiting.front());
            queue.waiting.pop_front();
            job->started = true;
        }
        wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
        WGPUComputePipeline pipeline = compilePrepared(device, job->prepared);
        std::string error;
        WGPUPopErrorScopeCallbackInfo info{};
        info.mode = WGPUCallbackMode_AllowSpontaneous;
        info.callback = onPopErrorScope;
        info.userdata1 = &error;
        (void)wgpuDevicePopErrorScope(device, info);
        const std::lock_guard<std::mutex> lock{queue.mutex};
        job->pipeline = pipeline;
        job->error = std::move(error);
        job->done = true;
    }
}

/// Reiht einen wartenden Auftrag ein: dringende hinter die schon wartenden dringenden, vor alle vorab bestellten.
/// Aufrufer hält queue.mutex.
void place(CompileQueue& queue, std::shared_ptr<CompilingPipeline> job) {
    if (!job->urgent) {
        queue.waiting.push_back(std::move(job));
        return;
    }
    const auto firstLater = std::ranges::find_if(queue.waiting, [](const auto& waiting) { return !waiting->urgent; });
    queue.waiting.insert(firstLater, std::move(job));
}

bool isDone(CompileQueue& queue, const CompilingPipeline& state) {
    const std::lock_guard<std::mutex> lock{queue.mutex};
    return state.done;
}

}  // namespace

Result<PipelineTicket> WebGpuDevice::Impl::beginCompile(const ComputePipelineDesc& desc, bool urgent) {
    Result<PreparedPipeline> prepared = preparePipeline(*this, desc);
    if (!prepared.ok()) {
        return prepared.error();
    }
    pump(false);
    if (const std::vector<std::string> found = drainErrors(); !found.empty() || prepared.value().module == nullptr) {
        releasePrepared(prepared.value(), false);
        std::string text = "Der Shader ist ungültig";
        for (const std::string& e : found) {
            text += ": " + e;
        }
        return Error{ErrorCode::ShaderError, text};
    }
    auto state = std::make_shared<CompilingPipeline>();
    state->prepared = std::move(prepared.value());
    state->urgent = urgent;
    {
        const std::lock_guard<std::mutex> lock{compileQueue.mutex};
        place(compileQueue, state);
        if (!compileQueue.worker.joinable()) {
            compileQueue.worker = std::thread{runCompiler, std::ref(compileQueue), device};
        }
    }
    compileQueue.wake.notify_one();
    const PipelineTicket ticket{++nextTicket};
    compiling.emplace(ticket.value(), std::move(state));
    return ticket;
}

Result<PipelineTicket> WebGpuDevice::beginComputePipeline(const ComputePipelineDesc& desc) {
    return impl_->beginCompile(desc, true);
}

Result<PipelineTicket> WebGpuDevice::prefetchComputePipeline(const ComputePipelineDesc& desc) {
    return impl_->beginCompile(desc, false);
}

void WebGpuDevice::promoteComputePipeline(PipelineTicket ticket) noexcept {
    Impl& impl = *impl_;
    const auto it = impl.compiling.find(ticket.value());
    if (it == impl.compiling.end() || it->second->urgent) {
        return;
    }
    const std::lock_guard<std::mutex> lock{impl.compileQueue.mutex};
    std::shared_ptr<CompilingPipeline> job = it->second;
    job->urgent = true;
    if (job->started) {
        return;  // läuft schon
    }
    std::erase(impl.compileQueue.waiting, job);
    place(impl.compileQueue, std::move(job));
}

std::optional<Result<ComputePipelineHandle>> WebGpuDevice::takeComputePipeline(PipelineTicket ticket) {
    Impl& impl = *impl_;
    const auto it = impl.compiling.find(ticket.value());
    if (it == impl.compiling.end()) {
        return Result<ComputePipelineHandle>{Error{ErrorCode::NotFound, "Unbekannte Pipeline-Marke"}};
    }
    CompilingPipeline& state = *it->second;
    if (!isDone(impl.compileQueue, state)) {
        return std::nullopt;
    }
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
    std::shared_ptr<CompilingPipeline> job = std::move(it->second);
    impl.compiling.erase(it);
    bool waiting = false;
    {
        const std::lock_guard<std::mutex> lock{impl.compileQueue.mutex};
        if (!job->started) {
            std::erase(impl.compileQueue.waiting, job);
            waiting = true;
        }
    }
    if (waiting) {
        releaseCompiled(*job);  // nie übersetzt: nur Modul und Layouts freigeben
        return;
    }
    impl.abandoned.push_back(std::move(job));  // nicht warten: poll() räumt auf, sobald der Thread fertig ist
    impl.reapAbandoned();
}

void WebGpuDevice::Impl::reapAbandoned() noexcept {
    for (auto it = abandoned.begin(); it != abandoned.end();) {
        if (isDone(compileQueue, **it)) {
            releaseCompiled(**it);
            it = abandoned.erase(it);
        } else {
            ++it;
        }
    }
}

void WebGpuDevice::Impl::joinCompiling() noexcept {
    {
        const std::lock_guard<std::mutex> lock{compileQueue.mutex};
        compileQueue.stopping = true;
        compileQueue.waiting.clear();  // die Aufträge selbst hängen noch an `compiling`
    }
    compileQueue.wake.notify_all();
    if (compileQueue.worker.joinable()) {
        compileQueue.worker.join();  // ein laufendes Übersetzen geht noch zu Ende
    }
    for (auto& [id, state] : compiling) {
        releaseCompiled(*state);
    }
    compiling.clear();
    for (const std::shared_ptr<CompilingPipeline>& state : abandoned) {
        releaseCompiled(*state);
    }
    abandoned.clear();
}

}  // namespace whitehole::gpu
