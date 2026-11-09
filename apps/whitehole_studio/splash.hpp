// splash.hpp: der Startbildschirm des Studios (Logo, Name, Version, Meldung zum Fortschritt).
//
// Warum es das gibt: Der Start dauert einen Moment (GPU-Adapter suchen, Fenster aufbauen, Vorlagen laden). Ohne
// Rückmeldung sieht es aus, als passiere nichts. Das Bild wird in Code gezeichnet (QPainter), damit es zur
// Sprache und zur Version passt und keine zweite Grafikdatei gepflegt werden muss; das Logo kommt aus den
// eingebetteten Symbolen (studio.qrc).
#pragma once

#include <QPixmap>
#include <QSplashScreen>
#include <QString>

namespace whitehole::studio {

/// Der Startbildschirm: das Bild plus der aktuelle Schritt ("GPU wird vorbereitet …") unten links.
class StartSplash : public QSplashScreen {
public:
    explicit StartSplash(const QString& version);
    /// Zeigt den Schritt an und lässt Qt zeichnen (der Start läuft sonst ohne Ereignisschleife).
    void step(const QString& text);

protected:
    void drawContents(QPainter* painter) override;

private:
    QString step_;
};

/// Das Bild des Startbildschirms (Name, Version `version`, Logo). Nie leer.
[[nodiscard]] QPixmap makeSplashPixmap(const QString& version);

/// Rauchtest ohne Bildschirm: Das Bild hat die erwartete Größe, ist nicht leer und enthält das Logo und den Text.
/// Leer = in Ordnung, sonst die Fehlerbeschreibung.
[[nodiscard]] QString splashProblem();

}  // namespace whitehole::studio
