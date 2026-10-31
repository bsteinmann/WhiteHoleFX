// value_store.hpp: die Werte, die CPU-Nodes im aktuellen Frame an ihren Ausgängen liefern.
//
// Warum es diese Datei gibt: Zwischen Auswertung (Evaluator) und Verbrauch
// (Parameterpuffer, andere CPU-Nodes) braucht es einen neutralen Ablageort.
// Der ValueStore ist genau das und nicht mehr: eine Tabelle (Node, Ausgang) -> Wert.
// Er implementiert graph::IValueLookup, damit der Compiler ihn lesen kann, ohne
// diese Bibliothek zu kennen.
//
// Threading: nicht thread-sicher. Der Engine-Thread schreibt und liest ihn im
// selben Frame nacheinander; wer ihn teilen will, kopiert ihn (Wertetyp).
#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/spectrum.hpp"
#include "whitehole/graph/value_lookup.hpp"

namespace whitehole::values {

class ValueStore final : public graph::IValueLookup {
public:
    /// Setzt den Wert eines Ausgangs; ein vorhandener Wert wird ersetzt.
    void set(NodeId node, std::string_view portKey, graph::ParameterValue value);

    [[nodiscard]] std::optional<graph::ParameterValue> value(NodeId node,
                                                             std::string_view portKey) const override;

    /// Spektren liegen auf einem eigenen Kanal (sie sind keine ParameterValues).
    void setSpectrum(NodeId node, std::string_view portKey, const Spectrum& spectrum);
    [[nodiscard]] std::optional<Spectrum> spectrum(NodeId node, std::string_view portKey) const override;

    /// Entfernt alle Werte (Anfang jedes Frames).
    void clear() noexcept;

    /// Anzahl der gespeicherten Werte (Ausgänge, nicht Nodes).
    [[nodiscard]] std::size_t size() const noexcept { return count_; }

private:
    std::unordered_map<NodeId, std::map<std::string, graph::ParameterValue, std::less<>>> values_;
    std::unordered_map<NodeId, std::map<std::string, Spectrum, std::less<>>> spectra_;
    std::size_t count_{0};
};

}  // namespace whitehole::values
