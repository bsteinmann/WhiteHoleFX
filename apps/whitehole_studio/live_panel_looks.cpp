// live_panel_looks.cpp: Drücken/Loslassen der Look-Knöpfe, Flash-Einstellung je Look und "Steuerung lernen" im
// Live-Fenster (Teil von LivePanel).
//
// Warum es diese Datei gibt: Ob ein Look umschaltet oder flasht, legt der Look fest (Player::setLookTrigger,
// Projektdatei `trigger flash`), damit es für alle Eingänge gleich gilt - Knöpfe hier, Pads am MIDI-Controller, später DMX.
// Das Live-Fenster bietet dafür das Rechtsklick-Menü und, wie der Reiter "Steuerung", Lernen-Knöpfe für die
// Steuerfunktionen. Eigene Datei, damit live_panel.cpp klein bleibt. Entwurf: docs/entwurf-midi.md, Abschnitte 5 und 6.
#include <QActionGroup>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QPushButton>

#include <string>
#include <utility>

#include "live_panel.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

}  // namespace

QString LivePanel::buttonText(int index) const {
    const auto& looks = player_.looks();
    const project::Look& look = looks[static_cast<std::size_t>(index)];
    QString text = index < 9 ? QStringLiteral("%1   ").arg(index + 1) : QString{};
    text += fromStd(look.name);
    if (look.trigger == project::LookTrigger::Flash) {
        text += tr("  · Flash");
    }
    return text;
}

bool LivePanel::press(int index) {
    if (player_.cuesRunning()) {
        Q_EMIT failed(tr("Die Cuelist läuft und schaltet die Looks selbst. Erst in der Cuelist auf Stopp drücken."));
        return false;
    }
    const Result<void> r = player_.liveDown(index, fadeOverride(), useGo());
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return false;
    }
    refresh();
    return true;
}

void LivePanel::release(int index) {
    if (const Result<void> r = player_.liveUp(index); !r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
    }
    refresh();
}

bool LivePanel::setLookTrigger(int index, project::LookTrigger trigger, std::optional<double> returnSeconds) {
    const Result<void> r = player_.setLookTrigger(index, trigger, returnSeconds);
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return false;
    }
    if (index >= 0 && index < buttonCount()) {
        buttons_[static_cast<std::size_t>(index)]->setText(buttonText(index));
    }
    return true;
}

void LivePanel::askReturnSeconds(int index) {
    const project::Look& look = player_.looks()[static_cast<std::size_t>(index)];
    bool ok = false;
    const double seconds = QInputDialog::getDouble(this, tr("Zurückblenden"), tr("Zeit zum Zurückblenden beim Loslassen (s):"),
                                                   look.returnSeconds.value_or(look.fadeSeconds), 0.0, 600.0, 1, &ok);
    if (ok) {
        (void)setLookTrigger(index, project::LookTrigger::Flash, seconds);
    }
}

QMenu* LivePanel::lookMenu(int index) {
    auto* menu = new QMenu{this};
    if (index < 0 || index >= static_cast<int>(player_.looks().size())) {
        return menu;
    }
    const project::Look& look = player_.looks()[static_cast<std::size_t>(index)];
    const bool flash = look.trigger == project::LookTrigger::Flash;
    auto* modes = new QActionGroup{menu};
    QAction* toggle = menu->addAction(tr("Umschalten (bleibt an)"));
    QAction* flashing = menu->addAction(tr("Flashen (nur solange gedrückt)"));
    for (QAction* action : {toggle, flashing}) {
        action->setCheckable(true);
        modes->addAction(action);
    }
    (flash ? flashing : toggle)->setChecked(true);
    connect(toggle, &QAction::triggered, this, [this, index]() { (void)setLookTrigger(index, project::LookTrigger::Toggle, std::nullopt); });
    connect(flashing, &QAction::triggered, this, [this, index]() {
        (void)setLookTrigger(index, project::LookTrigger::Flash, player_.looks()[static_cast<std::size_t>(index)].returnSeconds);
    });
    menu->addSeparator();
    auto* returns = new QActionGroup{menu};
    QAction* lookTime = menu->addAction(tr("Zurückblenden mit der Zeit des Looks (%1 s)").arg(look.fadeSeconds, 0, 'f', 1));
    QAction* ownTime = menu->addAction(look.returnSeconds ? tr("Zurückblenden mit eigener Zeit (%1 s) …").arg(*look.returnSeconds, 0, 'f', 1)
                                                          : tr("Zurückblenden mit eigener Zeit …"));
    for (QAction* action : {lookTime, ownTime}) {
        action->setCheckable(true);
        action->setEnabled(flash);
        returns->addAction(action);
    }
    (look.returnSeconds ? ownTime : lookTime)->setChecked(true);
    connect(lookTime, &QAction::triggered, this, [this, index]() { (void)setLookTrigger(index, project::LookTrigger::Flash, std::nullopt); });
    connect(ownTime, &QAction::triggered, this, [this, index]() { askReturnSeconds(index); });
    return menu;
}

void LivePanel::setLearning(std::shared_ptr<midi::MidiStore> midi, std::shared_ptr<const input::DmxStore> dmx, LearnHandler handler) {
    delete learnRow_->layout();
    for (QWidget* child : learnRow_->findChildren<QWidget*>(QString{}, Qt::FindDirectChildrenOnly)) {
        delete child;
    }
    auto* row = new QHBoxLayout{learnRow_};
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(new QLabel{tr("Steuerung lernen:"), learnRow_});
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
