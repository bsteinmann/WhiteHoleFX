#include "preview_widget.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QResizeEvent>

#include <algorithm>

namespace whitehole::studio {

namespace {

constexpr qreal kHandle = 5.0;  // halbe Kantenlänge der Eckgriffe

void drawFrame(QPainter& painter, const QRectF& rect, const QColor& color, Qt::PenStyle style, qreal width) {
    QPen pen{color};
    pen.setStyle(style);
    pen.setWidthF(width);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect);
}

}  // namespace

PreviewWidget::PreviewWidget(QWidget* parent) : QWidget{parent} {
    setAutoFillBackground(false);
    setFocusPolicy(Qt::ClickFocus);
    setMouseTracking(true);  // hovered() auch ohne gedrückte Taste
}

void PreviewWidget::setImage(const QImage& image) {
    image_ = image;
    update();
}

void PreviewWidget::setMessage(const QString& message) {
    message_ = message;
    update();
}

void PreviewWidget::setOverlay(const PreviewOverlay& overlay) {
    overlay_ = overlay;
    update();
}

void PreviewWidget::setBanner(const QString& banner) {
    banner_ = banner;
    update();
}

void PreviewWidget::paintEvent(QPaintEvent*) {
    QPainter painter{this};
    painter.fillRect(rect(), QColor{14, 14, 18});
    if (!image_.isNull()) {
        // Passt die Größe, wird 1:1 gezeichnet; bei einem kurzen Versatz nach dem Skalieren des
        // Fensters füllt das Bild einfach den Platz, bis das neue eintrifft.
        painter.drawImage(rect(), image_);
    }
    if (overlay_.editingRegion && !overlay_.wall.isEmpty()) {
        drawFrame(painter, overlay_.wall, QColor{140, 140, 150}, Qt::DotLine, 1.0);
    }
    for (const QRectF& rect : overlay_.selected) {
        drawFrame(painter, rect.adjusted(-3, -3, 3, 3), QColor{80, 200, 255}, Qt::DashLine, 1.5);
    }
    if (!overlay_.region.isEmpty()) {
        drawFrame(painter, overlay_.region, QColor{255, 170, 40}, Qt::SolidLine, 2.0);
        if (overlay_.editingRegion) {
            painter.setBrush(QColor{255, 170, 40});
            for (const QPointF& corner : {overlay_.region.topLeft(), overlay_.region.topRight(),
                                          overlay_.region.bottomLeft(), overlay_.region.bottomRight()}) {
                painter.drawRect(QRectF{corner.x() - kHandle, corner.y() - kHandle, 2 * kHandle, 2 * kHandle});
            }
            const QPointF c = overlay_.region.center();  // Griff zum Verschieben
            painter.drawEllipse(c, kHandle + 1.0, kHandle + 1.0);
        }
    }
    if (!overlay_.rubberBand.isEmpty()) {
        painter.fillRect(overlay_.rubberBand, QColor{80, 200, 255, 40});
        drawFrame(painter, overlay_.rubberBand, QColor{80, 200, 255}, Qt::SolidLine, 1.0);
    }
    if (!banner_.isEmpty()) {
        QFont font = painter.font();
        font.setBold(true);
        font.setPointSizeF(font.pointSizeF() * 1.3);
        painter.setFont(font);
        const QFontMetrics metrics{font};
        const QRect text = metrics.boundingRect(banner_).adjusted(-12, -6, 12, 6);
        const QRect box{width() - text.width() - 12, 12, text.width(), text.height()};
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor{200, 30, 30});
        painter.drawRoundedRect(box, 6, 6);
        painter.setPen(Qt::white);
        painter.drawText(box, Qt::AlignCenter, banner_);
    }
    if (!message_.isEmpty()) {
        painter.setPen(QColor{200, 200, 205});
        painter.drawText(rect().adjusted(16, 16, -16, -16), Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap,
                         message_);
    }
}

void PreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    Q_EMIT resized(event->size());
}

void PreviewWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    setFocus();
    dragging_ = true;
    Q_EMIT mousePressed(event->position(), event->modifiers());
}

void PreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_) {
        // Auch außerhalb des Widgets weiterverfolgen, aber im Bild halten.
        const QPointF p{std::clamp(event->position().x(), 0.0, static_cast<qreal>(width())),
                        std::clamp(event->position().y(), 0.0, static_cast<qreal>(height()))};
        Q_EMIT mouseDragged(p);
        return;
    }
    Q_EMIT hovered(event->position());
}

void PreviewWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    Q_EMIT hoverEnded();
}

void PreviewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        const QPointF p{std::clamp(event->position().x(), 0.0, static_cast<qreal>(width())),
                        std::clamp(event->position().y(), 0.0, static_cast<qreal>(height()))};
        Q_EMIT mouseReleased(p);
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void PreviewWidget::keyPressEvent(QKeyEvent* event) {
    int dx = 0;
    int dy = 0;
    switch (event->key()) {
        case Qt::Key_Left: dx = -1; break;
        case Qt::Key_Right: dx = 1; break;
        case Qt::Key_Up: dy = 1; break;
        case Qt::Key_Down: dy = -1; break;
        default: QWidget::keyPressEvent(event); return;
    }
    Q_EMIT nudged(dx, dy, event->modifiers());
}

}  // namespace whitehole::studio
