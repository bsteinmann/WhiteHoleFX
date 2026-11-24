// sampling_shader.hpp: der Compute-Einstieg, der ein Feld an LED-Positionen abtastet.
//
// Warum es diese Datei gibt: Das ist das Gegenstück zum Canvas-Einstieg für LEDs im
// Raum. Jeder Thread nimmt eine LED, liest ihre Position im Einheitswürfel und ruft
// `wh_field(p)` auf. Damit gilt derselbe Effekt für eine Matrix, einen Strip, einen
// Cube und eine Punktwolke, ohne ein Volumen zu rastern. Der Text ist von der Stage
// getrennt und ohne GPU testbar.
//
// Bindings (Gruppe 0 gehört der Feldbibliothek):
//   @group(1) @binding(0)  Positionen  array<vec4<f32>>, xyz im Einheitswürfel, w = 0
//   @group(1) @binding(1)  Farben      array<vec4<f32>>, linear RGBA, eine pro LED
//   @group(1) @binding(2)  WhLeds      Anzahl der gültigen LEDs, gewählter Zusatzausgang, dessen Lage und Kennung
//
// Zusatzausgänge (geteilte Vorschau im Live-Betrieb): Hat die Feldbibliothek Einstiege `wh_field_extra<i>`, tastet der
// Shader zusätzlich genau einen davon ab (WhLeds.extra, ungültig = keiner; ein einheitlicher Zweig, die anderen kosten
// nichts). Ergebnis: an Stelle `base` ein Kopf vec4(kennung, 0, 0, 0), ab `base + 1` eine Farbe je LED. Die Kennung
// zeigt dem Leser, zu welcher Auswahl das Bild gehört (ein Readback kommt ein paar Frames später an).
//
// Die Anzahl steht im Uniform, nicht in arrayLength(): Die Puffer sind für eine
// Höchstzahl ausgelegt, das Layout darf kleiner sein.
//
// Dispatch: WebGPU erlaubt höchstens 65535 Gruppen je Dimension. Bei mehr als
// 65535 * 64 = 4,2 Mio. LEDs reicht eine Dimension nicht; der Dispatch ist daher 2D
// und der Shader setzt den Index aus beiden Dimensionen zusammen.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "whitehole/core/result.hpp"
#include "whitehole/executor/field_pass.hpp"

namespace whitehole::executor {

inline constexpr std::uint32_t kSamplingBindGroup = 1;
inline constexpr std::uint32_t kSamplingPositionsBinding = 0;
inline constexpr std::uint32_t kSamplingColorsBinding = 1;
inline constexpr std::uint32_t kSamplingInfoBinding = 2;

inline constexpr std::uint32_t kSamplingWorkgroupSize = 64;
inline constexpr std::uint32_t kMaxGroupsPerDimension = 65535;

/// Kein Zusatzausgang gewählt (WhLeds.extra).
inline constexpr std::uint32_t kNoSamplingExtra = 0xFFFFFFFFU;

/// Hängt den Einstieg `main` an die Feldbibliothek; `extraCount` Zusatzausgänge (`wh_field_extra0` ...) stehen in ihr.
[[nodiscard]] std::string samplingShader(std::string_view fieldLibrary, std::size_t extraCount = 0);

/// Wie viele Zusatzausgänge (`fn wh_field_extra<i>(`, lückenlos ab 0) die Feldbibliothek hat.
[[nodiscard]] std::size_t countFieldExtras(std::string_view fieldLibrary);

/// Dispatch-Größe für `ledCount` LEDs (x * y * 64 >= ledCount, x <= 65535).
[[nodiscard]] DispatchSize samplingDispatch(std::uint64_t ledCount) noexcept;

}  // namespace whitehole::executor
