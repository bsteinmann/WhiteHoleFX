// look_button.cpp: Look-Knopf mit großem Vorschaubild (Begründung siehe look_button.hpp).
#include "look_button.hpp"

#include <QFontMetrics>
#include <QPixmap>
#include <QStyleOptionButton>
#include <QStylePainter>

#include <algorithm>
#include <cmath>

namespace whitehole::studio {

namespace {
constexpr int kMargin = 6;  // Rand zwischen Knopfrahmen und Bild
}

QSize lookIconSize(QSize button, QSize image, int textHeight) {
    if (image.width() <= 0 || image.height() <= 0) {
        return {};
    }
    const double maxW = std::max(1, button.width() - 2 * kMargin);
    const double maxH = std::max(1, button.height() - 2 * kMargin - textHeight);
    const double scale = std::min(maxW / image.width(), maxH / image.height());
    return {std::max(1, static_cast<int>(image.width() * scale)), std::max(1, static_cast<int>(image.height() * scale))};
}

QString checkLookIconSize() {
    const QSize image{96, 54};
    const QSize small = lookIconSize(QSize{200, 100}, image, 24);
    const QSize big = lookIconSize(QSize{400, 200}, image, 24);
    if (big.width() <= small.width() || big.height() <= small.height()) {
        return QStringLiteral("Live: Das Vorschaubild wächst nicht mit dem Knopf (%1x%2 -> %3x%4)")
            .arg(small.width()).arg(small.height()).arg(big.width()).arg(big.height());
    }
    if (std::abs(static_cast<double>(big.width()) / big.height() - 96.0 / 54.0) > 0.1 || big.width() > 400 - 2 * kMargin ||
        big.height() > 200 - 2 * kMargin - 24) {
        return QStringLiteral("Live: Das Vorschaubild verliert sein Seitenverhältnis oder ragt über den Knopf");
    }
    if (small.width() * 2 < 200 - 2 * kMargin - 30 && small.height() * 2 < 100 - 2 * kMargin - 24 - 30) {
        return QStringLiteral("Live: Das Vorschaubild füllt die Knopffläche nicht");
    }
    if (lookIconSize(QSize{200, 72}, QSize{}, 24).isValid()) {
        return QStringLiteral("Live: Ohne Vorschaubild darf es keine Bildgröße geben");
    }
    return {};
}

LookButton::LookButton(const QString& text, QWidget* parent) : QPushButton{text, parent} {}

void LookButton::paintEvent(QPaintEvent* event) {
    if (icon().isNull()) {
        QPushButton::paintEvent(event);
        return;
    }
    // Rahmen und Hintergrund vom Stil (auch vom Stylesheet), ohne Icon und Text
    QStylePainter painter{this};
    QStyleOptionButton option;
    initStyleOption(&option);
    const QString label = option.text;
    const QIcon image = option.icon;
    option.text.clear();
    option.icon = QIcon{};
    painter.drawControl(QStyle::CE_PushButton, option);

    const int textHeight = fontMetrics().height() + 4;
    const QSize source = image.availableSizes().value(0, QSize{96, 54});
    const QSize target = lookIconSize(size(), source, textHeight);
    const QRect area{kMargin, kMargin, width() - 2 * kMargin, std::max(1, height() - 2 * kMargin - textHeight)};
    const QPixmap pixmap = image.pixmap(source).scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    painter.drawPixmap(QRect{area.center() - QPoint{target.width() / 2, target.height() / 2}, target}, pixmap);

    painter.setPen(palette().buttonText().color());
    const QRect textArea{kMargin, height() - kMargin - textHeight, width() - 2 * kMargin, textHeight};
    painter.drawText(textArea, Qt::AlignCenter, fontMetrics().elidedText(label, Qt::ElideRight, textArea.width()));
}

}  // namespace whitehole::studio
