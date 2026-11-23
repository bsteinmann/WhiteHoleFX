// midi_profile_box.cpp: Umsetzung zu midi_profile_box.hpp.
#include "midi_profile_box.hpp"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace whitehole::studio {

namespace {

QString qs(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

}  // namespace

std::vector<midi::LiveProfile> allLiveProfiles(const QStringList& userLines) {
    std::vector<midi::LiveProfile> all = midi::builtinLiveProfiles();
    for (const QString& line : userLines) {
        const std::string text = line.toStdString();
        if (auto profile = midi::parseProfile(text)) {
            bool taken = false;
            for (const midi::LiveProfile& existing : all) {
                taken = taken || existing.name == profile->name;
            }
            if (!taken) {
                all.push_back(std::move(*profile));
            }
        }
    }
    return all;
}

midi::LiveProfile findLiveProfile(const QStringList& userLines, const QString& name) {
    const std::vector<midi::LiveProfile> all = allLiveProfiles(userLines);
    for (const midi::LiveProfile& profile : all) {
        if (qs(profile.name) == name) {
            return profile;
        }
    }
    return all.front();
}

MidiProfileBox::MidiProfileBox(const QString& active, const QStringList& userLines, QWidget* parent)
    : QGroupBox{tr("Live-Modus auf dem Controller (Akai APC mini mk1)"), parent}, profiles_{allLiveProfiles(userLines)} {
    updating_ = true;  // die Felder melden beim Aufbau Änderungen, die noch nichts bedeuten
    auto* layout = new QVBoxLayout{this};
    auto* intro = new QLabel{tr("Im Live-Modus rufen die Pads die Looks ab (Raster wie im Live-Fenster, erste Zeile oben) "
                                "und leuchten: grün = läuft, gelb blinkend = vorgeladen, gelb = blendet ein. "
                                "Außerhalb des Live-Modus bleiben Pads und Fader frei. Nummern: Noten bzw. Regler (CC), −1 = nicht belegt."),
                             this};
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto* row = new QHBoxLayout;
    combo_ = new QComboBox{this};
    auto* copy = new QPushButton{tr("Kopieren …"), this};
    rename_ = new QPushButton{tr("Umbenennen …"), this};
    remove_ = new QPushButton{tr("Löschen"), this};
    row->addWidget(combo_, 1);
    row->addWidget(copy);
    row->addWidget(rename_);
    row->addWidget(remove_);
    layout->addLayout(row);
    auto* form = new QFormLayout;
    const auto spin = [this](int low, int high) {
        auto* box = new QSpinBox{this};
        box->setRange(low, high);
        box->setSpecialValueText(low < 0 ? tr("aus") : QString{});
        connect(box, &QSpinBox::valueChanged, this, [this](int) { storeFields(); });
        return box;
    };
    columns_ = spin(1, 8);
    go_ = spin(-1, 127);
    stop_ = spin(-1, 127);
    master_ = spin(-1, 127);
    fade_ = spin(-1, 127);
    crossfade_ = spin(-1, 127);
    form->addRow(tr("Looks je Zeile"), columns_);
    form->addRow(tr("Taste GO (Note)"), go_);
    form->addRow(tr("Taste Live beenden (Note)"), stop_);
    form->addRow(tr("Fader Master (CC)"), master_);
    form->addRow(tr("Fader Überblendzeit 0–10 s (CC)"), fade_);
    form->addRow(tr("Fader manuelle Überblendung (CC)"), crossfade_);
    layout->addLayout(form);

    connect(combo_, &QComboBox::currentIndexChanged, this, [this](int) { showCurrent(); });
    connect(copy, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Profil kopieren"), tr("Name des neuen Profils:"),
                                                   QLineEdit::Normal, combo_->currentText() + tr(" (Kopie)"), &ok);
        if (ok) {
            (void)copyCurrent(name);
        }
    });
    connect(rename_, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Profil umbenennen"), tr("Neuer Name:"), QLineEdit::Normal,
                                                   combo_->currentText(), &ok);
        if (ok) {
            (void)renameCurrent(name);
        }
    });
    connect(remove_, &QPushButton::clicked, this, [this]() { (void)removeCurrent(); });
    updating_ = false;
    fillCombo(active);
}

