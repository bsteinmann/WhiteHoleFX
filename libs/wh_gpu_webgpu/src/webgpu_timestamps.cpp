// webgpu_timestamps.cpp: GPU-Laufzeit je Frame über Zeitstempel-Abfragen (Benchmark, docs/entwurf-benchmark.md).
//
// Warum es diese Datei gibt: Die Gesamtzeit eines Frames enthält auch CPU-Anteile und das Warten auf den Readback; wie
// lange die Grafikkarte selbst rechnet, sagen nur Zeitstempel der GPU. Sie sind ein optionales Feature (nicht jeder
// Adapter und Treiber kann es, der Software-Adapter meist nicht) und werden nur mit WebGpuOptions::timestamps
// angefordert, damit der normale Betrieb unverändert bleibt.
//
// Ablauf je submit: Der erste Compute-Pass schreibt den Anfang (Index 0), jeder Pass das Ende (Index 1, der letzte
// gewinnt). Danach werden beide Werte in einen Puffer aufgelöst, kopiert und nach dem Einreichen gemappt; poll() liest sie.
// Solange ein Wert noch gemappt wird, misst der nächste Frame nicht mit (kein zweiter Lese-Puffer nötig).
#include <cstring>

#include "webgpu_impl.hpp"

namespace whitehole::gpu {

namespace {

constexpr std::uint64_t kBytes = 2 * sizeof(std::uint64_t);

void onTimestampsMapped(WGPUMapAsyncStatus status, WGPUStringView, void* user, void*) {
    auto* state = static_cast<TimestampState*>(user);
    state->mapStatus = status;
    state->mapDone = true;
}

WGPUBuffer makeBuffer(WGPUDevice device, WGPUBufferUsage usage, const char* label) {
    WGPUBufferDescriptor bd{};
    bd.label = view(label);
    bd.size = kBytes;
    bd.usage = usage;
    return wgpuDeviceCreateBuffer(device, &bd);
}

}  // namespace

bool allowsGpuTimestamps(std::string_view backend, bool softwareAdapter) noexcept {
    return !(softwareAdapter && backend == "DX12");
}

bool setupTimestamps(WebGpuDevice::Impl& impl) {
    TimestampState& ts = impl.timestamps;
    WGPUQuerySetDescriptor qd{};
    qd.label = view("wh.timestamps");
    qd.type = WGPUQueryType_Timestamp;
    qd.count = 2;
    ts.querySet = wgpuDeviceCreateQuerySet(impl.device, &qd);
    ts.resolve = makeBuffer(impl.device, WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc, "wh.timestamps.resolve");
    ts.read = makeBuffer(impl.device, WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst, "wh.timestamps.read");
    if (ts.querySet == nullptr || ts.resolve == nullptr || ts.read == nullptr) {
        releaseTimestamps(impl);
        return false;
    }
    const float period = wgpuQueueGetTimestampPeriod(impl.queue);
    ts.nsPerTick = period > 0.0F ? static_cast<double>(period) : 1.0;
    return true;
}

void releaseTimestamps(WebGpuDevice::Impl& impl) noexcept {
    TimestampState& ts = impl.timestamps;
    if (ts.read != nullptr) {
        wgpuBufferDestroy(ts.read);
        wgpuBufferRelease(ts.read);
        ts.read = nullptr;
    }
    if (ts.resolve != nullptr) {
        wgpuBufferDestroy(ts.resolve);
        wgpuBufferRelease(ts.resolve);
        ts.resolve = nullptr;
    }
    if (ts.querySet != nullptr) {
        // Nur freigeben: wgpuQuerySetDestroy vor dem Release bringt wgpu-native 29 zum Absturz („vacant resource“).
        wgpuQuerySetRelease(ts.querySet);
        ts.querySet = nullptr;
    }
}

void beginFrameTimestamps(WebGpuDevice::Impl& impl) {
    TimestampState& ts = impl.timestamps;
    ts.recording = ts.querySet != nullptr && !ts.mapPending;
    ts.firstPass = true;
    ts.wrote = false;
    ts.resolved = false;
}

const WGPUPassTimestampWrites* passTimestampWrites(WebGpuDevice::Impl& impl, WGPUPassTimestampWrites& writes) {
    TimestampState& ts = impl.timestamps;
    if (!ts.recording) {
        return nullptr;
    }
    writes = WGPUPassTimestampWrites{};
    writes.querySet = ts.querySet;
    writes.beginningOfPassWriteIndex = ts.firstPass ? 0U : WGPU_QUERY_SET_INDEX_UNDEFINED;
    writes.endOfPassWriteIndex = 1U;
    ts.firstPass = false;
    ts.wrote = true;
    return &writes;
}

void resolveFrameTimestamps(WebGpuDevice::Impl& impl, WGPUCommandEncoder encoder) {
    TimestampState& ts = impl.timestamps;
    if (!ts.recording || !ts.wrote) {
        return;
    }
    wgpuCommandEncoderResolveQuerySet(encoder, ts.querySet, 0, 2, ts.resolve, 0);
    wgpuCommandEncoderCopyBufferToBuffer(encoder, ts.resolve, 0, ts.read, 0, kBytes);
    ts.resolved = true;
}

void afterSubmitTimestamps(WebGpuDevice::Impl& impl) {
    TimestampState& ts = impl.timestamps;
    if (!ts.resolved) {
        return;
    }
    ts.resolved = false;
    ts.mapPending = true;
    ts.mapDone = false;
    WGPUBufferMapCallbackInfo info{};
    info.mode = WGPUCallbackMode_AllowProcessEvents;
    info.callback = onTimestampsMapped;
    info.userdata1 = &ts;
    wgpuBufferMapAsync(ts.read, WGPUMapMode_Read, 0, kBytes, info);
}

void pollTimestamps(WebGpuDevice::Impl& impl) {
    TimestampState& ts = impl.timestamps;
    if (!ts.mapPending || !ts.mapDone) {
        return;
    }
    ts.mapPending = false;
    if (ts.mapStatus != WGPUMapAsyncStatus_Success) {
        return;
    }
    if (const void* mapped = wgpuBufferGetConstMappedRange(ts.read, 0, kBytes); mapped != nullptr) {
        std::uint64_t ticks[2]{};
        std::memcpy(ticks, mapped, kBytes);
        if (ticks[1] >= ticks[0]) {
            ts.lastMs = static_cast<double>(ticks[1] - ticks[0]) * ts.nsPerTick / 1e6;
        }
    }
    wgpuBufferUnmap(ts.read);
}

}  // namespace whitehole::gpu
