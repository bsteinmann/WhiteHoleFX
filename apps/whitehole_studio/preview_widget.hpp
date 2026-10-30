// preview_widget.hpp: zeichnet das Vorschaubild.
//
// Das Widget kennt weder Projekt noch GPU. Es zeigt ein QImage und meldet, wenn sich seine Größe
// ändert, damit das Hauptfenster ein Bild in passender Größe nachliefert (der Bildaufbau
// rechnet in Pixeln, ein skaliertes Bild wäre unscharf).
#pragma once

#include <QImage>
#include <QWidget>

namespace whitehole::studio {

class PreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit PreviewWidget(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setMessage(const QString& message);

    [[nodiscard]] QSize sizeHint() const override { return {960, 540}; }
    [[nodiscard]] QSize minimumSizeHint() const override { return {160, 90}; }

Q_SIGNALS:
    void resized(QSize size);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QImage image_;
    QString message_;
};

}  // namespace whitehole::studio
