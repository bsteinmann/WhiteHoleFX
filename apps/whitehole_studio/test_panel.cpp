// test_panel.cpp: siehe test_panel.hpp.
#include "test_panel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>

#include "layout_panel.hpp"

namespace whitehole::studio {

namespace {

using runtime::TestPatternKind;

struct NamedColor {
    const char* name;
    float r, g, b;
};

constexpr NamedColor kColors[] = {
    {"Weiß", 1.0F, 1.0F, 1.0F}, {"Rot", 1.0F, 0.0F, 0.0F},  {"Grün", 0.0F, 1.0F, 0.0F},
    {"Blau", 0.0F, 0.0F, 1.0F}, {"Gelb", 1.0F, 1.0F, 0.0F}, {"Cyan", 0.0F, 1.0F, 1.0F},
    {"Magenta", 1.0F, 0.0F, 1.0F},
};

constexpr TestPatternKind kKinds[] = {TestPatternKind::Off,        TestPatternKind::Color,
                                      TestPatternKind::Highlight,  TestPatternKind::Sweep,
                                      TestPatternKind::ColorOrder, TestPatternKind::Chase};

QString rateSuffix(TestPatternKind kind) {
    switch (kind) {
        case TestPatternKind::Sweep: return QObject::tr(" Durchläufe/s");
        case TestPatternKind::ColorOrder: return QObject::tr(" Wechsel/s");
        case TestPatternKind::Chase: return QObject::tr(" LEDs/s");
        default: return QString{};
    }
}

}  // namespace

TestPanel::TestPanel(runtime::Player& player, LayoutPanel& layoutPanel, QWidget* parent)
    : QWidget{parent}, player_{player}, layout_{layoutPanel} {
    auto* column = new QVBoxLayout{this};
    auto* form = new QFormLayout;

    kind_ = new QComboBox{this};
    for (TestPatternKind kind : kKinds) {
        kind_->addItem(tr(runtime::testPatternName(kind)), static_cast<int>(kind));
    }
    form->addRow(tr("Testbild"), kind_);

    color_ = new QComboBox{this};
    for (const NamedColor& c : kColors) {
        color_->addItem(tr(c.name));
    }
    form->addRow(tr("Farbe"), color_);

    axis_ = new QComboBox{this};
    axis_->addItems({tr("x (links → rechts)"), tr("y (unten → oben)"), tr("z (Tiefe)")});
    form->addRow(tr("Richtung"), axis_);

    rate_ = new QDoubleSpinBox{this};
    rate_->setRange(0.05, 100.0);
    rate_->setDecimals(2);
    rate_->setSingleStep(0.1);
    rate_->setValue(1.0);
    form->addRow(tr("Tempo"), rate_);

    onlySelected_ = new QCheckBox{tr("Nur auf gewählten Fixtures"), this};
    onlySelected_->setToolTip(tr("Die Fixtures in der Layout-Liste; alle anderen bleiben dunkel."));
    form->addRow(QString{}, onlySelected_);
    column->addLayout(form);

    stop_ = new QPushButton{tr("Testmodus beenden"), this};
    column->addWidget(stop_);

    hint_ = new QLabel{this};
    hint_->setWordWrap(true);
    hint_->setText(
        tr("Das Testbild ersetzt den Effekt in der Vorschau und, wenn „Ausgabe senden“ an ist, auch auf den "
           "LEDs. Es wird nicht im Projekt gespeichert.\n\n"
           "Achtung: Weiß und Farbe leuchten mit voller Helligkeit. Prüfe vorher, ob die Netzteile die ganze "
           "Wand auf 100 % tragen."));
    column->addWidget(hint_);
    column->addStretch(1);

    connect(kind_, &QComboBox::currentIndexChanged, this, [this]() { kindChanged(); });
    connect(color_, &QComboBox::currentIndexChanged, this, [this]() { apply(); });
    connect(axis_, &QComboBox::currentIndexChanged, this, [this]() { apply(); });
    connect(rate_, &QDoubleSpinBox::valueChanged, this, [this]() { apply(); });
    connect(onlySelected_, &QCheckBox::toggled, this, [this]() { apply(); });
    connect(stop_, &QPushButton::clicked, this, [this]() { kind_->setCurrentIndex(0); });
    // Das Hervorheben folgt der Auswahl in der Layout-Liste und in der Vorschau.
    connect(&layout_, &LayoutPanel::selectionChanged, this, [this]() {
        const TestPatternKind kind = currentKind();
        if (kind == TestPatternKind::Highlight || (kind != TestPatternKind::Off && onlySelected_->isChecked())) {
            apply();
        }
    });
    updateEnabled();
}

TestPatternKind TestPanel::currentKind() const {
    return static_cast<TestPatternKind>(kind_->currentData().toInt());
}

void TestPanel::updateEnabled() {
    const TestPatternKind kind = currentKind();
    const bool on = kind != TestPatternKind::Off;
    color_->setEnabled(kind == TestPatternKind::Color);
    axis_->setEnabled(kind == TestPatternKind::Sweep);
    rate_->setEnabled(kind == TestPatternKind::Sweep || kind == TestPatternKind::ColorOrder ||
                      kind == TestPatternKind::Chase);
    onlySelected_->setEnabled(on && kind != TestPatternKind::Highlight);
    stop_->setEnabled(on);
    rate_->setSuffix(rateSuffix(kind));
}

void TestPanel::kindChanged() {
    if (updating_) {
        return;
    }
    const TestPatternKind kind = currentKind();
    updating_ = true;
    rate_->setValue(static_cast<double>(runtime::defaultTestRate(kind)));
    updating_ = false;
    apply();
}

void TestPanel::apply() {
    if (updating_) {
        return;
    }
    const TestPatternKind kind = currentKind();
    updateEnabled();
    if (kind == TestPatternKind::Off) {
        player_.clearTestPattern();
        Q_EMIT patternChanged();
        return;
    }
    runtime::TestPattern pattern;
    pattern.kind = kind;
    const int colorIndex = std::clamp(color_->currentIndex(), 0, static_cast<int>(std::size(kColors)) - 1);
    pattern.color = ColorRgba{kColors[colorIndex].r, kColors[colorIndex].g, kColors[colorIndex].b, 1.0F};
    pattern.axis = static_cast<runtime::SweepAxis>(std::clamp(axis_->currentIndex(), 0, 2));
    pattern.rate = static_cast<float>(rate_->value());
    if (kind == TestPatternKind::Highlight || onlySelected_->isChecked()) {
        pattern.fixtures = layout_.selectedIds();
        if (pattern.fixtures.empty() && kind != TestPatternKind::Highlight) {
            // "Nur gewählte" ohne Auswahl: lieber nichts leuchten lassen als alles.
            pattern.fixtures = {mapping::FixtureId{}};
        }
    }
    const Result<void> r = player_.setTestPattern(std::move(pattern));
    if (!r.ok()) {
        Q_EMIT failed(QString::fromUtf8(r.error().message.c_str()));
        updating_ = true;
        kind_->setCurrentIndex(0);
        updating_ = false;
        updateEnabled();
    }
    Q_EMIT patternChanged();
}

void TestPanel::reset() {
    updating_ = true;
    kind_->setCurrentIndex(0);
    updating_ = false;
    updateEnabled();
    Q_EMIT patternChanged();
}

QString TestPanel::description() const {
    const runtime::TestPattern* pattern = player_.testPattern();
    if (pattern == nullptr) {
        return {};
    }
    return tr(runtime::testPatternName(pattern->kind));
}

QString TestPanel::exercise() {
    for (int i = 1; i < kind_->count(); ++i) {
        kind_->setCurrentIndex(i);
        if (player_.testPattern() == nullptr) {
            return QStringLiteral("Testmodus: Art %1 wurde nicht eingeschaltet").arg(kind_->itemText(i));
        }
        if (description().isEmpty()) {
            return QStringLiteral("Testmodus: keine Beschreibung für Art %1").arg(kind_->itemText(i));
        }
        if (!stop_->isEnabled()) {
            return QStringLiteral("Testmodus: \"beenden\" ist bei Art %1 gesperrt").arg(kind_->itemText(i));
        }
    }
    kind_->setCurrentIndex(static_cast<int>(TestPatternKind::Sweep));
    axis_->setCurrentIndex(1);
    rate_->setValue(rate_->value() + 0.1);
    if (player_.testPattern() == nullptr || player_.testPattern()->axis != runtime::SweepAxis::Y) {
        return QStringLiteral("Testmodus: Richtung y kam nicht beim Player an");
    }
    stop_->click();
    if (player_.testPattern() != nullptr || !description().isEmpty()) {
        return QStringLiteral("Testmodus: \"beenden\" schaltet das Testbild nicht ab");
    }
    return {};
}

}  // namespace whitehole::studio
