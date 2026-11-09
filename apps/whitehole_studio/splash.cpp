// splash.cpp: Umsetzung zu splash.hpp.
#include "splash.hpp"

#include <QColor>
#include <QObject>
#include <QFont>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QRect>
#include <QApplication>

namespace whitehole::studio {

namespace {

constexpr int kWidth = 520;
constexpr int kHeight = 300;
constexpr int kLogo = 128;

}  // namespace

QPixmap makeSplashPixmap(const QString& version) {
    QPixmap pixmap{kWidth, kHeight};
    pixmap.fill(Qt::transparent);
    QPainter painter{&pixmap};
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QLinearGradient background{0.0, 0.0, kWidth, kHeight};
    background.setColorAt(0.0, QColor{0x1b, 0x1d, 0x27});
    background.setColorAt(1.0, QColor{0x0b, 0x0c, 0x11});
    painter.setPen(QColor{0x3a, 0x3d, 0x4d});
    painter.setBrush(background);
    painter.drawRoundedRect(QRectF{0.5, 0.5, kWidth - 1.0, kHeight - 1.0}, 14.0, 14.0);

    const QPixmap logo{QStringLiteral(":/icons/app-256.png")};
    if (!logo.isNull()) {
        painter.drawPixmap(QRect{36, (kHeight - kLogo) / 2 - 14, kLogo, kLogo}, logo);
    }

    const int textLeft = 36 + kLogo + 32;
    painter.setPen(QColor{0xf2, 0xf2, 0xf5});
    QFont title{painter.font()};
    title.setPointSize(26);
    title.setBold(true);
    painter.setFont(title);
    painter.drawText(QRect{textLeft, 96, kWidth - textLeft - 24, 44}, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("WhiteHoleFX"));

    painter.setPen(QColor{0xff, 0xa0, 0x40});
    QFont sub{painter.font()};
    sub.setPointSize(11);
    sub.setBold(false);
    painter.setFont(sub);
    painter.drawText(QRect{textLeft, 142, kWidth - textLeft - 24, 24}, Qt::AlignLeft | Qt::AlignVCenter,
                     QObject::tr("Version %1").arg(version));
    painter.setPen(QColor{0x9a, 0x9d, 0xb0});
    painter.drawText(QRect{textLeft, 166, kWidth - textLeft - 24, 24}, Qt::AlignLeft | Qt::AlignVCenter,
                     QObject::tr("LED-Steuerung für Matrizen, Cubes und Pixelinstallationen"));
    painter.end();
    return pixmap;
}

StartSplash::StartSplash(const QString& version) : QSplashScreen{makeSplashPixmap(version), Qt::WindowStaysOnTopHint} {}

void StartSplash::step(const QString& text) {
    step_ = text;
    repaint();
    QApplication::processEvents();
}

void StartSplash::drawContents(QPainter* painter) {
    painter->setPen(QColor{0x9a, 0x9d, 0xb0});
    painter->drawText(QRect{36, kHeight - 52, kWidth - 72, 28}, Qt::AlignLeft | Qt::AlignVCenter, step_);
}

QString splashProblem() {
    const QPixmap pixmap = makeSplashPixmap(QStringLiteral("0.0.0"));
    if (pixmap.width() != kWidth || pixmap.height() != kHeight) {
        return QStringLiteral("Startbild: falsche Größe %1x%2").arg(pixmap.width()).arg(pixmap.height());
    }
    StartSplash splash{QStringLiteral("0.0.0")};  // nur anlegen, nicht zeigen
    if (splash.pixmap().size() != pixmap.size()) {
        return QStringLiteral("Startbild: der Startbildschirm hat nicht die Größe des Bildes");
    }
    if (QPixmap{QStringLiteral(":/icons/app-256.png")}.isNull()) {
        return QStringLiteral("Startbild: das Logo fehlt in den eingebetteten Dateien");
    }
    // Das Logo (orange Leuchtring) und der Text (hell) müssen im Bild vorkommen.
    const QImage image = pixmap.toImage();
    bool orange = false;
    bool bright = false;
    for (int y = 0; y < image.height() && !(orange && bright); y += 2) {
        for (int x = 0; x < image.width(); x += 2) {
            const QColor c = image.pixelColor(x, y);
            orange = orange || (c.red() > 200 && c.green() > 90 && c.green() < 190 && c.blue() < 80);
            bright = bright || (c.red() > 230 && c.green() > 230 && c.blue() > 230);
        }
    }
    if (!orange || !bright) {
        return QStringLiteral("Startbild: Logo oder Text fehlen im Bild (Logo %1, Text %2)").arg(orange).arg(bright);
    }
    return {};
}

}  // namespace whitehole::studio
