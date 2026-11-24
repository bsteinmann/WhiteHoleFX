// live_panel_looks.cpp: Drücken/Loslassen der Look-Knöpfe, Flash-Einstellung je Look, Einstellungen ein-/ausblenden
// und "Steuerung belegen" im Live-Fenster (Teil von LivePanel).
//
// Warum es diese Datei gibt: Ob ein Look umschaltet oder flasht, legt der Look fest (Player::setLookTrigger,
// Projektdatei `trigger flash`), damit es für alle Eingänge gleich gilt - Knöpfe hier, Pads am MIDI-Controller, später DMX.
// Das Live-Fenster zeigt das unter jedem Look als Haken "Flash" (sichtbar, nicht nur im Rechtsklick-Menü) und bietet, wie
// der Reiter "Steuerung", Lernen-Knöpfe für die Steuerfunktionen. Beides blendet die Schaltfläche "Einstellungen" ein
// und aus, damit die Auftrittsansicht aufgeräumt bleibt. Eigene Datei, damit live_panel.cpp klein bleibt.
// Entwurf: docs/entwurf-midi.md, Abschnitte 5 und 6.
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

QIcon LivePanel::thumbnailIcon(int index) const {
    const project::Look& look = player_.looks()[static_cast<std::size_t>(index)];
    if (!look.thumbnail) {
        return {};
    }
    const project::LookThumbnail& thumb = *look.thumbnail;
    const QImage image{thumb.rgb.data(), static_cast<int>(thumb.width), static_cast<int>(thumb.height),
                       static_cast<int>(thumb.width) * 3, QImage::Format_RGB888};
    // Das Bild liegt nur im Speicher des Looks; die Pixmap kopiert es (copy), bevor der Look sich ändern kann.
    return QIcon{QPixmap::fromImage(image.copy())};
}

QString LivePanel::flashInText(int index) const {
    const project::Look& look = player_.looks()[static_cast<std::size_t>(index)];
    return look.flashInSeconds ? tr("Rein: %1 s").arg(*look.flashInSeconds, 0, 'f', 1) : tr("Rein: Look-Zeit");
}

QString LivePanel::returnText(int index) const {
    const project::Look& look = player_.looks()[static_cast<std::size_t>(index)];
    return look.returnSeconds ? tr("Zurück: %1 s").arg(*look.returnSeconds, 0, 'f', 1) : tr("Zurück: Look-Zeit");
}

void LivePanel::syncLookOptions(int index) {
    const auto i = static_cast<std::size_t>(index);
    if (index < 0 || i >= player_.looks().size() || i >= buttons_.size()) {
        return;
    }
    const bool flash = player_.looks()[i].trigger == project::LookTrigger::Flash;
    buttons_[i]->setText(buttonText(index));
    {
        const QSignalBlocker blocker{flashChecks_[i]};
        flashChecks_[i]->setChecked(flash);
    }
    inButtons_[i]->setText(flashInText(index));
    inButtons_[i]->setEnabled(flash);  // ohne Flash gibt es weder etwas ein- noch zurückzublenden
    returnButtons_[i]->setText(returnText(index));
    returnButtons_[i]->setEnabled(flash);
}

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
    syncLookOptions(index);  // Knopftext "· Flash", Haken und Zurückblend-Knopf
    return true;
}

bool LivePanel::setLookFlashIn(int index, std::optional<double> seconds) {
    const Result<void> r = player_.setLookFlashIn(index, seconds);
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return false;
    }
    syncLookOptions(index);
    return true;
}

void LivePanel::askFlashSeconds(int index, bool returnTime) {
    const project::Look& look = player_.looks()[static_cast<std::size_t>(index)];
    const std::optional<double> current = returnTime ? look.returnSeconds : look.flashInSeconds;
    bool ok = false;
    const double seconds = QInputDialog::getDouble(
        this, returnTime ? tr("Zurückblenden") : tr("Einblenden"),
        returnTime ? tr("Zeit zum Zurückblenden beim Loslassen (s):") : tr("Zeit zum Einblenden beim Drücken (s):"),
        current.value_or(look.fadeSeconds), 0.0, 600.0, 1, &ok);
    if (!ok) {
        return;
    }
    if (returnTime) {
        (void)setLookTrigger(index, project::LookTrigger::Flash, seconds);
    } else {
        (void)setLookFlashIn(index, seconds);
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
    // Zwei Zeiten für Flash: beim Drücken einblenden und beim Loslassen zurückblenden; jeweils Zeit des Looks oder eigene
    const auto addTimes = [this, menu, index, flash](const QString& byLookText, const QString& ownText, bool hasOwn, bool returnTime) {
        menu->addSeparator();
        auto* group = new QActionGroup{menu};
        QAction* byLook = menu->addAction(byLookText);
        QAction* own = menu->addAction(ownText);
        for (QAction* action : {byLook, own}) {
            action->setCheckable(true);
            action->setEnabled(flash);
            group->addAction(action);
        }
        (hasOwn ? own : byLook)->setChecked(true);
        connect(byLook, &QAction::triggered, this, [this, index, returnTime]() {
            if (returnTime) {
                (void)setLookTrigger(index, project::LookTrigger::Flash, std::nullopt);
            } else {
                (void)setLookFlashIn(index, std::nullopt);
            }
        });
        connect(own, &QAction::triggered, this, [this, index, returnTime]() { askFlashSeconds(index, returnTime); });
    };
    addTimes(tr("Einblenden mit der Zeit des Looks (%1 s)").arg(look.fadeSeconds, 0, 'f', 1),
             look.flashInSeconds ? tr("Einblenden mit eigener Zeit (%1 s) …").arg(*look.flashInSeconds, 0, 'f', 1)
                                 : tr("Einblenden mit eigener Zeit …"),
             look.flashInSeconds.has_value(), false);
    addTimes(tr("Zurückblenden mit der Zeit des Looks (%1 s)").arg(look.fadeSeconds, 0, 'f', 1),
             look.returnSeconds ? tr("Zurückblenden mit eigener Zeit (%1 s) …").arg(*look.returnSeconds, 0, 'f', 1)
                                : tr("Zurückblenden mit eigener Zeit …"),
             look.returnSeconds.has_value(), true);
    return menu;
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
