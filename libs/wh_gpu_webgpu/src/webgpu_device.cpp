// webgpu_device.cpp: Umsetzung zu webgpu_device.hpp. IGpuDevice auf einer echten GPU (oder einem Software-Rasterizer).
#include "whitehole/gpu_webgpu/webgpu_device.hpp"

#include <chrono>
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

Result<std::unique_ptr<WebGpuDevice>> WebGpuDevice::create(const WebGpuOptions& options) {
    auto impl = std::make_unique<Impl>();

    impl->instance = wgpuCreateInstance(nullptr);
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

    return std::unique_ptr<WebGpuDevice>(new WebGpuDevice(std::move(impl)));
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
