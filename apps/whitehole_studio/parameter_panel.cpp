// parameter_panel.cpp: Bedienelemente je Parametertyp.
#include "parameter_panel.hpp"

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <type_traits>
#include <variant>

#include "whitehole/preview/preview.hpp"

namespace whitehole::studio {

namespace {

using Commit = std::function<void(graph::ParameterValue)>;
using Show = std::function<void(const graph::ParameterValue&)>;

struct Editor {
    QWidget* widget{nullptr};
    Show show;  // zeigt einen (gespeicherten) Wert an, ohne Änderungssignale auszulösen
};

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

QDoubleSpinBox* makeSpin(QWidget* parent, double min, double max, double step) {
    auto* spin = new QDoubleSpinBox{parent};
    spin->setDecimals(3);
    spin->setRange(min, max);
    spin->setSingleStep(step);
    return spin;
}

QColor toQColor(const ColorRgba& c) {
    QColor q;
    q.setRgb(preview::toSrgb8(c.r), preview::toSrgb8(c.g), preview::toSrgb8(c.b));
    q.setAlpha(static_cast<int>(std::lround(std::clamp(c.a, 0.0F, 1.0F) * 255.0F)));
    return q;
}

ColorRgba fromQColor(const QColor& q) {
    return ColorRgba{preview::fromSrgb8(static_cast<std::uint8_t>(q.red())),
                     preview::fromSrgb8(static_cast<std::uint8_t>(q.green())),
                     preview::fromSrgb8(static_cast<std::uint8_t>(q.blue())), static_cast<float>(q.alpha()) / 255.0F};
}

void paintSwatch(QPushButton* button, const ColorRgba& c) {
    const QColor q = toQColor(c);
    button->setStyleSheet(QStringLiteral("background-color: rgb(%1,%2,%3); border: 1px solid #666; min-height: 22px;")
                              .arg(q.red())
                              .arg(q.green())
                              .arg(q.blue()));
    button->setToolTip(QStringLiteral("r %1  g %2  b %3  a %4 (linear)")
                           .arg(static_cast<double>(c.r), 0, 'f', 3)
                           .arg(static_cast<double>(c.g), 0, 'f', 3)
                           .arg(static_cast<double>(c.b), 0, 'f', 3)
                           .arg(static_cast<double>(c.a), 0, 'f', 3));
}

Editor numberEditor(const runtime::ParameterInfo& info, const Commit& commit, QWidget* parent) {
    auto* box = new QWidget{parent};
    auto* layout = new QHBoxLayout{box};
    layout->setContentsMargins(0, 0, 0, 0);
    const bool ranged = info.minValue && info.maxValue && *info.maxValue > *info.minValue;
    const double lo = info.minValue.value_or(-1e9);
    const double hi = info.maxValue.value_or(1e9);
    auto* spin = makeSpin(box, lo, hi, ranged ? (hi - lo) / 100.0 : 0.1);
    QSlider* slider = nullptr;
    if (ranged) {
        slider = new QSlider{Qt::Horizontal, box};
        slider->setRange(0, 1000);
        layout->addWidget(slider, 1);
    }
    layout->addWidget(spin);

    const auto sliderOf = [lo, hi](double v) { return static_cast<int>(std::lround((v - lo) / (hi - lo) * 1000.0)); };
    if (slider != nullptr) {
        QObject::connect(slider, &QSlider::valueChanged, spin, [=](int v) {
            const double value = lo + (hi - lo) * static_cast<double>(v) / 1000.0;
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
    editor.show(info.value);
    return editor;
}

Editor integerEditor(const runtime::ParameterInfo& info, const Commit& commit, QWidget* parent) {
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
    auto* button = new QPushButton{parent};
    auto current = std::make_shared<ColorRgba>(std::get<ColorRgba>(info.value));
    QObject::connect(button, &QPushButton::clicked, button, [=]() {
        const QColor picked = QColorDialog::getColor(toQColor(*current), button, QStringLiteral("Farbe"),
                                                     QColorDialog::ShowAlphaChannel);
        if (picked.isValid()) {
            commit(fromQColor(picked));
        }
    });
    Editor editor;
    editor.widget = button;
    editor.show = [=](const graph::ParameterValue& value) {
        *current = std::get<ColorRgba>(value);
        paintSwatch(button, *current);
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

Editor makeEditor(const runtime::ParameterInfo& info, const Commit& commit, QWidget* parent) {
    switch (info.value.index()) {
        case 0: return boolEditor(info, commit, parent);
        case 1: return integerEditor(info, commit, parent);
        case 2: return numberEditor(info, commit, parent);
        case 3: return vectorEditor(info, commit, parent, 2);
        case 4: return vectorEditor(info, commit, parent, 3);
        case 5: return colorEditor(info, commit, parent);
        default: return textEditor(info, commit, parent);
    }
}

}  // namespace

ParameterPanel::ParameterPanel(QWidget* parent) : QScrollArea{parent} {
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    auto* hint = new QLabel{tr("Kein Projekt geladen."), this};
    hint->setMargin(12);
    setWidget(hint);
}

void ParameterPanel::rebuild(const std::vector<runtime::NodeInfo>& nodes) {
    auto* content = new QWidget;
    auto* column = new QVBoxLayout{content};
    if (nodes.empty()) {
        column->addWidget(new QLabel{tr("Dieses Projekt hat keine einstellbaren Parameter."), content});
    }
    for (const runtime::NodeInfo& node : nodes) {
        auto* group = new QGroupBox{fromStd(node.title), content};
        auto* grid = new QGridLayout{group};
        int row = 0;
        for (const runtime::ParameterInfo& info : node.parameters) {
            auto holder = std::make_shared<Editor>();
            const NodeId id = node.id;
            const std::string key = info.key;
            const Commit commit = [this, holder, id, key](graph::ParameterValue value) {
                if (!handler_) {
                    return;
                }
                if (const auto stored = handler_(id, key, std::move(value))) {
                    holder->show(*stored);
                }
            };
            *holder = makeEditor(info, commit, group);

            auto* label = new QLabel{fromStd(info.displayName), group};
            auto* reset = new QToolButton{group};
            reset->setText(QStringLiteral("↺"));
            reset->setToolTip(tr("Auf Standardwert zurücksetzen"));
            const graph::ParameterValue defaultValue = info.defaultValue;
            QObject::connect(reset, &QToolButton::clicked, reset, [commit, defaultValue]() { commit(defaultValue); });

            if (info.driven) {
                holder->widget->setEnabled(false);
                reset->setEnabled(false);
                label->setToolTip(tr("Wird durch eine Verbindung im Graphen gesteuert."));
                holder->widget->setToolTip(label->toolTip());
            }
            grid->addWidget(label, row, 0);
            grid->addWidget(holder->widget, row, 1);
            grid->addWidget(reset, row, 2);
            ++row;
        }
        grid->setColumnStretch(1, 1);
        column->addWidget(group);
    }
    column->addStretch(1);
    setWidget(content);  // übernimmt den Besitz und löscht den alten Inhalt
}

}  // namespace whitehole::studio
