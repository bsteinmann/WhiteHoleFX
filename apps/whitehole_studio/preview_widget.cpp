#include "preview_widget.hpp"

#include <QPainter>
#include <QResizeEvent>

namespace whitehole::studio {

PreviewWidget::PreviewWidget(QWidget* parent) : QWidget{parent} {
    setAutoFillBackground(false);
}

void PreviewWidget::setImage(const QImage& image) {
    image_ = image;
    update();
}

void PreviewWidget::setMessage(const QString& message) {
    message_ = message;
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
    if (!message_.isEmpty()) {
        painter.setPen(QColor{200, 200, 205});
        painter.drawText(rect().adjusted(16, 16, -16, -16), Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap,
                         message_);
    }
}

void PreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    emit resized(event->size());
}

}  // namespace whitehole::studio
