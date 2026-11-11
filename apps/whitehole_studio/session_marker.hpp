// session_marker.hpp: Merkt sich, ob das Studio beim letzten Mal sauber beendet wurde.
//
// Warum es das gibt: Der Absturzbericht (stall_watchdog_win.cpp) entsteht nur, wenn Windows den Absturz an unseren Handler
// weiterreicht. Bestimmte Abstürze tun das nicht (Schnellabbruch der Laufzeit, beschädigter Heap, Fehler in Erweiterungen
// der Windows-Dialoge): Das Fenster verschwindet, es gibt weder Bericht noch Hinweis. Die Sitzungsmarke fängt genau das
// auf: Beim Start legt das Studio <Cache>/crash/session.txt mit der Prozessnummer an, beim ordentlichen Beenden löscht
// es sie. Findet der nächste Start die Datei noch vor und der Prozess lebt nicht mehr, war das Ende nicht sauber.
// Zwei gleichzeitig laufende Studios stören sich nicht: Eine Marke eines noch laufenden Prozesses zählt nicht, und beim
// Beenden löscht jeder nur seine eigene.
#pragma once

#include <QString>

namespace whitehole::studio {

/// Legt die Marke dieses Prozesses an. Gibt true zurück, wenn die vorige Sitzung nicht sauber endete.
[[nodiscard]] bool beginSession(const QString& cacheDir);

/// Löscht die Marke, falls sie noch von diesem Prozess stammt. Beim ordentlichen Beenden aufrufen.
void endSession(const QString& cacheDir);

/// Prüft die Logik in einem Wegwerf-Ordner (für `--session-test`). Gibt an stderr aus, was nicht stimmt.
[[nodiscard]] bool sessionSelfTest();

}  // namespace whitehole::studio
