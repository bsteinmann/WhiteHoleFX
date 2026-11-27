// live_panel_cells.cpp: die Felder des Live-Rasters (Aufbau, Beschriftung, Belegen, Flash-Einstellung je Feld) als Teil
// von LivePanel.
//
// Warum es diese Datei gibt: Das Raster ist fest, die Felder werden frei mit Looks belegt (Wunsch vom 25.11.,
// docs/entwurf-live-raster.md). Flash, Einblend- und Rückblendzeit gehören zum *Feld*, damit derselbe Look einmal als Flash
// und einmal zum Umschalten liegen kann. Die Felder stehen im Projekt (project::LiveGrid); das Panel zeigt sie und ändert sie
// nur über den Player (setLiveCell). Eigene Datei, damit live_panel.cpp klein bleibt.
#include <QActionGroup>
#include <QCheckBox>
#include <QGridLayout>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <algorithm>
#include <string>
#include <utility>

#include "live_panel.hpp"
#include "look_button.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

}  // namespace

QString LivePanel::buttonText(int index) const {
    const int look = player_.cellLook(index);
    if (look < 0) {
        return {};
    }
    const project::LiveCell& cell = player_.liveGrid().cells[static_cast<std::size_t>(index)];
    QString text = index < 9 ? QStringLiteral("%1   ").arg(index + 1) : QString{};
    text += fromStd(player_.looks()[static_cast<std::size_t>(look)].name);
    if (cell.trigger == project::LookTrigger::Flash) {
        text += tr("  · Flash");
    }
    return text;
}

QIcon LivePanel::thumbnailIcon(int index) const {
    const int look = player_.cellLook(index);
    if (look < 0 || !player_.looks()[static_cast<std::size_t>(look)].thumbnail) {
        return {};
    }
    const project::LookThumbnail& thumb = *player_.looks()[static_cast<std::size_t>(look)].thumbnail;
    const QImage image{thumb.rgb.data(), static_cast<int>(thumb.width), static_cast<int>(thumb.height),
                       static_cast<int>(thumb.width) * 3, QImage::Format_RGB888};
    // Das Bild liegt nur im Speicher des Looks; die Pixmap kopiert es (copy), bevor der Look sich ändern kann.
    return QIcon{QPixmap::fromImage(image.copy())};
}

QString LivePanel::flashInText(int index) const {
    const project::LiveCell& cell = player_.liveGrid().cells[static_cast<std::size_t>(index)];
    return cell.flashInSeconds ? tr("Rein: %1 s").arg(*cell.flashInSeconds, 0, 'f', 1) : tr("Rein: Look-Zeit");
}

QString LivePanel::returnText(int index) const {
    const project::LiveCell& cell = player_.liveGrid().cells[static_cast<std::size_t>(index)];
    return cell.returnSeconds ? tr("Zurück: %1 s").arg(*cell.returnSeconds, 0, 'f', 1) : tr("Zurück: Look-Zeit");
}

void LivePanel::syncLookOptions(int index) {
    const auto i = static_cast<std::size_t>(index);
    if (index < 0 || i >= buttons_.size() || i >= player_.liveGrid().cells.size()) {
        return;
    }
    const bool occupied = player_.cellLook(index) >= 0;
    const bool flash = occupied && player_.liveGrid().cells[i].trigger == project::LookTrigger::Flash;
    buttons_[i]->setText(buttonText(index));
    {
        const QSignalBlocker blocker{flashChecks_[i]};
        flashChecks_[i]->setChecked(flash);
    }
    flashChecks_[i]->setEnabled(occupied);
    inButtons_[i]->setText(flashInText(index));
    inButtons_[i]->setEnabled(flash);  // ohne Flash gibt es weder etwas ein- noch zurückzublenden
    returnButtons_[i]->setText(returnText(index));
    returnButtons_[i]->setEnabled(flash);
}

bool LivePanel::changeCell(int index, const project::LiveCell& value) {
    if (const Result<void> r = player_.setLiveCell(index, value); !r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return false;
    }
    return true;
}

bool LivePanel::setLookTrigger(int index, project::LookTrigger trigger, std::optional<double> returnSeconds) {
    if (player_.cellLook(index) < 0) {
        return false;
    }
    project::LiveCell cell = player_.liveGrid().cells[static_cast<std::size_t>(index)];
    const bool becomesFlash = trigger == project::LookTrigger::Flash && cell.trigger != project::LookTrigger::Flash;
    cell.trigger = trigger;
    cell.returnSeconds = returnSeconds;
    if (becomesFlash) {
        project::fillFlashDefaults(cell);  // neu auf Flash: 0,1 s Ein- und Zurückblenden, solange keine eigene Zeit gesetzt ist
    }
    if (!changeCell(index, cell)) {
        return false;
    }
    syncLookOptions(index);  // Knopftext "· Flash", Haken und Zurückblend-Knopf
    return true;
}

