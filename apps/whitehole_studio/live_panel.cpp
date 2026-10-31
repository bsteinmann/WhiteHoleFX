// live_panel.cpp: große Look-Knöpfe, GO, Überblendzeit, Fortschritt.
#include "live_panel.hpp"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

constexpr int kColumns = 4;
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
    layout->addWidget(nowLabel_);
    layout->addWidget(nextLabel_);

    progress_ = new QProgressBar{this};
    progress_->setRange(0, 1000);
    progress_->setTextVisible(false);
    progress_->setFixedHeight(10);
    layout->addWidget(progress_);

    gridHost_ = new QWidget{this};
    grid_ = new QGridLayout{gridHost_};
    grid_->setSpacing(8);
    layout->addWidget(gridHost_, 1);

    auto* options = new QHBoxLayout;
    useGo_ = new QCheckBox{tr("Mit GO (vorladen)"), this};
    useGo_->setToolTip(tr("Ein Klick lädt den Look nur vor. GO (Leertaste) startet dann die Überblendung."));
    customFade_ = new QCheckBox{tr("Eigene Überblendzeit:"), this};
    customFade_->setToolTip(tr("Statt der Zeit, die beim Look gespeichert ist"));
    fade_ = new QDoubleSpinBox{this};
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
    auto* leave = new QPushButton{tr("Zurück zum Editor (Esc)"), this};
    options->addWidget(useGo_);
    options->addWidget(customFade_);
    options->addWidget(fade_);
    options->addStretch(1);
    layout->addLayout(options);
    layout->addWidget(goButton_);
    layout->addWidget(leave);

    connect(goButton_, &QPushButton::clicked, this, [this]() { go(); });
    connect(leave, &QPushButton::clicked, this, [this]() { Q_EMIT leaveRequested(); });
    connect(useGo_, &QCheckBox::toggled, this, [this](bool) { refresh(); });

    // Tasten gelten, solange das Panel sichtbar ist (das Panel liegt im Hauptfenster).
    auto* space = new QShortcut{QKeySequence{Qt::Key_Space}, this};
    connect(space, &QShortcut::activated, this, [this]() { go(); });
    auto* escape = new QShortcut{QKeySequence{Qt::Key_Escape}, this};
    connect(escape, &QShortcut::activated, this, [this]() { Q_EMIT leaveRequested(); });
    for (int i = 0; i < 9; ++i) {
        auto* key = new QShortcut{QKeySequence{static_cast<int>(Qt::Key_1) + i}, this};
        connect(key, &QShortcut::activated, this, [this, i]() {
            if (i < buttonCount()) {
                trigger(i);
            }
        });
    }
    rebuild();
}

void LivePanel::rebuild() {
    for (QPushButton* button : buttons_) {
        grid_->removeWidget(button);
        delete button;
    }
    buttons_.clear();
    const auto& looks = player_.looks();
    for (std::size_t i = 0; i < looks.size(); ++i) {
        const QString prefix = i < 9 ? QStringLiteral("%1   ").arg(i + 1) : QString{};
        auto* button = new QPushButton{prefix + fromStd(looks[i].name), gridHost_};
        button->setMinimumHeight(72);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        button->setToolTip(tr("Überblendzeit %1 s").arg(looks[i].fadeSeconds, 0, 'f', 1));
        const int index = static_cast<int>(i);
        connect(button, &QPushButton::clicked, this, [this, index]() { trigger(index); });
        grid_->addWidget(button, index / kColumns, index % kColumns);
        buttons_.push_back(button);
    }
    refresh();
}

void LivePanel::refresh() {
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
    const Result<void> r = useGo_->isChecked() ? player_.liveArm(index) : player_.liveTrigger(index, fadeOverride());
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return false;
    }
    refresh();
    return true;
}

bool LivePanel::go() {
    const Result<void> r = player_.liveGo(fadeOverride());
    if (!r.ok()) {
        Q_EMIT failed(fromStd(r.error().message));
        return false;
    }
    refresh();
    return true;
}

}  // namespace whitehole::studio
