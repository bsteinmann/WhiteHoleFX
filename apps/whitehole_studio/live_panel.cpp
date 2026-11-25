// live_panel.cpp: Aufbau des Live-Fensters, GO, Überblendzeit, Fortschritt und Farben der Felder. Die Felder des Rasters
// selbst (Aufbau, Menü, Belegen) stehen in live_panel_cells.cpp.
#include "live_panel.hpp"


#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QIcon>
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

QString LivePanel::emptyStyle() {
    return QStringLiteral("QPushButton { background: transparent; border: 1px dashed #555; border-radius: 6px; color: #777; }");
}

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
    columnsRow->addWidget(new QLabel{tr("Zeilen:"), settingsBar_});
    rowsBox_ = new QSpinBox{settingsBar_};
    rowsBox_->setRange(1, 16);
    rowsBox_->setValue(rows_);
    rowsBox_->setToolTip(tr("Zeilen des festen Rasters; nicht belegte Felder bleiben leer. Bei mehr Looks als Feldern kommen Zeilen dazu"));
    columnsRow->addWidget(rowsBox_);
    columnsRow->addSpacing(16);
    splitCheck_ = new QCheckBox{tr("Vorschau teilen"), settingsBar_};
    splitCheck_->setObjectName(QStringLiteral("liveSplitPreview"));
    splitCheck_->setChecked(true);
    splitCheck_->setToolTip(tr("Links die Ausgabe, rechts der vorgeladene Look (während des Überblendens der einblendende). "
                               "Kostet etwas Rechenzeit, solange ein Look vorgeladen ist"));
    columnsRow->addWidget(splitCheck_);
    columnsRow->addStretch(1);
    barLayout->addLayout(columnsRow);
    for (QSpinBox* box : {columnsBox_, rowsBox_}) {
        connect(box, &QSpinBox::valueChanged, this, [this](int) {
            setGrid(columnsBox_->value(), rowsBox_->value());
            Q_EMIT gridChanged(columns_, rows_);
        });
    }
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

void LivePanel::setColumns(int columns) { setGrid(columns, rows_); }

void LivePanel::setRows(int rows) { setGrid(columns_, rows); }

void LivePanel::setGrid(int columns, int rows) {
    columns = std::clamp(columns, 1, 16);
    rows = std::clamp(rows, 1, 16);
    if (columns == columns_ && rows == rows_) {
        return;
    }
    columns_ = columns;
    rows_ = rows;
    syncGridBoxes();
    // Hat das Projekt schon ein Raster, bekommt es die neue Größe (belegte Felder behalten Zeile und Spalte);
    // sonst gelten die Werte als Vorgabe für das Raster, das rebuild() aus den Looks anlegt.
    if (player_.liveGrid().defined()) {
        if (const Result<void> r = player_.setLiveGridSize(columns_, rows_); !r.ok()) {
            Q_EMIT failed(fromStd(r.error().message));
        }
    }
    rebuild();
}

void LivePanel::setDefaultGrid(int columns, int rows) {
    columns_ = std::clamp(columns, 1, 16);
    rows_ = std::clamp(rows, 1, 16);
    syncGridBoxes();
}

void LivePanel::syncGridBoxes() {
    const QSignalBlocker columnsBlocker{columnsBox_};
    const QSignalBlocker rowsBlocker{rowsBox_};
    columnsBox_->setValue(columns_);
    rowsBox_->setValue(rows_);
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
        const int look = player_.cellLook(i);
        if (look < 0) {
            buttons_[static_cast<std::size_t>(i)]->setStyleSheet(emptyStyle());
            continue;
        }
        // Kam der Look über ein Feld, leuchtet nur dieses Feld; sonst (Cue, DMX-Look-Auswahl) jedes Feld mit dem Look
        const auto matches = [i, look](int statusCell, int statusLook) {
            return statusCell >= 0 ? statusCell == i : statusLook >= 0 && look == statusLook;
        };
        const char* color = kIdle;
        if (st.fading && matches(st.incomingCell, st.incoming)) {
            color = kIncoming;
        } else if (matches(st.currentCell, st.current)) {
            color = kLive;
        } else if (!st.fading && matches(st.armedCell, st.armed)) {
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
    const int look = player_.cellLook(index);  // `index` ist ein Feld des Rasters
    if (look < 0) {
        return false;  // leeres Feld
    }
    const Result<void> r = useGo_->isChecked() ? player_.liveArm(look) : player_.liveTrigger(look, fadeOverride());
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
