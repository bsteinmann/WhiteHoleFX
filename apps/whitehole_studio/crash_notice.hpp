// crash_notice.hpp: Hinweis beim Start, wenn das Studio zuletzt abgestürzt ist.
//
// Warum es das gibt: Der Absturzbericht (stall_watchdog_win.cpp) liegt in einem Ordner, den niemand von selbst
// findet. Der Wächter legt beim Absturz die Marke <Cache>/crash/pending.txt an; beim nächsten Start zeigt das
// Studio einen Hinweis mit dem Ordner und löscht die Marke. Der Hinweis blockiert nicht (kein modaler Dialog),
// damit unbeaufsichtigte Läufe nicht hängen bleiben.
#pragma once

#include <QString>

class QWidget;

namespace whitehole::studio {

/// Liest und löscht die Marke. Gibt den Pfad des Berichts zurück, leer wenn es keine gibt.
[[nodiscard]] QString takePendingCrashReport(const QString& cacheDir);

/// Zeigt den Hinweis (nicht blockierend). Leerer Pfad: tut nichts.
void showCrashNotice(QWidget* parent, const QString& reportPath);

/// Hinweis für den Fall ohne Bericht: Das Studio wurde zuletzt nicht ordentlich beendet (siehe session_marker.hpp), aber
/// der Absturz-Handler hat nichts geschrieben. `cacheDir` ist der Ordner mit stack.txt und crash/.
void showUncleanNotice(QWidget* parent, const QString& cacheDir);

}  // namespace whitehole::studio
