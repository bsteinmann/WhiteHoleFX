// fft.hpp: schnelle Fourier-Transformation (Radix-2) für die Audio-Analyse.
//
// Warum es diese Datei gibt: Die Spektrum-Anzeige und die Beat-Erkennung brauchen die
// Frequenzanteile des Eingangssignals. Eine eigene kleine FFT vermeidet eine Abhängigkeit für
// eine Handvoll Zeilen; bei 2048 Punkten und ~90 Aufrufen pro Sekunde ist sie weit schnell genug.
//
// Die Tabellen (Drehfaktoren, Bitumkehr) entstehen einmal im Konstruktor, `forward` allokiert nicht.
#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace whitehole::audio {

class Fft {
public:
    /// `size` wird auf die nächste Zweierpotenz aufgerundet (mindestens 2).
    explicit Fft(std::size_t size);

    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    /// Transformiert `data` (genau `size()` Werte) an Ort und Stelle. Ohne Normierung:
    /// ein Sinus der Amplitude A, der genau auf einen Bin fällt, ergibt dort |X| = A·N/2.
    void forward(std::span<std::complex<float>> data) const;

private:
    std::size_t size_;
    std::vector<std::complex<float>> twiddle_;  // exp(-2πi·k/N), k < N/2
    std::vector<std::uint32_t> reverse_;        // Bitumkehr-Reihenfolge
};

}  // namespace whitehole::audio
