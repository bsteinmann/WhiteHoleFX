// main_window_split_preview.cpp: die geteilte Vorschau des Live-Betriebs (Teil des Hauptfensters).
//
// Warum es diese Datei gibt: Im Live-Betrieb will man neben dem, was gerade rausgeht, auch den vorgeladenen Look laufen
// sehen, bevor man GO drückt. Der Player liefert dafür ein zweites Bild (Player::previewImage); hier werden beide in
// halber Breite nebeneinander gesetzt und beschriftet. Ausgelagert wegen der Dateigröße von main_window.cpp.
#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QRect>

#include <algorithm>

#include "main_window.hpp"
#include "preview_widget.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

constexpr int kGap = 4;  // Trennstrich zwischen den Hälften

QImage toQImage(const preview::Image& img) {
    return QImage{img.rgba.data(), static_cast<int>(img.width), static_cast<int>(img.height),
                  static_cast<qsizetype>(img.width) * 4, QImage::Format_RGBA8888};
}

void drawLabel(QPainter& painter, const QRect& half, const QString& text, const QColor& accent) {
    QFont font = painter.font();
    font.setBold(true);
    painter.setFont(font);
    const QRect box = painter.fontMetrics().boundingRect(text).adjusted(-8, -4, 8, 4);
    const QRect placed{half.left() + 8, half.top() + 8, box.width(), box.height()};
    painter.fillRect(placed, QColor{0, 0, 0, 170});
    painter.fillRect(QRect{placed.left(), placed.top(), 3, placed.height()}, accent);
    painter.setPen(Qt::white);
    painter.drawText(placed, Qt::AlignCenter, text);
}

}  // namespace

void MainWindow::refreshSplitImage(const preview::PreviewOptions& given) {
    const int width = static_cast<int>(given.width);
    const int height = static_cast<int>(given.height);
    preview::PreviewOptions half = given;
    half.width = static_cast<std::uint32_t>(std::max(32, (width - kGap) / 2));
    half.frame.reset();

    const bool view3d = preview_->view3d();
    const auto output = view3d ? player_->image3d(preview_->camera(), half) : player_->image(half);
    if (!output.ok()) {
        preview_->setMessage(fromStd(output.error().message));
        return;
    }
    const auto armed = view3d ? player_->previewImage3d(preview_->camera(), half) : player_->previewImage(half);

    QImage canvas{width, height, QImage::Format_RGB32};
    canvas.fill(QColor{0x18, 0x19, 0x1d});
    QPainter painter{&canvas};
    const QRect left{0, 0, static_cast<int>(half.width), height};
    const QRect right{static_cast<int>(half.width) + kGap, 0, width - static_cast<int>(half.width) - kGap, height};
    painter.drawImage(left.topLeft(), toQImage(output.value()));
    painter.fillRect(QRect{left.right() + 1, 0, kGap, height}, QColor{0x3a, 0x3c, 0x44});

    const std::optional<int> look = player_->previewLook();
    QString rightText;
    if (look) {
        const QString name = *look >= 0 && static_cast<std::size_t>(*look) < player_->looks().size()
                                 ? fromStd(player_->looks()[static_cast<std::size_t>(*look)].name)
                                 : tr("Schwarz");
        rightText = player_->liveStatus().fading ? tr("Blendet ein: %1").arg(name) : tr("Vorgeladen: %1").arg(name);
    }
    if (armed.ok()) {
        painter.drawImage(right.topLeft(), toQImage(armed.value()));
    } else {
        painter.setPen(QColor{0x9a, 0x9c, 0xa4});
        painter.drawText(right, Qt::AlignCenter, look ? tr("Wird geladen …") : tr("Kein Look vorgeladen"));
    }
    drawLabel(painter, left, tr("Ausgabe"), QColor{0x1f, 0x9d, 0x4c});
    if (look) {
        drawLabel(painter, right, rightText, QColor{0xd9, 0x82, 0x2b});
    }
    painter.end();
    preview_->setImage(canvas);
}

}  // namespace whitehole::studio
