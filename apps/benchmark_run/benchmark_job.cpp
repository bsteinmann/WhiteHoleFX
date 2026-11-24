// benchmark_job.cpp: siehe benchmark_job.hpp.
#include "benchmark_job.hpp"

#include <iostream>
#include <string>

#ifdef WH_BENCH_HAVE_GPU
#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#include "whitehole/runtime/benchmark_gpu_target.hpp"
#include "whitehole/runtime/benchmark_sensors.hpp"
#endif

namespace whitehole::benchapp {

#ifdef WH_BENCH_HAVE_GPU
Result<BenchmarkResult> runBenchmarkJob(const BenchmarkJob& job, const nodes::BuiltinLibrary& library,
                                        const std::atomic<bool>& cancel, std::string_view version,
                                        std::string_view buildId) {
    using namespace runtime;
    gpu::WebGpuOptions gpuOptions;
    gpuOptions.forceFallbackAdapter = job.software;
    gpuOptions.label = "WhiteHoleFX Benchmark";
    gpuOptions.timestamps = true;
    auto device = gpu::WebGpuDevice::create(gpuOptions);
    if (!device.ok()) {
        return device.error();
    }
    const auto plan = benchmarkPlan(job.series, job.mode);
    std::cerr << "GPU-Adapter: " << device.value()->adapterName() << (job.software ? " (Software)" : "")
              << (device.value()->measuresGpuTime() ? "" : ", ohne GPU-Zeitstempel") << "\n"
              << "Benchmark: " << plan.size() << " Stufen, je etwa 12 s (Strg+C bricht ab).\n";

    GpuBenchmarkTarget target{*device.value(), library};
    const auto sensors = openNvidiaSensors(device.value()->deviceName());  // erstmal nur NVIDIA, sonst nullptr
    BenchmarkHooks hooks;
    hooks.cancel = &cancel;
    hooks.sensors = sensors.get();
    hooks.stageStarted = [&plan](std::size_t i, std::size_t count) {
        std::cerr << "  Stufe " << (i + 1) << "/" << count << ": " << benchmarkStageLabel(plan[i]) << " "
                  << benchmarkSceneId(plan[i].scene) << "\n";
    };
    BenchmarkResult result;
    result.run = runBenchmark(plan, target, result.limits, hooks);
    result.info = BenchmarkSystemInfo{device.value()->deviceName(), device.value()->backendName(),
                                      device.value()->driverInfo(), std::string{version}, std::string{buildId}};
    return result;
}
#else
Result<BenchmarkResult> runBenchmarkJob(const BenchmarkJob&, const nodes::BuiltinLibrary&, const std::atomic<bool>&,
                                        std::string_view, std::string_view) {
    return Error{ErrorCode::GpuError,
                 "Dieses Programm wurde ohne GPU-Backend gebaut; der Benchmark braucht -DWHITEHOLEFX_WEBGPU=ON."};
}
#endif

}  // namespace whitehole::benchapp
