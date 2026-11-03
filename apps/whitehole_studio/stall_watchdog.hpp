// stall_watchdog.hpp: Wächter gegen "das Studio friert ein" und gegen Abstürze (Issue #26).
//
// Warum es das gibt: Ein Freeze oder Absturz lässt sich oft nicht unter gdb nachstellen, weil schon das
// Debuggen den Ablauf ändert. Der Wächter läuft deshalb immer mit (nur Linux, sonst leer): Ein Nebenthread
// prüft, ob der Hauptthread regelmäßig `stallHeartbeat()` aufruft. Bleibt der Herzschlag länger als zwei
// Sekunden aus, schickt er dem Hauptthread ein Signal, dessen Handler den Aufrufstapel ausgibt - so sieht man,
// wo das Programm steht. Dieselbe Ausgabe gibt es bei SIGSEGV, SIGABRT, SIGBUS, SIGFPE und SIGILL.
// Ausgegeben wird auf stderr und zusätzlich in eine Datei (Pfad beim Start übergeben), damit es auch ohne
// Terminal etwas zu lesen gibt. Abschalten: Umgebungsvariable WHITEHOLEFX_NO_WATCHDOG=1.
#pragma once

namespace whitehole::studio {

/// Installiert die Signalbehandlung und startet den Wächter-Thread. Aufruf vom Hauptthread, einmal.
/// `logPath` darf nullptr sein (dann nur stderr). Liefert false, wenn der Wächter abgeschaltet ist.
bool startStallWatchdog(const char* logPath);

/// "Der Hauptthread lebt": regelmäßig vom Hauptthread aufrufen (Studio: alle 100 ms per Timer).
void stallHeartbeat();

/// Nur für den Selbsttest (--hang-test): blockiert den aufrufenden Thread für `milliseconds`.
void stallForTest(int milliseconds);

}  // namespace whitehole::studio