int MidiProfileBox::currentIndex() const { return std::max(0, combo_->currentIndex()); }

QString MidiProfileBox::active() const { return qs(profiles_[static_cast<std::size_t>(currentIndex())].name); }

QStringList MidiProfileBox::userProfiles() const {
    QStringList lines;
    for (const midi::LiveProfile& profile : profiles_) {
        if (!profile.builtin) {
            lines << qs(midi::serializeProfile(profile));
        }
    }
    return lines;
}

bool MidiProfileBox::nameFree(const QString& name) const {
    if (name.trimmed().isEmpty() || name.contains(QLatin1Char('|'))) {
        return false;
    }
    for (const midi::LiveProfile& profile : profiles_) {
        if (qs(profile.name) == name.trimmed()) {
            return false;
        }
    }
    return true;
}

bool MidiProfileBox::copyCurrent(const QString& name) {
    if (!nameFree(name)) {
        return false;
    }
    midi::LiveProfile copy = profiles_[static_cast<std::size_t>(currentIndex())];
    copy.name = name.trimmed().toStdString();
    copy.builtin = false;
    profiles_.push_back(copy);
    fillCombo(qs(copy.name));
    return true;
}

bool MidiProfileBox::renameCurrent(const QString& name) {
    midi::LiveProfile& profile = profiles_[static_cast<std::size_t>(currentIndex())];
    if (profile.builtin || !nameFree(name)) {
        return false;
    }
    profile.name = name.trimmed().toStdString();
    fillCombo(qs(profile.name));
    return true;
}

bool MidiProfileBox::removeCurrent() {
    const auto index = static_cast<std::size_t>(currentIndex());
    if (profiles_[index].builtin) {
        return false;
    }
    profiles_.erase(profiles_.begin() + static_cast<std::ptrdiff_t>(index));
    fillCombo(qs(profiles_.front().name));
    return true;
}

bool MidiProfileBox::select(const QString& name) {
    const int index = combo_->findText(name);
    if (index < 0) {
        return false;
    }
    combo_->setCurrentIndex(index);
    return true;
}

void MidiProfileBox::fillCombo(const QString& selected) {
    updating_ = true;
    combo_->clear();
    for (const midi::LiveProfile& profile : profiles_) {
        combo_->addItem(qs(profile.name));
    }
    const int index = combo_->findText(selected);
    combo_->setCurrentIndex(index < 0 ? 0 : index);
    updating_ = false;
    showCurrent();
}

void MidiProfileBox::showCurrent() {
    if (updating_ || profiles_.empty()) {
        return;
    }
    const midi::LiveProfile& profile = profiles_[static_cast<std::size_t>(currentIndex())];
    updating_ = true;
    columns_->setValue(profile.columns);
    go_->setValue(profile.controls.goNote);
    stop_->setValue(profile.controls.stopNote);
    master_->setValue(profile.controls.masterCc);
    fade_->setValue(profile.controls.fadeCc);
    crossfade_->setValue(profile.controls.crossfadeCc);
    updating_ = false;
    for (QSpinBox* box : {columns_, go_, stop_, master_, fade_, crossfade_}) {
        box->setEnabled(!profile.builtin);
        box->setToolTip(profile.builtin ? tr("Vorgefertigtes Profil: zum Ändern erst kopieren") : QString{});
    }
    rename_->setEnabled(!profile.builtin);
    remove_->setEnabled(!profile.builtin);
}

void MidiProfileBox::storeFields() {
    if (updating_) {
        return;
    }
    midi::LiveProfile& profile = profiles_[static_cast<std::size_t>(currentIndex())];
    if (profile.builtin) {
        return;
    }
    profile.columns = columns_->value();
    profile.controls = midi::LiveControls{go_->value(), stop_->value(), master_->value(), fade_->value(), crossfade_->value()};
}

}  // namespace whitehole::studio
