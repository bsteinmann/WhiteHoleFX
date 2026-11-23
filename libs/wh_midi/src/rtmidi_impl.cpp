// rtmidi_impl.cpp: RtMidi in einer eigenen Übersetzungseinheit und ohne unsere Warnoptionen (Fremdcode).
// Warum es das gibt: Wie bei miniaudio soll Fremdcode keine Warnungen als Fehler auslösen und nur hier kompiliert werden.
// Welcher Betriebssystem-Zugang gebaut wird (ALSA, WinMM), setzt die CMakeLists.txt über Compile-Definitionen.
#include "RtMidi.cpp"
