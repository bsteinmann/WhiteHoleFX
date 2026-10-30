// main.cpp: Konsolen-Runner für WhiteHoleFX-Projekte.
//
// Ablauf: Optionen lesen -> Projektdatei lesen und prüfen -> mit --check den Bericht ausgeben,
// sonst GPU-Gerät und UDP-Versand anlegen, die Session starten und im Takt der Bildrate Frames
// rendern, bis --frames erreicht ist oder Strg+C kommt. Beendet wird immer geordnet: die
// laufenden Frames werden noch ausgeliefert, dann erscheint eine Zusammenfassung.
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "cli_options.hpp"
#include "whitehole/engine/frame_clocks.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/output/udp_sender.hpp"
#include "whitehole/preview/png.hpp"
#include "whitehole/project/project.hpp"
#include "whitehole/runtime/session.hpp"
#include "whitehole/runtime/summary.hpp"

#ifdef WH_CLI_HAVE_GPU
#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#endif

using namespace whitehole;

namespace {

std::atomic<bool> gStop{false};

extern "C" void onSignal(int) { gStop.store(true); }

int fail(const Error& error) {
    std::cerr << "Fehler: " << error.message << "\n";
    return 1;
}

Result<void> saveSnapshot(const mapping::LedMap& map, const std::vector<ColorRgba>& colors, const cli::Options& options) {
    preview::PreviewOptions previewOptions;
    previewOptions.width = options.snapshotWidth;
    previewOptions.height = options.snapshotHeight;
    const auto image = preview::renderPreview(map, colors, previewOptions);
    if (!image.ok()) {
        return image.error();
    }
    return preview::writePng(image.value(), options.snapshotPath);
}

#ifdef WH_CLI_HAVE_GPU
void printStats(const runtime::Session& session, std::ostream& out) {
    const runtime::SessionStats s = session.stats();
    out << "Frames " << s.engine.framesRendered << " (verworfen " << s.engine.framesDropped
        << "), Pakete " << s.output.packetsSent << ", Sendefehler " << s.output.sendErrors
        << ", Ausgabe-Fehler " << s.frameErrors << "\n";
}

int run(const project::Project& project, const nodes::BuiltinLibrary& library, const cli::Options& options) {
    gpu::WebGpuOptions gpuOptions;
    gpuOptions.forceFallbackAdapter = options.software;
    auto device = gpu::WebGpuDevice::create(gpuOptions);
    if (!device.ok() && !options.software) {
        std::cerr << "Hinweis: " << device.error().message << "\nVersuche den Software-Adapter.\n";
        gpuOptions.forceFallbackAdapter = true;
        device = gpu::WebGpuDevice::create(gpuOptions);
    }
    if (!device.ok()) {
        return fail(device.error());
    }
    auto sender = output::UdpSender::create();
    if (!sender.ok()) {
        return fail(sender.error());
    }

    engine::SteadyFrameClock clock;
    runtime::SessionOptions sessionOptions;
    sessionOptions.keepLatestColors = !options.snapshotPath.empty();
    auto session = runtime::Session::create(
        project, runtime::SessionDeps{*device.value(), library.snippets, library.kernels, *sender.value(), clock},
        sessionOptions);
    if (!session.ok()) {
        return fail(session.error());
    }

    const double fps = options.fps.value_or(project.framesPerSecond);
    const auto period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>{1.0 / fps});
    std::cerr << "Läuft mit " << fps << " fps, " << session.value()->ledCount() << " LEDs. Strg+C beendet.\n";

    // Eine Vorschau ohne Frameanzahl macht genau einen Frame.
    std::optional<std::uint64_t> frames = options.frames;
    if (!frames && !options.snapshotPath.empty()) {
        frames = 1;
    }
    const auto start = std::chrono::steady_clock::now();
    auto lastReport = start;
    for (std::uint64_t frame = 0; !gStop.load() && (!frames || frame < *frames); ++frame) {
        if (const Result<void> r = session.value()->renderFrame(); !r.ok()) {
            return fail(r.error());
        }
        const auto now = std::chrono::steady_clock::now();
        if (now - lastReport >= std::chrono::seconds{2}) {
            lastReport = now;
            printStats(*session.value(), std::cerr);
        }
        std::this_thread::sleep_until(start + period * static_cast<std::int64_t>(frame + 1));
    }

    const bool flushed = session.value()->flush(std::chrono::seconds{2});
    printStats(*session.value(), std::cout);
    if (!flushed) {
        std::cerr << "Hinweis: nicht alle Frames wurden mehr verarbeitet.\n";
    }
    if (const auto error = session.value()->stageError()) {
        std::cerr << "Effekt-Fehler: " << error->message << "\n";
        return 1;
    }
    if (!options.snapshotPath.empty()) {
        const std::vector<ColorRgba> colors = session.value()->latestColors();
        if (colors.empty()) {
            std::cerr << "Fehler: Es ist kein Frame angekommen, die Vorschau bleibt leer.\n";
            return 1;
        }
        if (const Result<void> saved = saveSnapshot(session.value()->map(), colors, options); !saved.ok()) {
            return fail(saved.error());
        }
        std::cout << "Vorschau gespeichert: " << options.snapshotPath << "\n";
    }
    if (const auto error = session.value()->lastOutputError()) {
        std::cerr << "Letzter Ausgabefehler: " << error->message << "\n";
    }
    return 0;
}
#else
int run(const project::Project&, const nodes::BuiltinLibrary&, const cli::Options&) {
    std::cerr << "Dieses Programm wurde ohne GPU-Backend gebaut. Mit -DWHITEHOLEFX_WEBGPU=ON neu bauen;\n"
                 "--check funktioniert auch so.\n";
    return 2;
}
#endif

}  // namespace

int main(int argc, char** argv) {
    const auto options = cli::parseOptions(std::vector<std::string>(argv + 1, argv + argc));
    if (!options.ok()) {
        std::cerr << options.error().message << "\n";
        return 2;
    }
    if (options.value().help) {
        std::cout << cli::usageText();
        return 0;
    }

    auto library = nodes::BuiltinLibrary::create();
    if (!library.ok()) {
        return fail(library.error());
    }
    const auto text = project::readTextFile(options.value().projectPath);
    if (!text.ok()) {
        return fail(text.error());
    }
    auto parsed = project::parseProject(text.value(), library.value()->registry);
    if (!parsed.ok()) {
        return fail(parsed.error());
    }

    if (options.value().check) {
        const auto report = runtime::summarize(parsed.value());
        if (!report.ok()) {
            return fail(report.error());
        }
        std::cout << report.value();
        if (!options.value().snapshotPath.empty()) {
            const auto map = parsed.value().layout.bake(parsed.value().normalize);
            if (!map.ok()) {
                return fail(map.error());
            }
            if (const Result<void> saved =
                    saveSnapshot(map.value(), preview::layoutColors(map.value()), options.value());
                !saved.ok()) {
                return fail(saved.error());
            }
            std::cout << "Layout-Vorschau gespeichert: " << options.value().snapshotPath << "\n";
        }
        return 0;
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    return run(parsed.value(), *library.value(), options.value());
}
