// look_button.cpp: Look-Knopf mit mitwachsendem Vorschaubild (Begründung siehe look_button.hpp).
#include "look_button.hpp"

#include <QResizeEvent>

#include <algorithm>
#include <cmath>

namespace whitehole::studio {

QSize lookIconSize(QSize button, QSize image, double maxWidthShare) {
    if (image.width() <= 0 || image.height() <= 0) {
        return {};
    }
    constexpr int kMargin = 12;  // Rand des Knopfs (Rahmen und Polster)
    const double maxW = std::max(1.0, button.width() * maxWidthShare);
    const double maxH = std::max(1.0, static_cast<double>(button.height() - kMargin));
    const double scale = std::min(maxW / image.width(), maxH / image.height());
    return {std::max(1, static_cast<int>(image.width() * scale)), std::max(1, static_cast<int>(image.height() * scale))};
}

QString checkLookIconSize() {
    const QSize image{32, 18};
    const QSize small = lookIconSize(QSize{200, 72}, image);
    const QSize big = lookIconSize(QSize{400, 144}, image);
    if (big.width() <= small.width() || big.height() <= small.height()) {
        return QStringLiteral("Live: Das Vorschaubild wächst nicht mit dem Knopf (%1x%2 -> %3x%4)")
            .arg(small.width()).arg(small.height()).arg(big.width()).arg(big.height());
    }
    if (std::abs(static_cast<double>(big.width()) / big.height() - 32.0 / 18.0) > 0.1 || big.width() > 400 * 0.55 + 1) {
        return QStringLiteral("Live: Das Vorschaubild verliert sein Seitenverhältnis oder nimmt zu viel Breite");
    }
    if (lookIconSize(QSize{200, 72}, QSize{}).isValid()) {
        return QStringLiteral("Live: Ohne Vorschaubild darf es keine Icon-Größe geben");
    }
    return {};
}

LookButton::LookButton(const QString& text, QWidget* parent) : QPushButton{text, parent} {}

void LookButton::setImageSize(QSize image) {
    image_ = image;
    fitIcon();
}

void LookButton::resizeEvent(QResizeEvent* event) {
    QPushButton::resizeEvent(event);
    fitIcon();
}

void LookButton::fitIcon() {
    if (const QSize size = lookIconSize(this->size(), image_); size.isValid()) {
        setIconSize(size);
    }
}

}  // namespace whitehole::studio
