// stall_watchdog.hpp: Wächter gegen "das Studio friert ein" und gegen Abstürze (Issue #26).
//
// Warum es das gibt: Ein Freeze oder Absturz lässt sich oft nicht unter gdb nachstellen, weil schon das
// Debuggen den Ablauf ändert. Der Wächter läuft deshalb immer mit (Linux und Windows): Ein Nebenthread
// prüft, ob der Hauptthread regelmäßig `stallHeartbeat()` aufruft. Bleibt der Herzschlag länger als zwei
// Sekunden aus, schickt er dem Hauptthread ein Signal, dessen Handler den Aufrufstapel ausgibt - so sieht man,
// wo das Programm steht. Dieselbe Ausgabe gibt es bei SIGSEGV, SIGABRT, SIGBUS, SIGFPE und SIGILL.
// Ausgegeben wird auf stderr und zusätzlich in eine Datei (Pfad beim Start übergeben), damit es auch ohne
// Terminal etwas zu lesen gibt. Abschalten: Umgebungsvariable WHITEHOLEFX_NO_WATCHDOG=1.
// Unter Windows setzt stall_watchdog_win.cpp dieselbe Schnittstelle um (Textbericht und Minidump in <Cache>/crash/).
#pragma once

#include <atomic>

namespace whitehole::studio {

/// Installiert die Signalbehandlung und startet den Wächter-Thread. Aufruf vom Hauptthread, einmal.
/// `logPath` darf nullptr sein (dann nur stderr). Liefert false, wenn der Wächter abgeschaltet ist.
bool startStallWatchdog(const char* logPath);

/// "Der Hauptthread lebt": regelmäßig vom Hauptthread aufrufen (Studio: alle 100 ms per Timer).
void stallHeartbeat();

/// Zähler der laufenden Pausen (siehe StallPause); der Wächter meldet keinen Stillstand, solange er über 0 steht.
inline std::atomic<int>& stallPauseCount() {
    static std::atomic<int> count{0};
    return count;
}

/// Hält den Wächter an, solange der Hauptthread bewusst wartet. Warum: Die Datei-Dialoge von Windows (Speichern
/// unter, Öffnen) laufen mit eigener Nachrichtenschleife; der Herzschlag-Timer von Qt tickt währenddessen nicht, und
/// wer den Dialog länger als fünf Sekunden offen lässt, bekam einen falschen "Stillstand"-Bericht. Auf den Stack legen,
/// bevor der Dialog aufgeht: `const StallPause pause;`.
class StallPause {
public:
    StallPause() { ++stallPauseCount(); }
    ~StallPause() {
        stallHeartbeat();  // die Zeit im Dialog zählt nicht als Schweigen
        --stallPauseCount();
    }
    StallPause(const StallPause&) = delete;
    StallPause& operator=(const StallPause&) = delete;
};

/// Zählt ein Ereignis für die Auswertung bei einem Stillstand: `name` ist ein Klassenname mit statischer
/// Lebensdauer (QMetaObject::className), `type` der Ereignistyp. Steht der Hauptthread, zeigt der Wächter,
/// welche (Klasse, Typ)-Paare in der letzten Sekunde am häufigsten vorkamen - eine Ereignisschleife (etwa
/// Scrollleisten, die sich gegenseitig ein- und ausblenden) fällt so sofort auf. Thread-sicher, ohne Sperre.
void stallCountEvent(const char* name, int type);

/// Teilt dem Wächter Version und GPU-Adapter für den Absturzbericht mit (nur Windows wertet es aus). Darf mehrfach
/// aufgerufen werden, nullptr lässt den bisherigen Wert.
void stallSetInfo(const char* version, const char* gpuAdapter);

/// Nur für den Selbsttest (--crash-test): stürzt absichtlich ab (Zugriff auf Adresse 0).
[[noreturn]] void stallCrashTest();

/// Nur für den Selbsttest (--hang-test): blockiert den aufrufenden Thread für `milliseconds`.
void stallForTest(int milliseconds);

}  // namespace whitehole::studio
