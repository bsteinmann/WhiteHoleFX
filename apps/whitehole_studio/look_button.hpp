// look_button.hpp: Look-Knopf im Live-Fenster, dessen Vorschaubild mit dem Knopf mitwächst.
//
// Warum es diese Datei gibt: Ein QPushButton zeichnet sein Icon in fester Größe (iconSize), auch wenn der Knopf durch
// Fenstergröße oder Rasterbreite viel größer wird - das Vorschaubild blieb winzig (Rückmeldung vom 25.11.). LookButton
// passt die Icon-Größe bei jeder Größenänderung an die Knopffläche an und behält dabei das Seitenverhältnis des Bildes.
#pragma once

#include <QPushButton>
#include <QSize>
#include <QString>

namespace whitehole::studio {

// Größe des Vorschaubilds im Knopf: so groß wie die Fläche erlaubt (Seitenverhältnis des Bildes bleibt), höchstens
// `maxWidthShare` der Knopfbreite, damit der Text daneben Platz behält. Reine Funktion, damit sie sich testen lässt.
[[nodiscard]] QSize lookIconSize(QSize button, QSize image, double maxWidthShare = 0.55);

// Selbstprüfung für den Studio-Rauchtest (Qt läuft nur in der CI): leer = in Ordnung, sonst die Fehlermeldung.
[[nodiscard]] QString checkLookIconSize();

class LookButton : public QPushButton {
    Q_OBJECT
public:
    explicit LookButton(const QString& text, QWidget* parent = nullptr);

    // Seitenverhältnis des Vorschaubilds merken (leer = kein Bild) und die Icon-Größe sofort anpassen.
    void setImageSize(QSize image);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void fitIcon();
    QSize image_;
};

}  // namespace whitehole::studio
