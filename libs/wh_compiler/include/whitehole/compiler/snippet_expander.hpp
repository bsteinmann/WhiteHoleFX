// snippet_expander.hpp: ersetzt die Platzhalter eines Snippet-Rumpfs durch WGSL-Ausdrücke.
#pragma once

#include <functional>
#include <string>
#include <string_view>

#include "whitehole/core/result.hpp"

namespace whitehole::compiler {

/// Auflösung der Platzhalter. Der Expander kennt weder Graph noch Nodes; wer
/// ihn aufruft, entscheidet, wohin `@in` und `@param` zeigen.
struct SnippetBindings {
    /// @in(port, koordinate): `coordinate` ist bereits expandiert.
    std::function<Result<std::string>(std::string_view port, std::string_view coordinate)> input;
    /// @param(schluessel)
    std::function<Result<std::string>(std::string_view key)> parameter;
    /// @spectrum(port, index): ein Band des Spektrums am Eingang `port`; `index` ist bereits
    /// expandiert und ein u32-Ausdruck (0..31). Optional: ohne Funktion ist der Platzhalter ein Fehler.
    std::function<Result<std::string>(std::string_view port, std::string_view index)> spectrum;
    /// @time
    std::string time;
};

/// Platzhalter: @in(port, koordinate), @param(schluessel), @spectrum(port, band), @time.
/// - Klammern in `koordinate` dürfen verschachtelt sein, auch weitere Platzhalter.
/// - Alles außerhalb von Platzhaltern bleibt unverändert.
/// Fehler (InvalidArgument): unbekannter Platzhalter, fehlende oder unpassende
/// Klammer, fehlendes Komma in @in, leerer Name; dazu jeder Fehler der Bindings.
[[nodiscard]] Result<std::string> expandSnippet(std::string_view body,
                                                const SnippetBindings& bindings);

}  // namespace whitehole::compiler
