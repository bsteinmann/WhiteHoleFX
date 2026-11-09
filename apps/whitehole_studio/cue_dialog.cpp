// cue_dialog.cpp: Fenster zum Bearbeiten eines Cues (siehe cue_dialog.hpp).
#include "cue_dialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

#include "form_helpers.hpp"

namespace whitehole::studio {

namespace {

constexpr int kTriggerCount = 5;  // Reihenfolge wie project::CueTrigger

}  // namespace

CueDialog::CueDialog(const project::Cue& cue, const std::vector<project::Look>& looks, QWidget* parent)
    : QDialog{parent} {
    setWindowTitle(tr("Cue bearbeiten"));
    auto* column = new QVBoxLayout{this};
    auto* form = new QFormLayout;
    column->addLayout(form);

    name_ = new QLineEdit{form::fromStd(cue.name)};
    form->addRow(tr("Name"), name_);

    note_ = new QPlainTextEdit{form::fromStd(cue.note)};
    note_->setPlaceholderText(tr("Notiz zum Cue (zum Beispiel Regieanweisung)"));
    note_->setToolTip(tr("Freie Notiz zu diesem Cue; sie steht in der Cuelist und wird mit dem Projekt gespeichert"));
    note_->setFixedHeight(note_->fontMetrics().lineSpacing() * 4);
    form->addRow(tr("Notiz"), note_);

    look_ = new QComboBox;
    look_->addItem(tr("(Schwarz)"), QString{});
    int lookIndex = 0;
    for (std::size_t i = 0; i < looks.size(); ++i) {
        look_->addItem(form::fromStd(looks[i].name), form::fromStd(looks[i].name));
        if (looks[i].name == cue.look) {
            lookIndex = static_cast<int>(i) + 1;
        }
    }
    look_->setCurrentIndex(lookIndex);
    form->addRow(tr("Look"), look_);

    trigger_ = new QComboBox;
    trigger_->addItems({tr("GO (von Hand)"), tr("Zeit"), tr("Taste"), tr("DMX-Kanal"), tr("Audio-Beat")});
    trigger_->setCurrentIndex(std::clamp(static_cast<int>(cue.trigger), 0, kTriggerCount - 1));
    form->addRow(tr("Auslöser"), trigger_);

    time_ = form::doubleBox(cue.time, 0.0, project::kMaxCueSeconds, 0.5, 2);
    time_->setSuffix(tr(" s"));
    time_->setToolTip(tr("Wartezeit nach dem Ende der vorigen Überblendung"));
    form->addRow(tr("Zeit-Auslöser: Wartezeit"), time_);

    key_ = new QKeySequenceEdit;
    key_->setKeySequence(QKeySequence{form::fromStd(cue.key), QKeySequence::PortableText});
    key_->setToolTip(tr("Taste drücken, die den Cue auslösen soll"));
    keyText_ = form::fromStd(cue.key);
    form->addRow(tr("Tasten-Auslöser: Taste"), key_);

    dmxProtocol_ = form::combo({tr("Art-Net"), tr("sACN")}, cue.dmxProtocol == project::CueDmxProtocol::Sacn ? 1 : 0);
    dmxUniverse_ = form::intBox(static_cast<int>(cue.dmxUniverse), 0, 32767);
    dmxChannel_ = form::intBox(static_cast<int>(std::clamp<std::uint32_t>(cue.dmxChannel, 1, 512)), 1, 512);
    dmxThreshold_ = form::intBox(static_cast<int>(std::clamp<std::uint32_t>(cue.dmxThreshold, 1, 255)), 1, 255);
    dmxThreshold_->setToolTip(tr("Der Cue löst aus, wenn der Kanalwert auf diese Zahl oder darüber steigt"));
    form->addRow(tr("DMX-Auslöser: Protokoll"), dmxProtocol_);
    form->addRow(tr("DMX-Auslöser: Universe"), dmxUniverse_);
    form->addRow(tr("DMX-Auslöser: Kanal"), dmxChannel_);
    form->addRow(tr("DMX-Auslöser: Schwelle (1 bis 255)"), dmxThreshold_);

    delay_ = form::doubleBox(cue.delay, 0.0, project::kMaxCueSeconds, 0.5, 2);
    delay_->setSuffix(tr(" s"));
    delay_->setToolTip(tr("Pause zwischen Auslöser und Beginn der Überblendung"));
    fade_ = form::doubleBox(cue.fade, 0.0, project::kMaxCueSeconds, 0.5, 2);
    fade_->setSuffix(tr(" s"));
    fade_->setToolTip(tr("Dauer der Überblendung; der vorige Look blendet im selben Zug aus"));
    form->addRow(tr("Delay"), delay_);
    form->addRow(tr("Fade"), fade_);

    auto* buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    column->addWidget(buttons);

    connect(trigger_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { updateEnabled(); });
    updateEnabled();
}

void CueDialog::updateEnabled() {
    const int t = trigger_->currentIndex();
    time_->setEnabled(t == static_cast<int>(project::CueTrigger::Time));
    key_->setEnabled(t == static_cast<int>(project::CueTrigger::Key));
    for (QWidget* w : std::initializer_list<QWidget*>{dmxProtocol_, dmxUniverse_, dmxChannel_, dmxThreshold_}) {
        w->setEnabled(t == static_cast<int>(project::CueTrigger::Dmx));
    }
}

project::Cue CueDialog::cue() const {
    project::Cue result;
    result.name = name_->text().toUtf8().toStdString();
    result.note = note_->toPlainText().toUtf8().toStdString();
    result.look = look_->currentData().toString().toUtf8().toStdString();
    result.trigger = static_cast<project::CueTrigger>(std::clamp(trigger_->currentIndex(), 0, kTriggerCount - 1));
    result.time = time_->value();
    const QKeySequence sequence = key_->keySequence();
    result.key = (sequence.isEmpty() ? keyText_ : sequence.toString(QKeySequence::PortableText)).toUtf8().toStdString();
    result.dmxProtocol = dmxProtocol_->currentIndex() == 1 ? project::CueDmxProtocol::Sacn : project::CueDmxProtocol::ArtNet;
    result.dmxUniverse = static_cast<std::uint32_t>(dmxUniverse_->value());
    result.dmxChannel = static_cast<std::uint32_t>(dmxChannel_->value());
    result.dmxThreshold = static_cast<std::uint32_t>(dmxThreshold_->value());
    result.delay = delay_->value();
    result.fade = fade_->value();
    return result;
}

}  // namespace whitehole::studio
