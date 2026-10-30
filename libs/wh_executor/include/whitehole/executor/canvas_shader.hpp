// canvas_shader.hpp: der Compute-Einstieg, der ein Feld in eine Textur rastert.
//
// Warum es diese Datei gibt: Der Feldcompiler liefert nur eine Bibliothek mit
// `wh_field(p)`. Wer sie benutzen will, hängt einen Einstieg an. Der einfachste
// Konsument ist der Canvas: Jedes Pixel einer 2D-Textur fragt das Feld an seiner
// Koordinate ab. (Andere Konsumenten folgen mit dem Mapping in Schritt 5: ein
// Einstieg, der das Feld an den LED-Positionen abtastet.) Der Text ist von der
// Stage getrennt, damit er ohne GPU testbar und prüfbar bleibt.
//
// Koordinaten: Pixel (x, y) tastet p = ((x + 0.5) / breite, 1 - (y + 0.5) / höhe, 0) ab.
// Die oberste Zeile (y = 0) liegt bei p.y nahe 1: y zeigt nach oben, wie im Raum und im
// Layout von wh_mapping.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "whitehole/core/result.hpp"
#include "whitehole/gpu/gpu_types.hpp"

namespace whitehole::executor {

/// Die Zieltextur liegt in Bind-Group 1 (Gruppe 0 gehört der Feldbibliothek).
inline constexpr std::uint32_t kCanvasBindGroup = 1;
inline constexpr std::uint32_t kCanvasTextureBinding = 0;

/// Kantenlänge der Arbeitsgruppe; die Stage teilt die Textur danach auf.
inline constexpr std::uint32_t kCanvasWorkgroupSize = 8;

/// Hängt den Einstieg `main` an die Feldbibliothek.
/// Fehler: InvalidArgument bei einem Format, das sich nicht als Storage-Textur
/// beschreiben lässt (R32Float).
[[nodiscard]] Result<std::string> canvasShader(std::string_view fieldLibrary,
                                               gpu::TextureFormat format);

}  // namespace whitehole::executor
