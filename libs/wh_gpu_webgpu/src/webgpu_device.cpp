// webgpu_device.cpp: Umsetzung zu webgpu_device.hpp. IGpuDevice auf einer echten GPU (oder einem Software-Rasterizer).
#include "whitehole/gpu_webgpu/webgpu_device.hpp"

#include <chrono>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "webgpu_impl.hpp"

namespace whitehole::gpu {

namespace {

constexpr auto kRequestTimeout = std::chrono::seconds(10);

struct AdapterRequest {
    bool done{false};
    WGPURequestAdapterStatus status{WGPURequestAdapterStatus_Error};
    WGPUAdapter adapter{nullptr};
    std::string message;
};

struct DeviceRequest {
    bool done{false};
    WGPURequestDeviceStatus status{WGPURequestDeviceStatus_Error};
    WGPUDevice device{nullptr};
    std::string message;
};

void onAdapter(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* user,
               void*) {
    auto* request = static_cast<AdapterRequest*>(user);
    request->status = status;
    request->adapter = adapter;
    request->message = toString(message);
    request->done = true;
}

void onDevice(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* user, void*) {
    auto* request = static_cast<DeviceRequest*>(user);
    request->status = status;
    request->device = device;
    request->message = toString(message);
    request->done = true;
}

void onUncapturedError(WGPUDevice const*, WGPUErrorType type, WGPUStringView message, void* user, void*) {
    static_cast<WebGpuDevice::Impl*>(user)->pushError("GPU-Fehler (Typ " + std::to_string(static_cast<int>(type)) +
                                                      "): " + toString(message));
}

void onDeviceLost(WGPUDevice const*, WGPUDeviceLostReason reason, WGPUStringView message, void* user, void*) {
    if (reason == WGPUDeviceLostReason_Destroyed) {
        return;  // normales Aufräumen
    }
    static_cast<WebGpuDevice::Impl*>(user)->pushError("GPU-Gerät verloren: " + toString(message));
}

template <typename Request>
bool waitFor(WebGpuDevice::Impl& impl, const Request& request) {
    const auto deadline = std::chrono::steady_clock::now() + kRequestTimeout;
    while (!request.done) {
        if (std::chrono::steady_clock::now() > deadline) {
            return false;
        }
        wgpuInstanceProcessEvents(impl.instance);
        if (!request.done) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    return true;
}

}  // namespace

WebGpuDevice::Impl::~Impl() {
    for (auto& [id, entry] : pipelines) {
        for (WGPUBindGroupLayout layout : entry.layouts) {
            wgpuBindGroupLayoutRelease(layout);
        }
        wgpuComputePipelineRelease(entry.pipeline);
    }
    for (auto& [id, entry] : textures) {
        wgpuTextureViewRelease(entry.view);
        wgpuTextureDestroy(entry.texture);
        wgpuTextureRelease(entry.texture);
    }
    for (auto& [id, entry] : buffers) {
        wgpuBufferDestroy(entry.buffer);
        wgpuBufferRelease(entry.buffer);
    }
    if (queue != nullptr) wgpuQueueRelease(queue);
    if (device != nullptr) wgpuDeviceRelease(device);
    if (adapter != nullptr) wgpuAdapterRelease(adapter);
    if (instance != nullptr) wgpuInstanceRelease(instance);
    // Erst jetzt aufräumen: Beim Freigeben der Puffer und der Instanz können noch
    // Map-Callbacks mit "abgebrochen" eintreffen, die auf diese Zustände zeigen.
    for (ReadbackState* state : readbacks) {
        if (state->buffer != nullptr) {
            wgpuBufferRelease(state->buffer);
        }
        delete state;
    }
}

void WebGpuDevice::Impl::pushError(std::string message) {
    const std::lock_guard<std::mutex> lock{errorMutex};
    errors.push_back(std::move(message));
}

std::vector<std::string> WebGpuDevice::Impl::drainErrors() {
    const std::lock_guard<std::mutex> lock{errorMutex};
    return std::exchange(errors, {});
}

void WebGpuDevice::Impl::pump(bool wait) {
    if (device != nullptr) {
        wgpuDevicePoll(device, wait ? 1 : 0, nullptr);
    }
    if (instance != nullptr) {
        wgpuInstanceProcessEvents(instance);
    }
}

WebGpuDevice::WebGpuDevice(std::unique_ptr<Impl> impl) : impl_{std::move(impl)} {}
WebGpuDevice::~WebGpuDevice() = default;

const std::string& WebGpuDevice::adapterName() const noexcept {
    return impl_->adapterName;
}

std::vector<std::string> WebGpuDevice::takeErrors() {
    return impl_->drainErrors();
}

namespace {

/// Öffnet Instanz, Adapter und Gerät. `backends` leer = Voreinstellung der Bibliothek.
Result<std::unique_ptr<WebGpuDevice::Impl>> openImpl(const WebGpuOptions& options,
                                                      std::optional<WGPUInstanceBackend> backends) {
    auto impl = std::make_unique<WebGpuDevice::Impl>();

    WGPUInstanceExtras extras{};
    extras.chain.sType = static_cast<WGPUSType>(WGPUSType_InstanceExtras);
    WGPUInstanceDescriptor instanceDesc{};
    const bool backendChosen = backends.has_value();
    if (backendChosen) {
        extras.backends = *backends;
        instanceDesc.nextInChain = &extras.chain;
    }
    // Ohne Wunsch genau wie vor 0.36.2: kein Deskriptor, die Bibliothek nimmt ihre Voreinstellung.
    impl->instance = wgpuCreateInstance(backendChosen ? &instanceDesc : nullptr);
    if (impl->instance == nullptr) {
        return Error{ErrorCode::GpuError, "Die WebGPU-Instanz ließ sich nicht anlegen"};
    }

    AdapterRequest adapterRequest;
    WGPURequestAdapterOptions adapterOptions{};
    adapterOptions.featureLevel = WGPUFeatureLevel_Core;
    adapterOptions.powerPreference = WGPUPowerPreference_HighPerformance;
    adapterOptions.forceFallbackAdapter = options.forceFallbackAdapter ? 1 : 0;
    WGPURequestAdapterCallbackInfo adapterCallback{};
    adapterCallback.mode = WGPUCallbackMode_AllowProcessEvents;
    adapterCallback.callback = onAdapter;
    adapterCallback.userdata1 = &adapterRequest;
    wgpuInstanceRequestAdapter(impl->instance, &adapterOptions, adapterCallback);
    if (!waitFor(*impl, adapterRequest)) {
        return Error{ErrorCode::GpuError, "Zeitüberschreitung bei der Suche nach einem GPU-Adapter"};
    }
    if (adapterRequest.status != WGPURequestAdapterStatus_Success || adapterRequest.adapter == nullptr) {
        return Error{ErrorCode::GpuError, "Kein WebGPU-Adapter verfügbar: " + adapterRequest.message};
    }
    impl->adapter = adapterRequest.adapter;

    WGPUAdapterInfo info{};
    if (wgpuAdapterGetInfo(impl->adapter, &info) == WGPUStatus_Success) {
        impl->adapterName = toString(info.device);
        if (impl->adapterName.empty()) {
            impl->adapterName = toString(info.description);
        }
        impl->deviceName = impl->adapterName;
        impl->backendType = info.backendType;
        switch (info.backendType) {
            case WGPUBackendType_Vulkan: impl->adapterName += " [Vulkan]"; break;
            case WGPUBackendType_OpenGL:
            case WGPUBackendType_OpenGLES: impl->adapterName += " [OpenGL]"; break;
            case WGPUBackendType_D3D12: impl->adapterName += " [DX12]"; break;
            case WGPUBackendType_Metal: impl->adapterName += " [Metal]"; break;
            default: break;
        }
        wgpuAdapterInfoFreeMembers(info);
    }

    DeviceRequest deviceRequest;
    WGPUDeviceDescriptor deviceDesc{};
    deviceDesc.label = view(options.label);
    deviceDesc.uncapturedErrorCallbackInfo.callback = onUncapturedError;
    deviceDesc.uncapturedErrorCallbackInfo.userdata1 = impl.get();
    deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    deviceDesc.deviceLostCallbackInfo.callback = onDeviceLost;
    deviceDesc.deviceLostCallbackInfo.userdata1 = impl.get();
    WGPURequestDeviceCallbackInfo deviceCallback{};
    deviceCallback.mode = WGPUCallbackMode_AllowProcessEvents;
    deviceCallback.callback = onDevice;
    deviceCallback.userdata1 = &deviceRequest;
    wgpuAdapterRequestDevice(impl->adapter, &deviceDesc, deviceCallback);
    if (!waitFor(*impl, deviceRequest)) {
        return Error{ErrorCode::GpuError, "Zeitüberschreitung beim Anlegen des GPU-Geräts"};
    }
    if (deviceRequest.status != WGPURequestDeviceStatus_Success || deviceRequest.device == nullptr) {
        return Error{ErrorCode::GpuError, "Das GPU-Gerät ließ sich nicht anlegen: " + deviceRequest.message};
    }
    impl->device = deviceRequest.device;
    impl->queue = wgpuDeviceGetQueue(impl->device);

    return impl;
}

}  // namespace

bool prefersOpenGl(std::string_view deviceName) noexcept {
    // Gesucht: "GTX 760", "GT 740", "GTX 780 Ti", "GeForce GTX 770M" ... Kepler ist die 600er- und 700er-Reihe,
    // außer den Maxwell-Karten 745 und 750 (Ti); bei diesen (und 750M, das beides sein kann) bleibt es bei Vulkan.
    for (const std::string_view prefix : {std::string_view{"GTX "}, std::string_view{"GT "}}) {
        for (std::size_t at = deviceName.find(prefix); at != std::string_view::npos;
             at = deviceName.find(prefix, at + 1)) {
            if (at > 0 && deviceName[at - 1] != ' ') {
                continue;  // nur ganze Wörter
            }
            const std::size_t digits = at + prefix.size();
            if (digits + 3 > deviceName.size()) {
                continue;
            }
            int model = 0;
            bool numeric = true;
            for (std::size_t i = 0; i < 3; ++i) {
                const char c = deviceName[digits + i];
                numeric = numeric && c >= '0' && c <= '9';
                model = model * 10 + (c - '0');
            }
            const bool fourDigits = digits + 3 < deviceName.size() && deviceName[digits + 3] >= '0' && deviceName[digits + 3] <= '9';
            if (numeric && !fourDigits && model >= 600 && model < 800 && model != 745 && model != 750) {
                return true;
            }
        }
    }
    return false;
}

Result<std::unique_ptr<WebGpuDevice>> WebGpuDevice::create(const WebGpuOptions& options) {
    // WHITEHOLEFX_GPU_BACKEND=vulkan|gl|dx12|metal wählt die Grafikschnittstelle und gilt immer. Gedacht für die
    // Fehlersuche (Issue #13): Dieselbe GPU kann je nach Schnittstelle andere Shader-Übersetzung und Treiberfehler haben.
    if (const char* wanted = std::getenv("WHITEHOLEFX_GPU_BACKEND"); wanted != nullptr) {
        const std::string name{wanted};
        const WGPUInstanceBackend backend = name == "vulkan"  ? WGPUInstanceBackend_Vulkan
                                            : name == "gl"    ? WGPUInstanceBackend_GL
                                            : name == "dx12"  ? WGPUInstanceBackend_DX12
                                            : name == "metal" ? WGPUInstanceBackend_Metal
                                                              : WGPUInstanceBackend_All;
        Result<std::unique_ptr<Impl>> chosen = openImpl(options, backend);
        if (!chosen.ok()) {
            return chosen.error();
        }
        return std::unique_ptr<WebGpuDevice>(new WebGpuDevice(std::move(chosen.value())));
    }
    Result<std::unique_ptr<Impl>> first = openImpl(options, std::nullopt);
    if (!first.ok()) {
        return first.error();
    }
    // Bekannter Treiberfehler: Auf Kepler-Karten (GTX 600/700) kommt der Text-Node unter Vulkan nicht an (#13).
    // Dort nimmt WhiteHoleFX OpenGL; schlägt das fehl, bleibt es bei Vulkan.
    if (!options.forceFallbackAdapter && first.value()->backendType == WGPUBackendType_Vulkan &&
        prefersOpenGl(first.value()->deviceName)) {
        Result<std::unique_ptr<Impl>> gl = openImpl(options, WGPUInstanceBackend_GL);
        if (gl.ok() && gl.value()->backendType != WGPUBackendType_Vulkan) {
            return std::unique_ptr<WebGpuDevice>(new WebGpuDevice(std::move(gl.value())));
        }
    }
    return std::unique_ptr<WebGpuDevice>(new WebGpuDevice(std::move(first.value())));
}

void WebGpuDevice::poll() {
    Impl& impl = *impl_;
    impl.pump(false);

    // Abgeschlossene Readbacks ausliefern. Der Callback der Bibliothek hat nur den Status
    // gesetzt; die Nutzer-Callbacks laufen hier, außerhalb der Bibliotheksaufrufe.
    std::vector<ReadbackState*> ready;
    for (auto it = impl.readbacks.begin(); it != impl.readbacks.end();) {
        if ((*it)->done) {
            ready.push_back(*it);
            it = impl.readbacks.erase(it);
        } else {
            ++it;
        }
    }
    for (ReadbackState* state : ready) {
        if (state->buffer != nullptr && state->status == WGPUMapAsyncStatus_Success) {
            const void* mapped = wgpuBufferGetConstMappedRange(state->buffer, 0, static_cast<size_t>(state->size));
            if (mapped != nullptr) {
                state->callback(std::span<const std::byte>{static_cast<const std::byte*>(mapped),
                                                           static_cast<size_t>(state->size)});
            } else {
                state->callback(Error{ErrorCode::GpuError, "Der gemappte Puffer ist nicht lesbar"});
            }
            wgpuBufferUnmap(state->buffer);
        } else {
            state->callback(Error{ErrorCode::GpuError, "Readback fehlgeschlagen: " + state->message});
        }
        const auto entry = impl.buffers.find(state->bufferId);
        if (entry != impl.buffers.end() && entry->second.pendingMaps > 0) {
            --entry->second.pendingMaps;
        }
        if (state->buffer != nullptr) {
            wgpuBufferRelease(state->buffer);
        }
        delete state;
    }
}

}  // namespace whitehole::gpu
