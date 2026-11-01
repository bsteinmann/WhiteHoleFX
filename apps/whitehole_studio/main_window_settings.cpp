// main_window_settings.cpp: Einstellungen im Hauptfenster (Menü, Dialog, Anwenden, Rauchtest).
//
// Warum eine eigene Datei: main_window.cpp ist an der 400-Zeilen-Grenze. Hier steht, wie die Wahl aus dem
// Dialog (Netzwerkkarten, DMX-Eingang, Audioquelle) wirklich umgesetzt wird: Karte am Sender binden,
// Empfänger neu starten, Audioquelle wechseln. Jeder Schritt meldet seinen Fehler als Text; ein Schritt,
// der scheitert, lässt die übrigen gelten und den alten Stand dieser einen Einstellung bestehen.
#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>
#include <QStringList>

#include <memory>

#include "main_window.hpp"
#include "settings_dialog.hpp"

namespace whitehole::studio {

void MainWindow::buildSettings(QMenu* editMenu) {
    audioInput_ = std::make_unique<audio::AudioInput>(library_->audio);
    settingsAction_ = new QAction{tr("&Einstellungen …"), this};
    settingsAction_->setShortcut(QKeySequence{Qt::CTRL | Qt::Key_Comma});
    settingsAction_->setToolTip(tr("Netzwerkkarte, DMX-Eingang und Audio-Interface wählen"));
    connect(settingsAction_, &QAction::triggered, this, [this]() { openSettings(); });
    editMenu->addSeparator();
    editMenu->addAction(settingsAction_);

    // Gemerkte Wahl umsetzen (ohne zu speichern). Fehler stehen in der Statuszeile; die gespeicherte Wahl bleibt.
    const QString problems = applySettings(loadAppSettings(), false);
    if (!problems.isEmpty()) {
        statusBar()->showMessage(problems, 12000);
    }
}

void MainWindow::openSettings() {
    SettingsDialog dialog{appSettings_, this};
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString problems = applySettings(dialog.settings(), true);
    if (!problems.isEmpty()) {
        QMessageBox::warning(this, tr("Einstellungen"), problems);
    }
    updateStatus();
}

QString MainWindow::applySettings(const AppSettings& wanted, bool persist) {
    QStringList problems;
    AppSettings next = wanted;

    if (next.outputInterface != appSettings_.outputInterface && udp_) {
        const Result<void> bound = udp_->bindInterface(next.outputInterface.toStdString());
        if (!bound.ok()) {
            problems << tr("Netzwerkkarte zum Senden: %1").arg(QString::fromStdString(bound.error().message));
            next.outputInterface = appSettings_.outputInterface;  // bleibt, wie sie war
        }
    }

    const bool dmxChanged = next.artnetIn != appSettings_.artnetIn || next.sacnIn != appSettings_.sacnIn ||
                            next.artnetPort != appSettings_.artnetPort || next.sacnPort != appSettings_.sacnPort ||
                            next.inputInterface != appSettings_.inputInterface;
    const QString audioBefore = appSettings_.audioSource;
    appSettings_ = next;
    if (dmxChanged) {
        if (const QString error = applyDmxInput(); !error.isEmpty()) {
            problems << error;
        }
    }
    if (next.audioSource != audioBefore) {
        if (const QString error = applyAudioChoice(next.audioSource); !error.isEmpty()) {
            problems << error;
        }
    }
    appSettings_.audioSource = audioChoice_;  // nach einem Fehler steht dort "off"

    if (persist) {
        saveAppSettings(appSettings_);
    }
    return problems.join(QLatin1Char('\n'));
}

QString MainWindow::exerciseSettings() {
    if (settingsAction_ == nullptr || settingsAction_->shortcut().isEmpty()) {
        return QStringLiteral("Einstellungen: der Menüpunkt fehlt");
    }
    // Die Felder des Dialogs geben zurück, was man hineinsteckt (auch Karten und Geräte, die es nicht gibt).
    AppSettings probe;
    probe.artnetIn = true;
    probe.sacnIn = true;
    probe.artnetPort = 7000;
    probe.sacnPort = 7001;
    probe.outputInterface = QStringLiteral("203.0.113.9");
    probe.inputInterface = QStringLiteral("203.0.113.10");
    probe.audioSource = QStringLiteral("device:Nicht vorhanden");
    if (!(SettingsDialog{probe, this}.settings() == probe)) {
        return QStringLiteral("Einstellungen: der Dialog gibt nicht zurück, was man ihm gibt (Gerät)");
    }
    probe.audioSource = QStringLiteral("file:/tmp/test.wav");
    if (!(SettingsDialog{probe, this}.settings() == probe)) {
        return QStringLiteral("Einstellungen: der Dialog gibt nicht zurück, was man ihm gibt (Datei)");
    }
    probe.audioSource = QStringLiteral("default");
    if (!(SettingsDialog{probe, this}.settings() == probe)) {
        return QStringLiteral("Einstellungen: der Dialog gibt nicht zurück, was man ihm gibt (Standardgerät)");
    }

    // Eine Karte, die es nicht gibt, scheitert und lässt die alte Wahl stehen; Loopback gelingt.
    const AppSettings before = appSettings_;
    AppSettings bad = before;
    bad.outputInterface = QStringLiteral("203.0.113.9");  // TEST-NET-3, nie lokal
    if (applySettings(bad, false).isEmpty() || appSettings_.outputInterface != before.outputInterface) {
        return QStringLiteral("Einstellungen: eine nicht vorhandene Karte wurde nicht abgelehnt");
    }
    AppSettings loopback = before;
    loopback.outputInterface = QStringLiteral("127.0.0.1");
    if (const QString error = applySettings(loopback, false); !error.isEmpty() || appSettings_.outputInterface != loopback.outputInterface) {
        return QStringLiteral("Einstellungen: die Karte 127.0.0.1 lässt sich nicht wählen: ") + error;
    }
    tick();  // das Senden läuft über die gewählte Karte weiter
    if (const QString error = applySettings(before, false); !error.isEmpty() || !(appSettings_ == before)) {
        return QStringLiteral("Einstellungen: der alte Stand lässt sich nicht wiederherstellen: ") + error;
    }
    return {};
}

}  // namespace whitehole::studio
