// scroll_behavior.hpp: Mausrad und Scrollbalken im ganzen Programm.
//
// Warum es das gibt: Wer in einer langen Liste von Reglern mit dem Mausrad scrollt, ändert ungewollt Werte, sobald
// der Zeiger über einem Zahlenfeld, einer Auswahl oder einem Schieber steht. Außerdem sind die Standard-Scrollbalken
// auf manchen Systemen kaum zu sehen. Beides wird einmal für die ganze Anwendung geregelt:
//   - Das Mausrad ändert nie den Wert eines Zahlenfelds, einer Auswahl oder eines Schiebers; es scrollt die Fläche
//     dahinter (Werte ändern sich durch Klick, Ziehen, Tippen und Pfeiltasten).
//   - Scrollbalken sind breiter und heben sich vom Hintergrund ab.
#pragma once

#include <QObject>
#include <QString>

class QApplication;

namespace whitehole::studio {

/// Installiert Filter und Stilvorgabe für die ganze Anwendung. Einmal beim Start aufrufen.
void installScrollBehavior(QApplication& app);

/// Rauchtest: Ein Mausrad-Ereignis über Zahlenfeld, Auswahl und Schieber ändert nichts und erreicht den Elternteil.
/// Leer, wenn alles stimmt, sonst die Beschreibung des Fehlers.
QString scrollBehaviorProblem();

}  // namespace whitehole::studio
