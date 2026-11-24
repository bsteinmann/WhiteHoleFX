// live_panel.cpp: große Look-Knöpfe, GO, Überblendzeit, Fortschritt.
#include "live_panel.hpp"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QShortcut>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

constexpr const char* kIdle = "#2d2f36";
constexpr const char* kLive = "#1f9d4c";
constexpr const char* kArmed = "#d9822b";
constexpr const char* kIncoming = "#c9b21e";

}  // namespace

QString LivePanel::styleFor(const QString& color) {
    return QStringLiteral(
               "QPushButton { background: %1; color: white; font-size: 16px; font-weight: bold; border-radius: 6px; "
               "padding: 10px; } QPushButton:pressed { background: #555; }")
        .arg(color);
}

LivePanel::LivePanel(runtime::Player& player, QWidget* parent) : QWidget{parent}, player_{player} {
    auto* layout = new QVBoxLayout{this};
    layout->setContentsMargins(8, 8, 8, 8);

    nowLabel_ = new QLabel{this};
    nowLabel_->setStyleSheet(QStringLiteral("font-size: 22px; font-weight: bold;"));
    nextLabel_ = new QLabel{this};
    nextLabel_->setStyleSheet(QStringLiteral("font-size: 14px; color: #d9822b;"));
    // Kopfzeile: links Name und "Bereit", rechts die Schaltfläche für die Einstellungen neben "Zurück zum Editor"
    auto* header = new QHBoxLayout;
    auto* titles = new QVBoxLayout;
    titles->addWidget(nowLabel_);
    titles->addWidget(nextLabel_);
    header->addLayout(titles, 1);
    settingsToggle_ = new QPushButton{tr("Einstellungen"), this};
    settingsToggle_->setCheckable(true);
    settingsToggle_->setToolTip(tr("Blendet Flash je Look, die Belegung von MIDI und DMX und die Menüleiste ein oder aus"));
    auto* leave = new QPushButton{tr("Zurück zum Editor (Esc)"), this};
    header->addWidget(settingsToggle_, 0, Qt::AlignTop);
    header->addWidget(leave, 0, Qt::AlignTop);
    layout->addLayout(header);

    progress_ = new QProgressBar{this};
    progress_->setRange(0, 1000);
    progress_->setTextVisible(false);
    progress_->setFixedHeight(10);
    layout->addWidget(progress_);

    // Einstellungsleiste: Standard ausgeblendet; die Zeile "Steuerung belegen" füllt setLearning
    settingsBar_ = new QWidget{this};
    auto* barLayout = new QVBoxLayout{settingsBar_};
    barLayout->setContentsMargins(0, 0, 0, 0);
    auto* hint = new QLabel{tr("Unter jedem Look: Flash (läuft nur, solange der Knopf gehalten wird) und die Zeit zum Zurückblenden."),
                            settingsBar_};
    hint->setWordWrap(true);
    barLayout->addWidget(hint);
    learnRow_ = new QWidget{settingsBar_};
    barLayout->addWidget(learnRow_);
    auto* columnsRow = new QHBoxLayout;
    columnsRow->addWidget(new QLabel{tr("Rasterbreite (Spalten):"), settingsBar_});
    columnsBox_ = new QSpinBox{settingsBar_};
    columnsBox_->setRange(1, 16);
    columnsBox_->setValue(columns_);
    columnsBox_->setToolTip(tr("Wie viele Look-Knöpfe nebeneinander stehen; mehr Spalten brauchen weniger Höhe"));
    columnsRow->addWidget(columnsBox_);
    columnsRow->addSpacing(16);
    splitCheck_ = new QCheckBox{tr("Vorschau teilen"), settingsBar_};
    splitCheck_->setObjectName(QStringLiteral("liveSplitPreview"));
    splitCheck_->setChecked(true);
    splitCheck_->setToolTip(tr("Links die Ausgabe, rechts der vorgeladene Look (während des Überblendens der einblendende). "
                               "Kostet etwas Rechenzeit, solange ein Look vorgeladen ist"));
    columnsRow->addWidget(splitCheck_);
    columnsRow->addStretch(1);
    barLayout->addLayout(columnsRow);
    connect(columnsBox_, &QSpinBox::valueChanged, this, [this](int value) {
        setColumns(value);
        Q_EMIT columnsChanged(value);
    });
    connect(splitCheck_, &QCheckBox::toggled, this, [this](bool on) { Q_EMIT splitPreviewChanged(on); });
    settingsBar_->setVisible(false);
    layout->addWidget(settingsBar_);

    // Das Raster liegt in einem Scrollbereich: ab der fünften Zeile (mehr als 16 Looks) würden die Knöpfe sonst abgeschnitten
    auto* scroll = new QScrollArea{this};
    scroll->setObjectName(QStringLiteral("liveGridScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gridHost_ = new QWidget{scroll};
    grid_ = new QGridLayout{gridHost_};
    grid_->setSpacing(8);
    scroll->setWidget(gridHost_);
    layout->addWidget(scroll, 1);

    auto* options = new QHBoxLayout;
    useGo_ = new QCheckBox{tr("Mit GO (vorladen)"), this};
    useGo_->setToolTip(tr("Ein Klick lädt den Look nur vor. GO (Leertaste) startet dann die Überblendung."));
    customFade_ = new QCheckBox{tr("Eigene Überblendzeit:"), this};
    customFade_->setToolTip(tr("Statt der Zeit, die beim Look gespeichert ist"));
    fade_ = new QDoubleSpinBox{this};
    fade_->setKeyboardTracking(false);  // Issue #22: erst bei Enter übernehmen
    fade_->setRange(0.0, 600.0);
    fade_->setDecimals(1);
    fade_->setSingleStep(0.5);
    fade_->setValue(2.0);
    fade_->setSuffix(tr(" s"));
    fade_->setEnabled(false);
    connect(customFade_, &QCheckBox::toggled, fade_, &QWidget::setEnabled);
    goButton_ = new QPushButton{tr("GO  (Leertaste)"), this};
    goButton_->setMinimumHeight(48);
    goButton_->setStyleSheet(styleFor(QStringLiteral("#2468c4")));
    goButton_->setEnabled(false);
    options->addWidget(useGo_);
    options->addWidget(customFade_);
    options->addWidget(fade_);
    options->addStretch(1);
    layout->addLayout(options);
    layout->addWidget(goButton_);

    connect(goButton_, &QPushButton::clicked, this, [this]() { go(); });
    connect(leave, &QPushButton::clicked, this, [this]() { Q_EMIT leaveRequested(); });
    connect(settingsToggle_, &QPushButton::toggled, this, [this](bool shown) { setSettingsShown(shown); });
    connect(useGo_, &QCheckBox::toggled, this, [this](bool) { refresh(); });

    // Tasten gelten, solange das Panel sichtbar ist (das Panel liegt im Hauptfenster).
    auto* space = new QShortcut{QKeySequence{Qt::Key_Space}, this};
    connect(space, &QShortcut::activated, this, [this]() { go(); });
    auto* escape = new QShortcut{QKeySequence{Qt::Key_Escape}, this};
    connect(escape, &QShortcut::activated, this, [this]() { Q_EMIT leaveRequested(); });
    for (int i = 0; i < 9; ++i) {
        auto* key = new QShortcut{QKeySequence{static_cast<int>(Qt::Key_1) + i}, this};
        digitKeys_.push_back(key);
        connect(key, &QShortcut::activated, this, [this, i]() {
            if (i < buttonCount()) {
                trigger(i);
            }
        });
    }
    rebuild();
}

void LivePanel::setSplitPreview(bool on) {
    const QSignalBlocker blocker{splitCheck_};
    splitCheck_->setChecked(on);
}

bool LivePanel::splitPreview() const { return splitCheck_->isChecked(); }

void LivePanel::setColumns(int columns) {
    columns = std::clamp(columns, 1, 16);
    if (columns == columns_) {
        return;
    }
    columns_ = columns;
    {
        const QSignalBlocker blocker{columnsBox_};
        columnsBox_->setValue(columns_);
    }
    rebuild();
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
    const auto& looks = player_.looks();
    for (std::size_t i = 0; i < looks.size(); ++i) {
        const int index = static_cast<int>(i);
        // Eine Zelle je Look: der große Knopf, darunter (nur bei offenen Einstellungen) Flash und Zurückblenden
        auto* cell = new QWidget{gridHost_};
        auto* cellLayout = new QVBoxLayout{cell};
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(4);
        auto* button = new QPushButton{buttonText(index), cell};
        button->setMinimumHeight(72);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        button->setToolTip(tr("Überblendzeit %1 s. Rechtsklick: umschalten oder flashen").arg(looks[i].fadeSeconds, 0, 'f', 1));
        // Drücken und Loslassen statt Klick: ein Flash-Look läuft, solange gehalten (live_panel_looks.cpp)
        connect(button, &QPushButton::pressed, this, [this, index]() { press(index); });
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
        flash->setToolTip(tr("Der Look läuft nur, solange der Knopf (oder das Pad) gehalten wird"));
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
            const std::optional<double> keep = player_.looks()[static_cast<std::size_t>(index)].returnSeconds;
            (void)setLookTrigger(index, on ? project::LookTrigger::Flash : project::LookTrigger::Toggle,
                                 on ? keep : std::optional<double>{});
            syncLookOptions(index);  // scheitert das Setzen, springt der Haken zurück
        });
        for (QPushButton* timeButton : {ins, returns}) {  // beide Zeiten: das Menü des Looks (Zeiten zum Wählen)
            connect(timeButton, &QPushButton::clicked, this, [this, index, timeButton]() {
                QMenu* menu = lookMenu(index);
                menu->exec(timeButton->mapToGlobal(QPoint{0, timeButton->height()}));
                menu->deleteLater();
            });
        }

        grid_->addWidget(cell, index / columns_, index % columns_);
        cells_.push_back(cell);
        buttons_.push_back(button);
        optionRows_.push_back(row);
        flashChecks_.push_back(flash);
        inButtons_.push_back(ins);
        returnButtons_.push_back(returns);
        syncLookOptions(index);
    }
    refresh();
}

