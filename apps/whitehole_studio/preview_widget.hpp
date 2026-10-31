// preview_widget.hpp: zeichnet das Vorschaubild samt Auswahl- und Bereichsrahmen und meldet Mausereignisse.
//
// Das Widget kennt weder Projekt noch GPU. Es zeigt ein QImage und meldet, wenn sich seine Größe
// ändert, damit das Hauptfenster ein Bild in passender Größe nachliefert (der Bildaufbau
// rechnet in Pixeln, ein skaliertes Bild wäre unscharf). Über das Bild legt es Rahmen
// (`Overlay`), die der PreviewController vorgibt; Maus und Pfeiltasten gibt es als Signale in
// Bildpunkten weiter, die Bedeutung (Fixture ziehen, Bereich aufspannen) liegt im Controller.
#pragma once

#include <QImage>
#include <QList>
#include <QRectF>
#include <QWidget>

namespace whitehole::studio {

/// Was über dem Bild gezeichnet wird, alles in Bildpunkten des Widgets.
struct PreviewOverlay {
    QList<QRectF> selected;      // Rahmen der gewählten Fixtures
    QRectF rubberBand;           // Auswahlrahmen beim Aufziehen; leer = keiner
    QRectF wall;                 // die ganze Leinwand (nur beim Bearbeiten des Bereichs gezeichnet)
    QRectF region;               // Rechteck des gewählten Region-Nodes; leer = keiner
    bool editingRegion{false};   // Region-Node gewählt: Griffe an Ecken und Mitte, Leinwand angedeutet
};

class PreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit PreviewWidget(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setMessage(const QString& message);
    void setOverlay(const PreviewOverlay& overlay);
    /// Auffälliger Hinweis oben rechts (z. B. "TESTMODUS"); leer = keiner.
    void setBanner(const QString& banner);

    [[nodiscard]] QSize sizeHint() const override { return {960, 540}; }
    [[nodiscard]] QSize minimumSizeHint() const override { return {160, 90}; }

Q_SIGNALS:
    void resized(QSize size);
    /// Linke Maustaste gedrückt / bewegt (bei gedrückter Taste) / losgelassen; Punkte im Widget.
    void mousePressed(QPointF position, Qt::KeyboardModifiers modifiers);
    void mouseDragged(QPointF position);
    void mouseReleased(QPointF position);
    /// Pfeiltaste: `dx` nach rechts, `dy` nach oben (je -1, 0 oder 1).
    void nudged(int dx, int dy, Qt::KeyboardModifiers modifiers);
    /// Die Maus bewegt sich über der Vorschau (ohne gedrückte Taste); `hoverEnded`, wenn sie sie verlässt.
    void hovered(QPointF position);
    void hoverEnded();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QImage image_;
    QString message_;
    QString banner_;
    PreviewOverlay overlay_;
    bool dragging_{false};
};

}  // namespace whitehole::studio
