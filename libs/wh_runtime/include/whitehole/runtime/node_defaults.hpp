// node_defaults.hpp: eigene Standardwerte für neue Nodes.
//
// Warum es das gibt: Wer immer wieder dieselben Nodes mit denselben Werten braucht (Value Noise mit Dichte 0,3,
// ein Output mit Helligkeit 0,8, ein Text in der Hausschrift), will sie nicht jedes Mal umstellen. Die
// Oberfläche legt pro Node-Typ und Parameter einen eigenen Standardwert fest; ein neu angelegter Node (Palette,
// Menü, Doppelklick) startet damit statt mit dem Wert aus der Bibliothek. Bestehende Nodes und geladene Projekte
// bleiben unberührt, und die Werte gehören zum Nutzer, nicht zum Projekt.
//
// Die Kodierung als Text ("d:0.25", "c:1,0.5,0,1") dient dem Speichern beim Nutzer (QSettings im Studio).
// Kein Qt, kein Betriebssystem.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "whitehole/graph/node_descriptor.hpp"

namespace whitehole::runtime {

class NodeDefaults {
public:
    using Values = std::map<std::string, graph::ParameterValue>;

    /// Legt den Standardwert fest (ersetzt einen vorhandenen). Typ und Bereich prüft erst das Anlegen eines
    /// Nodes (ein unpassender Wert wird dort übersprungen).
    void set(const std::string& typeId, const std::string& key, graph::ParameterValue value);
    /// Entfernt einen Standardwert; wahr, wenn es ihn gab.
    bool clear(const std::string& typeId, const std::string& key);
    /// Entfernt alle Standardwerte eines Typs bzw. alle.
    bool clearType(const std::string& typeId);
    void clearAll() noexcept { values_.clear(); }

    [[nodiscard]] const graph::ParameterValue* find(const std::string& typeId, const std::string& key) const;
    /// Alle Werte eines Typs (leer, wenn es keine gibt).
    [[nodiscard]] const Values* valuesOf(const std::string& typeId) const;
    [[nodiscard]] const std::map<std::string, Values>& all() const noexcept { return values_; }
    [[nodiscard]] bool empty() const noexcept { return values_.empty(); }

    bool operator==(const NodeDefaults&) const = default;

private:
    std::map<std::string, Values> values_;
};

/// Wert als Text: "b:1", "i:5", "d:0.25", "v2:x,y", "v3:x,y,z", "c:r,g,b,a" oder "s:Text".
[[nodiscard]] std::string encodeParameterValue(const graph::ParameterValue& value);
/// Umkehrung von encodeParameterValue; leer bei unbekannter Form oder nicht endlichen Zahlen.
[[nodiscard]] std::optional<graph::ParameterValue> decodeParameterValue(std::string_view text);

}  // namespace whitehole::runtime
