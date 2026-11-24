// benchmark_sensors.cpp: NVML zur Laufzeit laden und lesen (siehe benchmark_sensors.hpp).
//
// Nur die fünf nötigen Funktionen, mit selbst deklarierten Typen (kein NVML-Header nötig). Rückgabe 0 = Erfolg.
#include "whitehole/runtime/benchmark_sensors.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace whitehole::runtime {

namespace {

using NvmlDevice = void*;
using InitFn = int (*)();
using CountFn = int (*)(unsigned*);
using HandleFn = int (*)(unsigned, NvmlDevice*);
using NameFn = int (*)(NvmlDevice, char*, unsigned);
using ValueFn = int (*)(NvmlDevice, int, unsigned*);
constexpr int kTemperatureGpu = 0;  // NVML_TEMPERATURE_GPU
constexpr int kClockGraphics = 0;   // NVML_CLOCK_GRAPHICS

class Library {
public:
    Library() {
#ifdef _WIN32
        handle_ = LoadLibraryA("nvml.dll");
#else
        handle_ = dlopen("libnvidia-ml.so.1", RTLD_NOW | RTLD_LOCAL);
#endif
    }
    ~Library() {
        if (handle_ == nullptr) return;
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle_));
#else
        dlclose(handle_);
#endif
    }
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;
    [[nodiscard]] bool loaded() const noexcept { return handle_ != nullptr; }
    template <typename Fn>
    Fn get(const char* name) const {
#ifdef _WIN32
        return reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), name)));
#else
        return reinterpret_cast<Fn>(dlsym(handle_, name));
#endif
    }

private:
    void* handle_{nullptr};
};

class NvmlSensors final : public IGpuSensors {
public:
    NvmlSensors(std::unique_ptr<Library> lib, InitFn shutdown, ValueFn temperature, ValueFn clock, NvmlDevice device)
        : lib_{std::move(lib)}, shutdown_{shutdown}, temperature_{temperature}, clock_{clock}, device_{device} {}
    ~NvmlSensors() override {
        if (shutdown_ != nullptr) shutdown_();
    }
    GpuSensorReading read() override {
        GpuSensorReading reading;
        unsigned value = 0;
        if (temperature_(device_, kTemperatureGpu, &value) == 0) reading.temperatureC = value;
        if (clock_(device_, kClockGraphics, &value) == 0) reading.graphicsClockMHz = value;
        return reading;
    }

private:
    std::unique_ptr<Library> lib_;
    InitFn shutdown_;
    ValueFn temperature_;
    ValueFn clock_;
    NvmlDevice device_;
};

std::string lower(std::string_view text) {
    std::string out{text};
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

}  // namespace

bool gpuNamesMatch(std::string_view nvmlName, std::string_view adapterName) noexcept {
    if (nvmlName.empty() || adapterName.empty()) {
        return false;
    }
    const std::string a = lower(nvmlName);
    const std::string b = lower(adapterName);
    return a.find(b) != std::string::npos || b.find(a) != std::string::npos;
}

std::unique_ptr<IGpuSensors> openNvidiaSensors(std::string_view deviceName) {
    auto lib = std::make_unique<Library>();
    if (!lib->loaded()) {
        return nullptr;
    }
    const auto init = lib->get<InitFn>("nvmlInit_v2");
    const auto shutdown = lib->get<InitFn>("nvmlShutdown");
    const auto count = lib->get<CountFn>("nvmlDeviceGetCount_v2");
    const auto handle = lib->get<HandleFn>("nvmlDeviceGetHandleByIndex_v2");
    const auto name = lib->get<NameFn>("nvmlDeviceGetName");
    const auto temperature = lib->get<ValueFn>("nvmlDeviceGetTemperature");
    const auto clock = lib->get<ValueFn>("nvmlDeviceGetClockInfo");
    if (init == nullptr || shutdown == nullptr || count == nullptr || handle == nullptr || name == nullptr ||
        temperature == nullptr || clock == nullptr || init() != 0) {
        return nullptr;
    }
    unsigned devices = 0;
    if (count(&devices) == 0) {
        for (unsigned i = 0; i < devices; ++i) {
            NvmlDevice device = nullptr;
            char buffer[96]{};
            if (handle(i, &device) == 0 && name(device, buffer, sizeof buffer) == 0 && gpuNamesMatch(buffer, deviceName)) {
                return std::make_unique<NvmlSensors>(std::move(lib), shutdown, temperature, clock, device);
            }
        }
    }
    shutdown();
    return nullptr;
}

}  // namespace whitehole::runtime
