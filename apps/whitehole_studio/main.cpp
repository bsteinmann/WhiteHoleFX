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
#include <cstdio>
#include <memory>

#include "app_language.hpp"
#include "main_window.hpp"

#ifdef WH_STUDIO_HAVE_GPU
#include "whitehole/gpu_webgpu/webgpu_device.hpp"
#endif

using namespace whitehole;

int main(int argc, char** argv) {
    // Ausgabe sofort schreiben: Stürzt ein Rauchtest ab, soll man sehen, wie weit er kam.
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    QApplication app{argc, argv};
    QApplication::setApplicationName(QStringLiteral("WhiteHoleFX"));
    QApplication::setApplicationVersion(QStringLiteral(WH_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("whitehole-studio"));  // passt zum Starter (.desktop)

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("WhiteHoleFX Studio"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("projekt"), QStringLiteral("Projektdatei (.whfx)"), QStringLiteral("[projekt]"));
    const QCommandLineOption runOption{QStringLiteral("run"), QStringLiteral("Wiedergabe sofort starten")};
    const QCommandLineOption softwareOption{QStringLiteral("software"), QStringLiteral("Software-Adapter für die GPU bevorzugen")};
    const QCommandLineOption grabOption{QStringLiteral("grab"), QStringLiteral("Fensterbild speichern und beenden"), QStringLiteral("datei.png")};
    const QCommandLineOption framesOption{QStringLiteral("frames"), QStringLiteral("Frames vor --grab (Standard 30 mit --run)"), QStringLiteral("n"), QStringLiteral("30")};
    const QCommandLineOption exerciseOption{QStringLiteral("exercise"), QStringLiteral("vor --grab alle Regler einmal verstellen (Rauchtest)")};
    const QCommandLineOption langOption{QStringLiteral("lang"), QStringLiteral("Sprache der Oberfläche: de, en, fr oder es"), QStringLiteral("code")};
    parser.addOptions({runOption, softwareOption, grabOption, framesOption, exerciseOption, langOption});
    parser.process(app);

    // Sprache vor dem ersten Fenster setzen. Die Rauchtests (--grab) prüfen deutsche Texte und laufen deshalb
    // deutsch, außer --lang sagt etwas anderes.
    const bool grabbing = parser.isSet(grabOption);
    const QString language = studio::initialLanguage(parser.isSet(langOption) ? parser.value(langOption)
                                                     : grabbing                ? QStringLiteral("de")
                                                                               : QString{});
    if (!studio::installLanguage(language)) {
        std::fprintf(stderr, "Sprache %s lässt sich nicht laden, es bleibt bei Deutsch.\n", language.toUtf8().constData());
    }

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
        note = QObject::tr("Kein GPU-Adapter: %1").arg(QString::fromUtf8(created.error().message.c_str()));
    }
#else
    note = QObject::tr("Dieses Programm wurde ohne GPU-Backend gebaut (-DWHITEHOLEFX_WEBGPU=ON); es zeigt nur das Layout.");
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
    if (parser.isSet(exerciseOption)) {
        const int changed = window.exerciseParameters();
        std::printf("Regler verstellt: %d\n", changed);
        for (int i = 0; i < 10; ++i) {
            window.tick();
            QApplication::processEvents();
            QThread::msleep(25);
        }
        const QString editorProblem = window.exerciseEditor();
        std::printf("Node-Editor: %s\n", editorProblem.isEmpty() ? "ok" : "FEHLER");
        if (!editorProblem.isEmpty()) {
            std::fprintf(stderr, "Rauchtest fehlgeschlagen: Node-Editor, %s\n", editorProblem.toUtf8().constData());
            return 1;
        }
        const QString selection = window.exerciseSelection();
        std::printf("Node-Auswahl: %s\n", selection.isEmpty() ? "ok" : "FEHLER");
        const QString created = selection.isEmpty() ? window.exerciseNewProject() : QString{};
        std::printf("Neues Projekt: %s\n", created.isEmpty() ? "ok" : "FEHLER");
        const QString transparency = created.isEmpty() ? window.exerciseTransparency() : QString{};
        std::printf("Transparent: %s\n", transparency.isEmpty() ? "ok" : "FEHLER");
        const QString controls = transparency.isEmpty() ? window.exerciseControls() : QString{};
        std::printf("Master/Blackout: %s\n", controls.isEmpty() ? "ok" : "FEHLER");
        const QString dmx = controls.isEmpty() ? window.exerciseDmx() : QString{};
        std::printf("DMX-Eingang: %s\n", dmx.isEmpty() ? "ok" : "FEHLER");
        const QString audio = dmx.isEmpty() ? window.exerciseAudio() : QString{};
        std::printf("Audio-Eingang: %s\n", audio.isEmpty() ? "ok" : "FEHLER");
        const QString palette = audio.isEmpty() ? window.exercisePalette() : QString{};
        std::printf("Node-Palette: %s\n", palette.isEmpty() ? "ok" : "FEHLER");
        const QString live = palette.isEmpty() ? window.exerciseLive() : QString{};
        std::printf("Looks/Live: %s\n", live.isEmpty() ? "ok" : "FEHLER");
        const QString settingsResult = live.isEmpty() ? window.exerciseSettings() : QString{};
        std::printf("Einstellungen: %s\n", settingsResult.isEmpty() ? "ok" : "FEHLER");
        const QString stage = settingsResult.isEmpty() ? window.exerciseStageSetup() : QString{};
        std::printf("Stage Config: %s\n", stage.isEmpty() ? "ok" : "FEHLER");
        const QString view3d = stage.isEmpty() ? window.exerciseView3d() : QString{};
        std::printf("3D-Vorschau: %s\n", view3d.isEmpty() ? "ok" : "FEHLER");
        const QString menus = view3d.isEmpty() ? window.exerciseMenus() : QString{};
        std::printf("Menüleiste: %s\n", menus.isEmpty() ? "ok" : "FEHLER");
        const QString languages = menus.isEmpty() ? window.exerciseLanguage() : QString{};
        std::printf("Sprachen/Tutorial: %s\n", languages.isEmpty() ? "ok" : "FEHLER");
        for (const QString& failure : {selection, created, transparency, controls, dmx, audio, palette, live, settingsResult, stage, view3d, menus, languages}) {
            if (!failure.isEmpty()) {
                std::fprintf(stderr, "Rauchtest fehlgeschlagen: %s\n", failure.toUtf8().constData());
                return 1;
            }
        }
        const QString problem = window.problem();
        if (changed == 0 || !problem.isEmpty()) {
            std::fprintf(stderr, "Rauchtest fehlgeschlagen: %s\n", changed == 0 ? "kein Regler gefunden" : problem.toUtf8().constData());
            return 1;
        }
    }
    window.refreshImage();
    QApplication::processEvents();
    ok = window.grab().save(parser.value(grabOption), "PNG");
    window.stopPlayback();
    return ok ? 0 : 1;
}
