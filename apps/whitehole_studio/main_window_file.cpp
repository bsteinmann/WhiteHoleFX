// main_window_file.cpp: Datei-Menü des Hauptfensters (Neu, Öffnen, Neu laden, Speichern).
//
// Eigene Datei, damit main_window.cpp übersichtlich bleibt: Hier steht alles, was ein Projekt
// als Ganzes lädt oder schreibt. Die Bedienelemente selbst baut MainWindow::buildActions.
#include <QFileDialog>
#include <QFileInfo>
#include <QStatusBar>

#include "main_window.hpp"
#include "node_editor.hpp"
#include "layout_panel.hpp"
#include "parameter_panel.hpp"
#include "preview_widget.hpp"
#include "test_panel.hpp"

namespace whitehole::studio {

namespace {

QString toQString(const std::string& text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// Ausgangspunkt für ein neues Projekt: eine 16x8-Matrix mit Art-Net auf den eigenen Rechner
// (127.0.0.1, also ungefährlich) und ein Farbverlauf als Effekt. Dieselben Zeilen wie in
// examples/wand.whfx, nur ohne Strip und Dreher; so sieht man sofort ein Bild und kann Nodes
// und Layout nach Wunsch ändern (Ziel-IP und Universen unter "Layout").
constexpr const char* kNewProject = R"(whitehole-project 1
settings fps 40

destination 0 127.0.0.1

fixture Matrix
  matrix 16 8 pitch 1 1 start top-left first rows serpentine
  patch artnet destination 0 universe 0 channel 1 order RGB gamma 2.2 brightness 1
end

node verlauf wh.generator.gradient
  set colorA 0 0.2 1 1
  set colorB 1 0.3 0 1
  at 0 0
end

node ausgang wh.output.canvas
  at 300 0
end
connect verlauf.out -> ausgang.source
output ausgang.out
)";

}  // namespace

void MainWindow::afterLoad(const QString& message) {
    preview_->setMessage({});
    editor_->reset();  // zuerst: setzt die Auswahl zurück, die refreshParameters abfragt
    refreshParameters();
    layoutPanel_->rebuild();
    testPanel_->reset();  // der Player hat das Testbild beim Laden schon verworfen
    resetLive();          // Look-Liste neu, Live-Modus (falls aktiv) beendet
    updateTitle();
    statusBar()->showMessage(message, 5000);
    updateActions();
    refreshImage();
}

bool MainWindow::openPath(const QString& path, bool interactive) {
    if (interactive && !confirmDiscard()) {
        return false;
    }
    timer_.stop();
    const Result<void> r = player_->load(path.toUtf8().toStdString());
    if (!r.ok()) {
        reportError(tr("Projekt laden"), r.error().message, interactive);
        if (!QFileInfo::exists(path)) {
            forgetRecent(path);  // die Datei gibt es nicht mehr: nicht weiter anbieten
        }
        updateActions();
        return false;
    }
    rememberRecent(path);
    afterLoad(tr("Geladen: %1").arg(path));
    return true;
}

bool MainWindow::newProject(bool interactive) {
    if (interactive && !confirmDiscard()) {
        return false;
    }
    const bool wasRunning = player_->status().running;
    timer_.stop();
    const Result<void> r = player_->loadText(kNewProject);
    if (!r.ok()) {
        reportError(tr("Neues Projekt"), r.error().message, interactive);
        updateActions();
        return false;
    }
    afterLoad(tr("Neues Projekt. Mit Strg+S speichern."));
    if (wasRunning) {
        startPlayback(interactive);
    }
    return true;
}

void MainWindow::onNew() { newProject(true); }

void MainWindow::onOpen() {
    const QString start = player_->status().path.empty() ? QString{} : toQString(player_->status().path);
    const QString path = QFileDialog::getOpenFileName(this, tr("Projekt öffnen"), start,
                                                      tr("WhiteHoleFX-Projekte (*.whfx);;Alle Dateien (*)"));
    if (!path.isEmpty()) {
        openPath(path);
    }
}

void MainWindow::onReload() {
    const std::string path = player_->status().path;
    if (path.empty()) {
        return;
    }
    const bool wasRunning = player_->status().running;
    if (openPath(toQString(path)) && wasRunning) {
        startPlayback();
    }
}

void MainWindow::onSave() {
    const std::string path = player_->status().path;
    if (path.empty()) {
        onSaveAs();  // noch ohne Datei: nach dem Namen fragen
        return;
    }
    const Result<void> r = player_->save(path);
    if (!r.ok()) {
        reportError(tr("Speichern"), r.error().message, true);
        return;
    }
    updateTitle();
    statusBar()->showMessage(tr("Gespeichert: %1").arg(toQString(path)), 5000);
    updateActions();
}

void MainWindow::onSaveAs() {
    const QString start = player_->status().path.empty() ? QString{} : toQString(player_->status().path);
    QString path = QFileDialog::getSaveFileName(this, tr("Projekt speichern unter"), start,
                                                tr("WhiteHoleFX-Projekte (*.whfx)"));
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo{path}.suffix().isEmpty()) {
        path += QStringLiteral(".whfx");
    }
    const Result<void> r = player_->save(path.toUtf8().toStdString());
    if (!r.ok()) {
        reportError(tr("Speichern"), r.error().message, true);
        return;
    }
    rememberRecent(path);
    updateTitle();
    statusBar()->showMessage(tr("Gespeichert: %1").arg(path), 5000);
    updateActions();
}

}  // namespace whitehole::studio