bool LivePanel::setLookFlashIn(int index, std::optional<double> seconds) {
    if (player_.cellLook(index) < 0) {
        return false;
    }
    project::LiveCell cell = player_.liveGrid().cells[static_cast<std::size_t>(index)];
    cell.flashInSeconds = seconds;
    if (!changeCell(index, cell)) {
        return false;
    }
    syncLookOptions(index);
    return true;
}

bool LivePanel::assignLook(int index, const std::string& lookName) {
    const std::vector<project::Look>& looks = player_.looks();
    const auto found = std::find_if(looks.begin(), looks.end(), [&](const project::Look& l) { return l.name == lookName; });
    if (found == looks.end() || index < 0 || index >= buttonCount()) {
        return false;
    }
    project::LiveCell cell = player_.liveGrid().cells[static_cast<std::size_t>(index)];
    if (cell.empty()) {
        // Ein leeres Feld startet mit den Vorgaben des Looks (Umschalten oder Flash und seine Zeiten)
        cell.trigger = found->trigger;
        cell.flashInSeconds = found->flashInSeconds;
        cell.returnSeconds = found->returnSeconds;
    }  // ein belegtes Feld behält seinen Modus und seine Zeiten, nur der Look wechselt
    cell.look = lookName;
    if (!changeCell(index, cell)) {
        return false;
    }
    rebuild();
    return true;
}

bool LivePanel::clearCell(int index) {
    if (!changeCell(index, project::LiveCell{})) {
        return false;
    }
    rebuild();
    return true;
}

void LivePanel::askFlashSeconds(int index, bool returnTime) {
    const int look = player_.cellLook(index);
    if (look < 0) {
        return;
    }
    const project::LiveCell& cell = player_.liveGrid().cells[static_cast<std::size_t>(index)];
    const std::optional<double> current = returnTime ? cell.returnSeconds : cell.flashInSeconds;
    bool ok = false;
    const double seconds = QInputDialog::getDouble(
        this, returnTime ? tr("Zurückblenden") : tr("Einblenden"),
        returnTime ? tr("Zeit zum Zurückblenden beim Loslassen (s):") : tr("Zeit zum Einblenden beim Drücken (s):"),
        current.value_or(player_.looks()[static_cast<std::size_t>(look)].fadeSeconds), 0.0, 600.0, 1, &ok);
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
    if (index < 0 || index >= buttonCount()) {
        return menu;
    }
    const int lookIndex = player_.cellLook(index);
    const bool occupied = lookIndex >= 0;
    // Look wählen: bei einem leeren Feld "hierher legen", sonst wechseln (Modus und Zeiten des Feldes bleiben)
    QMenu* pick = menu->addMenu(occupied ? tr("Look wechseln") : tr("Look hierher legen"));
    for (const project::Look& look : player_.looks()) {
        QAction* action = pick->addAction(fromStd(look.name));
        action->setCheckable(true);
        action->setChecked(occupied && look.name == player_.looks()[static_cast<std::size_t>(lookIndex)].name);
        const std::string name = look.name;
        connect(action, &QAction::triggered, this, [this, index, name]() { (void)assignLook(index, name); });
    }
    pick->setEnabled(!player_.looks().empty());
    if (!occupied) {
        return menu;
    }
    QAction* clear = menu->addAction(tr("Feld leeren"));
    connect(clear, &QAction::triggered, this, [this, index]() { (void)clearCell(index); });
    menu->addSeparator();
    const project::Look& look = player_.looks()[static_cast<std::size_t>(lookIndex)];
    const project::LiveCell& cell = player_.liveGrid().cells[static_cast<std::size_t>(index)];
    const bool flash = cell.trigger == project::LookTrigger::Flash;
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
        (void)setLookTrigger(index, project::LookTrigger::Flash, player_.liveGrid().cells[static_cast<std::size_t>(index)].returnSeconds);
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
             cell.flashInSeconds ? tr("Einblenden mit eigener Zeit (%1 s) …").arg(*cell.flashInSeconds, 0, 'f', 1)
                                 : tr("Einblenden mit eigener Zeit …"),
             cell.flashInSeconds.has_value(), false);
    addTimes(tr("Zurückblenden mit der Zeit des Looks (%1 s)").arg(look.fadeSeconds, 0, 'f', 1),
             cell.returnSeconds ? tr("Zurückblenden mit eigener Zeit (%1 s) …").arg(*cell.returnSeconds, 0, 'f', 1)
                                : tr("Zurückblenden mit eigener Zeit …"),
             cell.returnSeconds.has_value(), true);
    return menu;
}

