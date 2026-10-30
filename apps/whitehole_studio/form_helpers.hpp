// form_helpers.hpp: kleine Bausteine für Formulare (Zahlenfelder, Auswahlfelder, Zeilen).
//
// Warum es diese Datei gibt: Fixture- und Ausgabeformular brauchen dieselben Handgriffe;
// hier stehen sie einmal. Die Felder melden Änderungen erst bei Enter, Pfeiltasten oder
// Fokuswechsel (KeyboardTracking aus), damit beim Tippen keine halben Zahlen im Projekt landen.
#pragma once

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <initializer_list>
#include <string>

#include "whitehole/core/types.hpp"

namespace whitehole::studio::form {

inline QString fromStd(const std::string& text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

inline QDoubleSpinBox* doubleBox(double value, double min, double max, double step = 1.0, int decimals = 3) {
    auto* box = new QDoubleSpinBox;
    box->setDecimals(decimals);
    box->setRange(min, max);
    box->setSingleStep(step);
    box->setValue(value);
    box->setKeyboardTracking(false);
    return box;
}

inline QSpinBox* intBox(int value, int min, int max) {
    auto* box = new QSpinBox;
    box->setRange(min, max);
    box->setValue(value);
    box->setKeyboardTracking(false);
    return box;
}

inline QComboBox* combo(const QStringList& items, int index) {
    auto* box = new QComboBox;
    box->addItems(items);
    box->setCurrentIndex(index);
    return box;
}

inline QWidget* rowOf(std::initializer_list<QWidget*> widgets) {
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout{row};
    layout->setContentsMargins(0, 0, 0, 0);
    for (QWidget* widget : widgets) {
        layout->addWidget(widget);
    }
    return row;
}

struct Vec3Boxes {
    QDoubleSpinBox* x{nullptr};
    QDoubleSpinBox* y{nullptr};
    QDoubleSpinBox* z{nullptr};

    [[nodiscard]] Vec3 value() const {
        return Vec3{static_cast<float>(x->value()), static_cast<float>(y->value()), static_cast<float>(z->value())};
    }
    [[nodiscard]] QWidget* widget() const { return rowOf({x, y, z}); }
};

struct Vec2Boxes {
    QDoubleSpinBox* x{nullptr};
    QDoubleSpinBox* y{nullptr};

    [[nodiscard]] Vec2 value() const { return Vec2{static_cast<float>(x->value()), static_cast<float>(y->value())}; }
    [[nodiscard]] QWidget* widget() const { return rowOf({x, y}); }
};

inline Vec3Boxes vec3Boxes(const Vec3& v, double min = -1.0e6, double max = 1.0e6, double step = 1.0) {
    return {doubleBox(v.x, min, max, step), doubleBox(v.y, min, max, step), doubleBox(v.z, min, max, step)};
}

inline Vec2Boxes vec2Boxes(const Vec2& v, double min, double max, double step = 0.5) {
    return {doubleBox(v.x, min, max, step), doubleBox(v.y, min, max, step)};
}

}  // namespace whitehole::studio::form
