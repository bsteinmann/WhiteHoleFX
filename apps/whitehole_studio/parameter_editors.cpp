// parameter_editors.cpp: Bedienelemente je Parametertyp (siehe parameter_editors.hpp).
#include "parameter_editors.hpp"

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

namespace whitehole::studio {

namespace {

/// Schrittweite aus einer Spanne: 1, 2 oder 5 mal einer Zehnerpotenz, etwa ein Hundertstel der Spanne.
double niceStep(double span) {
    const double raw = span / 100.0;
    if (!(raw > 0.0) || !std::isfinite(raw)) {
        return 1.0;
    }
    const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
    const double n = raw / magnitude;
    return (n < 2.0 ? 1.0 : (n < 5.0 ? 2.0 : 5.0)) * magnitude;
}

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

QDoubleSpinBox* makeSpin(QWidget* parent, double min, double max, double step) {
    auto* spin = new QDoubleSpinBox{parent};
    spin->setDecimals(3);
    spin->setRange(min, max);
    spin->setSingleStep(step);
    return spin;
}

// Farben werden so gespeichert, wie man sie auf dem Bildschirm sieht (siehe color_encoder.hpp):
// der Wert 0,5 ist ein mittleres Grau. Die Ausgabe potenziert mit Gamma, die Vorschau ebenfalls.
int byteOf(float v) { return static_cast<int>(std::lround(std::clamp(v, 0.0F, 1.0F) * 255.0F)); }

QColor toQColor(const ColorRgba& c) {
    QColor q;
    q.setRgb(byteOf(c.r), byteOf(c.g), byteOf(c.b));
    q.setAlpha(static_cast<int>(std::lround(std::clamp(c.a, 0.0F, 1.0F) * 255.0F)));
    return q;
}

ColorRgba fromQColor(const QColor& q) {
    return ColorRgba{static_cast<float>(q.red()) / 255.0F, static_cast<float>(q.green()) / 255.0F,
                     static_cast<float>(q.blue()) / 255.0F, static_cast<float>(q.alpha()) / 255.0F};
}

/// Farbfläche mit Schachbrett dahinter, damit man Transparenz sieht.
void paintSwatch(QPushButton* button, const ColorRgba& c) {
    const QSize size{72, 22};
    QPixmap pixmap{size};
    QPainter painter{&pixmap};
    constexpr int kCell = 6;
    for (int y = 0; y < size.height(); y += kCell) {
        for (int x = 0; x < size.width(); x += kCell) {
            const bool dark = ((x / kCell) + (y / kCell)) % 2 == 0;
            painter.fillRect(x, y, kCell, kCell, dark ? QColor{120, 120, 120} : QColor{190, 190, 190});
        }
    }
    painter.fillRect(pixmap.rect(), toQColor(c));
    painter.setPen(QColor{102, 102, 102});
    painter.drawRect(pixmap.rect().adjusted(0, 0, -1, -1));
    painter.end();
    button->setIcon(QIcon{pixmap});
    button->setIconSize(size);
    button->setToolTip(QStringLiteral("r %1  g %2  b %3  a %4")
                           .arg(static_cast<double>(c.r), 0, 'f', 3)
                           .arg(static_cast<double>(c.g), 0, 'f', 3)
                           .arg(static_cast<double>(c.b), 0, 'f', 3)
                           .arg(static_cast<double>(c.a), 0, 'f', 3));
}

Editor numberEditor(const runtime::ParameterInfo& info, const Commit& commit, QWidget* parent,
                    std::optional<std::array<double, 2>> sliderRange = std::nullopt) {
    auto* box = new QWidget{parent};
    auto* layout = new QHBoxLayout{box};
    layout->setContentsMargins(0, 0, 0, 0);
    // Schieberegler-Bereich: aus den Geschwister-Parametern "<name>Min"/"<name>Max" (einstellbar im
    // Node), sonst aus dem festen Bereich des Parameters, sonst gar kein Schieberegler.
    auto range = std::make_shared<std::array<double, 2>>(std::array<double, 2>{0.0, 1.0});
    bool sliding = false;
    if (sliderRange && (*sliderRange)[1] > (*sliderRange)[0]) {
        *range = *sliderRange;
        sliding = true;
    } else if (info.minValue && info.maxValue && *info.maxValue > *info.minValue) {
        *range = {*info.minValue, *info.maxValue};
        sliding = true;
    }
    const double lo = info.minValue.value_or(-1e9);
    const double hi = info.maxValue.value_or(1e9);
    const bool degrees = info.displayName.find("(deg)") != std::string::npos;
    auto* spin = makeSpin(box, lo, hi, sliding ? niceStep((*range)[1] - (*range)[0]) : (degrees ? 5.0 : 1.0));
    spin->setAccelerated(true);  // gedrückt halten wird schneller
    QSlider* slider = nullptr;
    if (sliding) {
        slider = new QSlider{Qt::Horizontal, box};
        slider->setRange(0, 1000);
        layout->addWidget(slider, 1);
    }
    layout->addWidget(spin);

    const auto sliderOf = [range](double v) {
        const double t = ((v - (*range)[0]) / ((*range)[1] - (*range)[0])) * 1000.0;
        return static_cast<int>(std::lround(std::clamp(t, 0.0, 1000.0)));
    };
    if (slider != nullptr) {
        QObject::connect(slider, &QSlider::valueChanged, spin, [=](int v) {
            const double value = (*range)[0] + ((*range)[1] - (*range)[0]) * static_cast<double>(v) / 1000.0;
            {
                const QSignalBlocker block{spin};
                spin->setValue(value);
            }
            commit(spin->value());
        });
    }
    QObject::connect(spin, &QDoubleSpinBox::valueChanged, spin, [=](double v) {
        if (slider != nullptr) {
            const QSignalBlocker block{slider};
            slider->setValue(sliderOf(v));
        }
        commit(v);
    });

    Editor editor;
    editor.widget = box;
    editor.show = [=](const graph::ParameterValue& value) {
        const double v = std::get<double>(value);
        const QSignalBlocker blockSpin{spin};
        spin->setValue(v);
        if (slider != nullptr) {
            const QSignalBlocker blockSlider{slider};
            slider->setValue(sliderOf(v));
        }
    };
    editor.setSliderRange = [=](double a, double b) {
        if (slider == nullptr || !(b > a)) {
            return;
        }
        *range = {a, b};
        spin->setSingleStep(niceStep(b - a));
        const QSignalBlocker blockSlider{slider};
        slider->setValue(sliderOf(spin->value()));
    };
    editor.show(info.value);
    return editor;
}

Editor choiceEditor(const runtime::ParameterInfo& info, const Commit& commit, QWidget* parent) {
    auto* combo = new QComboBox{parent};
    for (const std::string& label : info.choices) {
        combo->addItem(QString::fromStdString(label));
    }
    QObject::connect(combo, &QComboBox::activated, combo,
                     [=](int index) { commit(static_cast<std::int64_t>(index)); });
    Editor editor;
    editor.widget = combo;
    editor.show = [=](const graph::ParameterValue& value) {
        const QSignalBlocker block{combo};
        combo->setCurrentIndex(static_cast<int>(std::get<std::int64_t>(value)));
    };
    editor.show(info.value);
    return editor;
}

Editor integerEditor(const runtime::ParameterInfo& info, const Commit& commit, QWidget* parent) {
    if (!info.choices.empty()) {
        return choiceEditor(info, commit, parent);
    }
    auto* spin = new QSpinBox{parent};
    const auto limit = [](std::optional<double> v, int fallback) {
        return v ? static_cast<int>(std::clamp(*v, -2e9, 2e9)) : fallback;
    };
    spin->setRange(limit(info.minValue, std::numeric_limits<int>::min() / 2),
                   limit(info.maxValue, std::numeric_limits<int>::max() / 2));
    QObject::connect(spin, &QSpinBox::valueChanged, spin, [=](int v) { commit(static_cast<std::int64_t>(v)); });
    Editor editor;
    editor.widget = spin;
    editor.show = [=](const graph::ParameterValue& value) {
        const QSignalBlocker block{spin};
        spin->setValue(static_cast<int>(std::get<std::int64_t>(value)));
    };
    editor.show(info.value);
    return editor;
}

Editor boolEditor(const runtime::ParameterInfo& info, const Commit& commit, QWidget* parent) {
    auto* check = new QCheckBox{parent};
    QObject::connect(check, &QCheckBox::toggled, check, [=](bool on) { commit(on); });
    Editor editor;
    editor.widget = check;
    editor.show = [=](const graph::ParameterValue& value) {
        const QSignalBlocker block{check};
        check->setChecked(std::get<bool>(value));
    };
    editor.show(info.value);
    return editor;
}

/// Zwei oder drei Zahlenfelder nebeneinander.
Editor vectorEditor(const runtime::ParameterInfo& info, const Commit& commit, QWidget* parent, int components) {
    auto* box = new QWidget{parent};
    auto* layout = new QHBoxLayout{box};
    layout->setContentsMargins(0, 0, 0, 0);
    auto spins = std::make_shared<std::vector<QDoubleSpinBox*>>();
    for (int i = 0; i < components; ++i) {
        spins->push_back(makeSpin(box, -1e6, 1e6, 0.1));
        layout->addWidget(spins->back());
    }
    const auto read = [spins, components]() -> graph::ParameterValue {
        const auto at = [&](int i) { return static_cast<float>((*spins)[static_cast<std::size_t>(i)]->value()); };
        if (components == 2) {
            return Vec2{at(0), at(1)};
        }
        return Vec3{at(0), at(1), at(2)};
    };
    for (QDoubleSpinBox* spin : *spins) {
        QObject::connect(spin, &QDoubleSpinBox::valueChanged, spin, [=](double) { commit(read()); });
    }
    Editor editor;
    editor.widget = box;
    editor.show = [spins, components](const graph::ParameterValue& value) {
        float v[3] = {0, 0, 0};
        if (components == 2) {
            const Vec2 p = std::get<Vec2>(value);
            v[0] = p.x;
            v[1] = p.y;
        } else {
            const Vec3 p = std::get<Vec3>(value);
            v[0] = p.x;
            v[1] = p.y;
            v[2] = p.z;
        }
        for (int i = 0; i < components; ++i) {
            const QSignalBlocker block{(*spins)[static_cast<std::size_t>(i)]};
            (*spins)[static_cast<std::size_t>(i)]->setValue(static_cast<double>(v[i]));
        }
    };
    editor.show(info.value);
    return editor;
}

Editor colorEditor(const runtime::ParameterInfo& info, const Commit& commit, QWidget* parent) {
    auto* box = new QWidget{parent};
    auto* layout = new QHBoxLayout{box};
    layout->setContentsMargins(0, 0, 0, 0);
    auto* button = new QPushButton{box};
    auto* transparent = new QCheckBox{QStringLiteral("Transparent"), box};
    transparent->setToolTip(QStringLiteral("Alpha 0: an dieser Stelle bleibt die LED aus (die Farbe bleibt gemerkt)."));
    layout->addWidget(button);
    layout->addWidget(transparent);
    layout->addStretch(1);

    auto current = std::make_shared<ColorRgba>(std::get<ColorRgba>(info.value));
    QObject::connect(button, &QPushButton::clicked, button, [=]() {
        const QColor picked = QColorDialog::getColor(toQColor(*current), button, QStringLiteral("Farbe"),
                                                     QColorDialog::ShowAlphaChannel);
        if (picked.isValid()) {
            commit(fromQColor(picked));
        }
    });
    QObject::connect(transparent, &QCheckBox::toggled, transparent, [=](bool on) {
        ColorRgba next = *current;
        next.a = on ? 0.0F : 1.0F;  // Alpha dazwischen stellt man in der Farbwahl ein
        commit(next);
    });
    Editor editor;
    editor.widget = box;
    editor.show = [=](const graph::ParameterValue& value) {
        *current = std::get<ColorRgba>(value);
        paintSwatch(button, *current);
        const QSignalBlocker block{transparent};
        transparent->setChecked(current->a <= 0.0F);
    };
    editor.show(info.value);
    return editor;
}

Editor textEditor(const runtime::ParameterInfo& info, const Commit& commit, QWidget* parent) {
    auto* line = new QLineEdit{parent};
    QObject::connect(line, &QLineEdit::editingFinished, line, [=]() { commit(line->text().toUtf8().toStdString()); });
    Editor editor;
    editor.widget = line;
    editor.show = [=](const graph::ParameterValue& value) {
        const QSignalBlocker block{line};
        line->setText(fromStd(std::get<std::string>(value)));
    };
    editor.show(info.value);
    return editor;
}

}  // namespace

Editor makeEditor(const runtime::ParameterInfo& info, const Commit& commit, QWidget* parent,
                  std::optional<std::array<double, 2>> sliderRange) {
    switch (info.value.index()) {
        case 0: return boolEditor(info, commit, parent);
        case 1: return integerEditor(info, commit, parent);
        case 2: return numberEditor(info, commit, parent, sliderRange);
        case 3: return vectorEditor(info, commit, parent, 2);
        case 4: return vectorEditor(info, commit, parent, 3);
        case 5: return colorEditor(info, commit, parent);
        default: return textEditor(info, commit, parent);
    }
}

}  // namespace whitehole::studio