void LivePanel::rebuild() {
    for (QWidget* cell : cells_) {
        grid_->removeWidget(cell);
        delete cell;  // nimmt Knopf, Flash-Haken und Zurückblend-Knopf mit
    }
    cells_.clear();
    buttons_.clear();
    optionRows_.clear();
    flashChecks_.clear();
    inButtons_.clear();
    returnButtons_.clear();
    // Ältere Projekte haben noch kein Raster: es entsteht aus den Looks (mit mehr Spalten, wenn sie sonst nicht passen)
    for (int columns = columns_; columns <= 16 && !player_.liveGrid().defined(); ++columns) {
        (void)player_.ensureLiveGrid(columns, rows_);
    }
    const project::LiveGrid& gridData = player_.liveGrid();
    if (gridData.defined() && (gridData.columns != columns_ || gridData.rows != rows_)) {
        setDefaultGrid(gridData.columns, gridData.rows);  // die Größe des Projekts gilt
    }
    for (int index = 0; index < gridData.cellCount(); ++index) {
        // Eine Zelle je Feld: der große Knopf, darunter (nur bei offenen Einstellungen) Flash und Zurückblenden
        auto* cell = new QWidget{gridHost_};
        auto* cellLayout = new QVBoxLayout{cell};
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(4);
        auto* button = new LookButton{buttonText(index), cell};
        button->setMinimumHeight(96);
        if (const QIcon icon = thumbnailIcon(index); !icon.isNull()) {
            button->setIcon(icon);  // LookButton zeichnet das Bild selbst, so groß wie der Knopf erlaubt
        }
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        const int look = player_.cellLook(index);
        button->setToolTip(look >= 0 ? tr("Überblendzeit %1 s. Rechtsklick: Look, Modus und Zeiten ändern")
                                           .arg(player_.looks()[static_cast<std::size_t>(look)].fadeSeconds, 0, 'f', 1)
                                     : tr("Leeres Feld. Klick oder Rechtsklick: Look hierher legen"));
        // Drücken und Loslassen statt Klick: ein Flash-Feld läuft, solange gehalten (live_panel_looks.cpp). Ein leeres
        // Feld öffnet beim Drücken das Menü zum Belegen.
        connect(button, &QPushButton::pressed, this, [this, index, button]() {
            if (player_.cellLook(index) < 0) {
                QMenu* menu = lookMenu(index);
                menu->exec(button->mapToGlobal(QPoint{0, button->height()}));
                menu->deleteLater();
                return;
            }
            press(index);
        });
        connect(button, &QPushButton::released, this, [this, index]() { release(index); });
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button, &QWidget::customContextMenuRequested, this, [this, index, button](const QPoint& at) {
            QMenu* menu = lookMenu(index);
            menu->exec(button->mapToGlobal(at));
            menu->deleteLater();
        });
        cellLayout->addWidget(button, 1);

        auto* row = new QWidget{cell};
        auto* rowLayout = new QGridLayout{row};
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto* flash = new QCheckBox{tr("Flash"), row};
        flash->setToolTip(tr("Das Feld läuft nur, solange der Knopf (oder das Pad) gehalten wird"));
        auto* ins = new QPushButton{row};
        ins->setToolTip(tr("Zeit zum Einblenden beim Drücken"));
        auto* returns = new QPushButton{row};
        returns->setToolTip(tr("Zeit zum Zurückblenden beim Loslassen"));
        rowLayout->addWidget(flash, 0, 0, 1, 2);
        rowLayout->addWidget(ins, 1, 0);
        rowLayout->addWidget(returns, 1, 1);
        row->setVisible(settingsShown_);
        cellLayout->addWidget(row);
        // clicked statt toggled: ein vom Player nachgezogener Haken (syncLookOptions) löst nichts aus
        connect(flash, &QCheckBox::clicked, this, [this, index](bool on) {
            if (player_.cellLook(index) >= 0) {
                const std::optional<double> keep = player_.liveGrid().cells[static_cast<std::size_t>(index)].returnSeconds;
                (void)setLookTrigger(index, on ? project::LookTrigger::Flash : project::LookTrigger::Toggle,
                                     on ? keep : std::optional<double>{});
            }
            syncLookOptions(index);  // scheitert das Setzen, springt der Haken zurück
        });
        for (QPushButton* timeButton : {ins, returns}) {  // beide Zeiten: das Menü des Feldes (Zeiten zum Wählen)
            connect(timeButton, &QPushButton::clicked, this, [this, index, timeButton]() {
                QMenu* menu = lookMenu(index);
                menu->exec(timeButton->mapToGlobal(QPoint{0, timeButton->height()}));
                menu->deleteLater();
            });
        }

        grid_->addWidget(cell, index / gridData.columns, index % gridData.columns);
        cells_.push_back(cell);
        buttons_.push_back(button);
        optionRows_.push_back(row);
        flashChecks_.push_back(flash);
        inButtons_.push_back(ins);
        returnButtons_.push_back(returns);
        syncLookOptions(index);
    }
    // Festes Raster: alle Spalten und Zeilen gleich groß, nicht belegte Felder bleiben leer
    for (int c = 0; c < std::max(grid_->columnCount(), gridData.columns); ++c) {
        grid_->setColumnStretch(c, c < gridData.columns ? 1 : 0);
    }
    for (int r = 0; r < std::max(grid_->rowCount(), gridData.rows); ++r) {
        grid_->setRowStretch(r, r < gridData.rows ? 1 : 0);
    }
    refresh();
}

}  // namespace whitehole::studio
