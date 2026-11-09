// cue_panel.cpp: die Cuelist im Fenster (siehe cue_panel.hpp).
#include "cue_panel.hpp"

#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QShortcut>
#include <QtAlgorithms>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <string>
#include <utility>

#include "cue_dialog.hpp"

namespace whitehole::studio {

namespace {

constexpr int kColumns = 7;

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

QString secondsText(double seconds) { return QStringLiteral("%1 s").arg(seconds, 0, 'g', 4); }

QString triggerText(const project::Cue& cue) {
    switch (cue.trigger) {
        case project::CueTrigger::Go: return QObject::tr("GO");
        case project::CueTrigger::Time: return QObject::tr("Zeit: %1 nach dem vorigen").arg(secondsText(cue.time));
        case project::CueTrigger::Key: return QObject::tr("Taste %1").arg(fromStd(cue.key));
        case project::CueTrigger::Dmx:
            return QObject::tr("DMX %1 U%2 K%3 ab %4")
                .arg(cue.dmxProtocol == project::CueDmxProtocol::Sacn ? QObject::tr("sACN") : QObject::tr("Art-Net"))
                .arg(cue.dmxUniverse)
                .arg(cue.dmxChannel)
                .arg(cue.dmxThreshold);
        case project::CueTrigger::Audio: return QObject::tr("Audio-Beat");
    }
    return {};
}

}  // namespace

CuePanel::CuePanel(runtime::Player& player, QWidget* parent) : QWidget{parent}, player_{player} {
    auto* column = new QVBoxLayout{this};
    table_ = new QTableWidget{0, kColumns, this};
    table_->setHorizontalHeaderLabels({tr("Nr."), tr("Name"), tr("Look"), tr("Auslöser"), tr("Delay"), tr("Fade"), tr("Notiz")});
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->hide();
    table_->horizontalHeader()->setStretchLastSection(true);
    column->addWidget(table_, 1);

    auto* row = new QHBoxLayout;
    addButton_ = new QPushButton{tr("Cue +"), this};
    removeButton_ = new QPushButton{tr("Cue −"), this};
    editButton_ = new QPushButton{tr("Bearbeiten …"), this};
    upButton_ = new QPushButton{QStringLiteral("↑"), this};
    downButton_ = new QPushButton{QStringLiteral("↓"), this};
    startButton_ = new QPushButton{tr("Start"), this};
    startButton_->setCheckable(true);
    startButton_->setToolTip(tr("Cuelist starten: wechselt in den Live-Modus und zeigt Cue 1"));
    goButton_ = new QPushButton{tr("GO"), this};
    goButton_->setToolTip(tr("Nächsten Cue auslösen (Leertaste)"));
    for (QPushButton* b : {addButton_, removeButton_, editButton_, upButton_, downButton_}) {
        row->addWidget(b);
    }
    row->addStretch(1);
    row->addWidget(startButton_);
    row->addWidget(goButton_);
    column->addLayout(row);

    connect(addButton_, &QPushButton::clicked, this, [this]() { addCue(); });
    connect(removeButton_, &QPushButton::clicked, this, [this]() { removeSelected(); });
    connect(editButton_, &QPushButton::clicked, this, [this]() { editRow(selectedRow()); });
    connect(upButton_, &QPushButton::clicked, this, [this]() { moveSelected(-1); });
    connect(downButton_, &QPushButton::clicked, this, [this]() { moveSelected(1); });
    connect(goButton_, &QPushButton::clicked, this, [this]() { player_.cueGo(); });
    connect(startButton_, &QPushButton::clicked, this, [this](bool on) {
        if (on) {
            Q_EMIT startRequested();
        } else {
            Q_EMIT stopRequested();
        }
    });
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int clicked, int) {
        if (player_.cuesRunning()) {
            if (const Result<void> r = player_.cueFire(clicked); !r.ok()) {
                Q_EMIT failed(fromStd(r.error().message));
            }
        } else {
            editRow(clicked);
        }
    });
    connect(table_, &QTableWidget::itemSelectionChanged, this, [this]() { updateButtons(); });
    rebuild();
}

int CuePanel::cueCount() const { return table_->rowCount(); }

