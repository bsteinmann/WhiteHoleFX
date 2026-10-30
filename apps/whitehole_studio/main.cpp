// main.cpp: Einstieg der Qt-Oberfläche.
//
//   whitehole_studio [projekt.whfx] [--run] [--software]
//   whitehole_studio projekt.whfx --grab fenster.png [--frames 40] [--run]    (für die CI)
//
// --grab öffnet das Fenster (mit QT_QPA_PLATFORM=offscreen auch ohne Bildschirm), lässt optional
// einige Frames laufen, speichert ein Bild des Fensters und beendet sich. Das dient als
// Rauchtest der ganzen Oberfläche.
#include <QApplication>
#include <QCommandLineParser>
#include <QPixmap>
#include <QThread>

#include <chrono>
#include <memory>

#include "main_window.hpp"

#ifdef WH_STUDIO_HAVE_GPU
#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#endif

using namespace whitehole;

int main(int argc, char** argv) {
    QApplication app{argc, argv};
    QApplication::setApplicationName(QStringLiteral("WhiteHoleFX"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("WhiteHoleFX Studio"));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("projekt"), QStringLiteral("Projektdatei (.whfx)"), QStringLiteral("[projekt]"));
    const QCommandLineOption runOption{QStringLiteral("run"), QStringLiteral("Wiedergabe sofort starten")};
    const QCommandLineOption softwareOption{QStringLiteral("software"), QStringLiteral("Software-Adapter für die GPU bevorzugen")};
    const QCommandLineOption grabOption{QStringLiteral("grab"), QStringLiteral("Fensterbild speichern und beenden"), QStringLiteral("datei.png")};
    const QCommandLineOption framesOption{QStringLiteral("frames"), QStringLiteral("Frames vor --grab (Standard 30 mit --run)"), QStringLiteral("n"), QStringLiteral("30")};
    parser.addOptions({runOption, softwareOption, grabOption, framesOption});
    parser.process(app);

    std::unique_ptr<gpu::IGpuDevice> device;
    QString note;
#ifdef WH_STUDIO_HAVE_GPU
    gpu::WebGpuOptions gpuOptions;
    gpuOptions.forceFallbackAdapter = parser.isSet(softwareOption);
    auto created = gpu::WebGpuDevice::create(gpuOptions);
    if (!created.ok() && !gpuOptions.forceFallbackAdapter) {
        gpuOptions.forceFallbackAdapter = true;
        created = gpu::WebGpuDevice::create(gpuOptions);
    }
    if (created.ok()) {
        device = std::move(created).value();
    } else {
        note = QStringLiteral("Kein GPU-Adapter: %1").arg(QString::fromUtf8(created.error().message.c_str()));
    }
#else
    note = QStringLiteral("Dieses Programm wurde ohne GPU-Backend gebaut (-DWHITEHOLEFX_WEBGPU=ON); es zeigt nur das Layout.");
#endif

    studio::MainWindow window{device.get(), note};
    window.resize(1100, 680);
    window.show();

    const QStringList args = parser.positionalArguments();
    const bool grab = parser.isSet(grabOption);
    bool opened = false;
    if (!args.isEmpty()) {
        opened = window.openPath(args.first(), !grab);
        if (!opened && grab) {
            return 1;
        }
    }
    if (parser.isSet(runOption) && opened) {
        if (!window.startPlayback(!grab) && grab) {
            return 1;
        }
    }

    if (!grab) {
        return QApplication::exec();
    }

    // Rauchtest: einige Frames im Takt, dann Fensterbild.
    QApplication::processEvents();
    bool ok = true;
    if (parser.isSet(runOption)) {
        const int frames = parser.value(framesOption).toInt();
        for (int i = 0; i < frames; ++i) {
            window.tick();
            QApplication::processEvents();
            QThread::msleep(25);
        }
    }
    window.refreshImage();
    QApplication::processEvents();
    ok = window.grab().save(parser.value(grabOption), "PNG");
    window.stopPlayback();
    return ok ? 0 : 1;
}
