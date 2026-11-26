// live_panel_looks.cpp: Drücken/Loslassen der Felder, Einstellungen ein-/ausblenden und "Steuerung belegen" im
// Live-Fenster (Teil von LivePanel).
//
// Warum es diese Datei gibt: Ob ein Feld umschaltet oder flasht, legt das Feld fest (Player::liveCellDown/-Up, Projektdatei
// `cell ... flash`), damit es für alle Eingänge gleich gilt - Knöpfe hier, Pads am MIDI-Controller, DMX. Das Live-Fenster
// zeigt das unter jedem Feld als Haken "Flash" und bietet, wie der Reiter "Steuerung", Lernen-Knöpfe für die
// Steuerfunktionen. Beides blendet die Schaltfläche "Einstellungen" ein und aus, damit die Auftrittsansicht aufgeräumt
// bleibt. Die Felder selbst stehen in live_panel_cells.cpp. Entwurf: docs/entwurf-midi.md, Abschnitte 5 und 6, und
// docs/entwurf-live-raster.md.
#include <QActionGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>

#include <string>
#include <utility>

#include "live_panel.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

}  // namespace

QCheckBox* LivePanel::flashCheck(int index) const {
    return index >= 0 && index < static_cast<int>(flashChecks_.size()) ? flashChecks_[static_cast<std::size_t>(index)] : nullptr;
}

void LivePanel::setSettingsShown(bool shown) {
    if (shown == settingsShown_) {
        return;
    }
    settingsShown_ = shown;
    settingsBar_->setVisible(shown);
    for (QWidget* row : optionRows_) {
        row->setVisible(shown);
    }
    {
        const QSignalBlocker blocker{settingsToggle_};  // der Knopf löst das hier aus; der Aufruf von außen zieht ihn nach
        settingsToggle_->setChecked(shown);
    }
    Q_EMIT settingsToggled(shown);
}

bool LivePanel::press(int index) {
    if (player_.cuesRunning()) {
        Q_EMIT failed(tr("Die Cuelist läuft und schaltet die Looks selbst. Erst in der Cuelist auf Stopp drücken."));
        return false;
    }
    const Result<void> r = player_.liveCellDown(index, fadeOverride(), useGo());
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return false;
    }
    refresh();
    return true;
}

void LivePanel::release(int index) {
    if (const Result<void> r = player_.liveCellUp(index); !r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
    }
    refresh();
}

void LivePanel::setLearning(std::shared_ptr<midi::MidiStore> midi, std::shared_ptr<const input::DmxStore> dmx, LearnHandler handler) {
    delete learnRow_->layout();
    for (QWidget* child : learnRow_->findChildren<QWidget*>(QString{}, Qt::FindDirectChildrenOnly)) {
        delete child;
    }
    auto* row = new QHBoxLayout{learnRow_};
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(new QLabel{tr("Steuerung belegen:"), learnRow_});
    for (const runtime::LiveFunction function : runtime::kLiveFunctions) {
        row->addWidget(new QLabel{liveFunctionName(function), learnRow_});
        row->addWidget(makeLiveLearnButton(function, midi, dmx,
                                           [handler, function](const LearnedSource& found) {
                                               if (handler) {
                                                   handler(function, found);
                                               }
                                           },
                                           learnRow_));
    }
    row->addStretch(1);
    learnRow_->setVisible(true);
}

}  // namespace whitehole::studio