int CuePanel::selectedRow() const {
    const auto rows = table_->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

void CuePanel::rebuild() {
    const std::vector<project::Cue>& cues = player_.cues();
    table_->setRowCount(static_cast<int>(cues.size()));
    for (std::size_t i = 0; i < cues.size(); ++i) {
        const project::Cue& cue = cues[i];
        const QStringList cells{QString::number(i + 1), fromStd(cue.name),
                                cue.look.empty() ? tr("(Schwarz)") : fromStd(cue.look), triggerText(cue),
                                secondsText(cue.delay), secondsText(cue.fade),
                                fromStd(cue.note).simplified()};
        for (int c = 0; c < kColumns; ++c) {
            table_->setItem(static_cast<int>(i), c, new QTableWidgetItem{cells[c]});
        }
    }
    table_->resizeColumnsToContents();
    lastCurrent_ = lastNext_ = lastPending_ = -2;  // Farben beim nächsten refresh() neu setzen
    updateButtons();
    refresh();
}

void CuePanel::updateButtons() {
    const bool running = player_.cuesRunning();
    const bool hasRow = selectedRow() >= 0;
    // Während des Ablaufs wird die Liste nicht umgebaut: Auf der Bühne soll nichts versehentlich verschwinden.
    addButton_->setEnabled(!running);
    removeButton_->setEnabled(!running && hasRow);
    editButton_->setEnabled(!running && hasRow);
    upButton_->setEnabled(!running && hasRow);
    downButton_->setEnabled(!running && hasRow);
    goButton_->setEnabled(running);
    startButton_->setEnabled(running || !player_.cues().empty());
    const QSignalBlocker block{startButton_};
    startButton_->setChecked(running);
    startButton_->setText(running ? tr("Stopp") : tr("Start"));
}

void CuePanel::refresh() {
    syncShortcuts();
    const runtime::CueRunnerStatus s = player_.cueStatus();
    if (s.current == lastCurrent_ && s.next == lastNext_ && s.pending == lastPending_ &&
        s.running == (startButton_->isChecked())) {
        return;
    }
    lastCurrent_ = s.current;
    lastNext_ = s.next;
    lastPending_ = s.pending;
    const QBrush none{Qt::NoBrush};
    const QBrush running{QColor{46, 160, 67, 110}};
    const QBrush next{QColor{230, 140, 30, 110}};
    const QBrush waiting{QColor{220, 200, 40, 130}};
    for (int r = 0; r < table_->rowCount(); ++r) {
        const QBrush& brush = !s.running ? none : r == s.pending ? waiting : r == s.current ? running : r == s.next ? next : none;
        for (int c = 0; c < kColumns; ++c) {
            if (QTableWidgetItem* item = table_->item(r, c)) {
                item->setBackground(brush);
            }
        }
    }
    updateButtons();
}

void CuePanel::syncShortcuts() {
    const bool running = player_.cuesRunning();
    if (running == shortcutsActive_) {
        return;
    }
    shortcutsActive_ = running;
    qDeleteAll(shortcuts_);
    shortcuts_.clear();
    if (!running) {
        return;
    }
    for (const project::Cue& cue : player_.cues()) {
        if (cue.trigger != project::CueTrigger::Key || cue.key.empty()) {
            continue;
        }
        const QString text = fromStd(cue.key);
        // Eltern ist das Panel (nicht das Fenster): So räumt Qt beim Beenden genau einmal auf.
        auto* shortcut = new QShortcut{QKeySequence{text, QKeySequence::PortableText}, this};
        shortcut->setContext(Qt::ApplicationShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, key = cue.key]() { player_.cueKey(key); });
        shortcuts_.push_back(shortcut);
    }
}

bool CuePanel::commit(std::vector<project::Cue> cues, int selectRow) {
    if (const Result<void> r = player_.setCues(std::move(cues)); !r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return false;
    }
    rebuild();
    if (selectRow >= 0 && selectRow < table_->rowCount()) {
        table_->selectRow(selectRow);
    }
    Q_EMIT changed();
    return true;
}

bool CuePanel::addCue() {
    std::vector<project::Cue> cues = player_.cues();
    project::Cue cue;
    const std::vector<project::Look>& looks = player_.looks();
    if (!looks.empty()) {
        cue.look = looks.front().name;
        cue.fade = looks.front().fadeSeconds;
    }
    cues.push_back(cue);
    const int last = static_cast<int>(cues.size()) - 1;
    return commit(std::move(cues), last);
}

bool CuePanel::removeSelected() {
    const int row = selectedRow();
    std::vector<project::Cue> cues = player_.cues();
    if (row < 0 || row >= static_cast<int>(cues.size())) {
        return false;
    }
    cues.erase(cues.begin() + row);
    const int select = std::min(row, static_cast<int>(cues.size()) - 1);
    return commit(std::move(cues), select);
}

bool CuePanel::moveSelected(int delta) {
    const int row = selectedRow();
    std::vector<project::Cue> cues = player_.cues();
    const int to = row + delta;
    if (row < 0 || to < 0 || to >= static_cast<int>(cues.size())) {
        return false;
    }
    std::swap(cues[static_cast<std::size_t>(row)], cues[static_cast<std::size_t>(to)]);
    return commit(std::move(cues), to);
}

bool CuePanel::applyCue(int row, const project::Cue& cue) {
    std::vector<project::Cue> cues = player_.cues();
    if (row < 0 || row >= static_cast<int>(cues.size())) {
        return false;
    }
    cues[static_cast<std::size_t>(row)] = cue;
    return commit(std::move(cues), row);
}

bool CuePanel::editRow(int row) {
    const std::vector<project::Cue>& cues = player_.cues();
    if (row < 0 || row >= static_cast<int>(cues.size())) {
        return false;
    }
    CueDialog dialog{cues[static_cast<std::size_t>(row)], player_.looks(), this};
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    return applyCue(row, dialog.cue());
}

}  // namespace whitehole::studio