void LivePanel::refresh() {
    // Läuft die Cuelist, gehören die Zifferntasten ihr (Tasten-Auslöser); sonst wären sie doppelt belegt.
    const bool cuesRunning = player_.cuesRunning();
    for (QShortcut* key : digitKeys_) {
        key->setEnabled(!cuesRunning);
    }
    const runtime::LiveStatus st = player_.liveStatus();
    const auto& looks = player_.looks();
    auto nameOf = [&looks](int index) {
        return index >= 0 && static_cast<std::size_t>(index) < looks.size()
                   ? fromStd(looks[static_cast<std::size_t>(index)].name)
                   : QString{};
    };
    for (int i = 0; i < buttonCount(); ++i) {
        const char* color = kIdle;
        if (st.fading && i == st.incoming) {
            color = kIncoming;
        } else if (i == st.current) {
            color = kLive;
        } else if (i == st.armed && !st.fading) {
            color = kArmed;
        }
        buttons_[static_cast<std::size_t>(i)]->setStyleSheet(styleFor(QString::fromLatin1(color)));
    }
    const QString now = nameOf(st.current);
    nowLabel_->setText(st.fading ? tr("%1  →  %2").arg(now, nameOf(st.incoming)) : (now.isEmpty() ? tr("(kein Look)") : now));
    nextLabel_->setText(st.armed >= 0 && !st.fading ? tr("Bereit: %1 - GO drücken").arg(nameOf(st.armed)) : QString{});
    progress_->setValue(static_cast<int>(std::lround(std::clamp(st.progress, 0.0, 1.0) * 1000.0)));
    goButton_->setEnabled(st.armed >= 0 && !st.fading);
}

std::optional<double> LivePanel::fadeOverride() const {
    if (customFade_->isChecked()) {
        return fade_->value();
    }
    return std::nullopt;
}

void LivePanel::setFadeOverride(std::optional<double> seconds) {
    customFade_->setChecked(seconds.has_value());
    if (seconds) {
        fade_->setValue(*seconds);
    }
}

void LivePanel::setUseGo(bool useGo) { useGo_->setChecked(useGo); }

bool LivePanel::useGo() const { return useGo_->isChecked(); }

bool LivePanel::trigger(int index) {
    if (player_.cuesRunning()) {
        Q_EMIT failed(tr("Die Cuelist läuft und schaltet die Looks selbst. Erst in der Cuelist auf Stopp drücken."));
        return false;
    }
    const Result<void> r = useGo_->isChecked() ? player_.liveArm(index) : player_.liveTrigger(index, fadeOverride());
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return false;
    }
    refresh();
    return true;
}

bool LivePanel::go() {
    if (player_.cuesRunning()) {
        player_.cueGo();  // die Cuelist läuft: GO löst ihren nächsten Cue aus
        return true;
    }
    const Result<void> r = player_.liveGo(fadeOverride());
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return false;
    }
    refresh();
    return true;
}

}  // namespace whitehole::studio
