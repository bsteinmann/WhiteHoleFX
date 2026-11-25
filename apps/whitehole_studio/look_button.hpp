// look_button.hpp: Look-Knopf im Live-Fenster, dessen Vorschaubild die Knopffläche füllt.
//
// Warum es diese Datei gibt: Ein QPushButton zeichnet sein Icon in fester Größe neben dem Text, auch wenn der Knopf durch
// Fenstergröße oder Raster viel größer wird - das Vorschaubild blieb winzig (Rückmeldung vom 25.11., zweimal). LookButton
// zeichnet Bild und Name selbst: das Bild füllt die Fläche über dem Namen (Seitenverhältnis bleibt), der Name steht
// darunter in einer Zeile. Ohne Bild verhält sich der Knopf wie jeder QPushButton.
#pragma once

#include <QPushButton>
#include <QSize>
#include <QString>

namespace whitehole::studio {

// Größe des Vorschaubilds im Knopf: die Knopffläche abzüglich Rand und `textHeight` für den Namen darunter, das
// Seitenverhältnis des Bildes bleibt. Reine Funktion, damit sie sich testen lässt; leer, wenn das Bild keine Größe hat.
[[nodiscard]] QSize lookIconSize(QSize button, QSize image, int textHeight);

// Selbstprüfung für den Studio-Rauchtest (Qt läuft nur in der CI): leer = in Ordnung, sonst die Fehlermeldung.
[[nodiscard]] QString checkLookIconSize();

class LookButton : public QPushButton {
    Q_OBJECT
public:
    explicit LookButton(const QString& text, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
};

}  // namespace whitehole::studio
