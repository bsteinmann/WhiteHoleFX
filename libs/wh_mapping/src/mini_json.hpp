// mini_json.hpp (intern): ein kleiner JSON-Leser für den Punkte-Import.
//
// Warum es diese Datei gibt: Der Import von Layouts (point_import.hpp) liest JSON-Dateien aus anderen
// Programmen. Eine große JSON-Bibliothek nur dafür wäre überdimensioniert; gebraucht werden Zahlen,
// Texte, Listen und Objekte. Kein Gleitkomma-Sonderfall (NaN/Infinity gibt es in JSON nicht), keine
// Kommentare. Verschachtelung ist auf 32 Ebenen begrenzt, damit fehlerhafte Dateien nicht den Stapel sprengen.
// Abhängigkeiten: nur wh_core (Result).
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "whitehole/core/result.hpp"

namespace whitehole::mapping::detail {

struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type{Type::Null};
    double number{0.0};
    bool boolean{false};
    std::string text;
    std::vector<Json> items;        // Array
    std::vector<std::string> keys;  // Object: Feldnamen in Dateireihenfolge
    std::vector<Json> values;       // Object: Werte, values[i] gehört zu keys[i]
    // (kein vector<pair<string, Json>>: std::pair verlangt einen vollständigen Typ, Json ist hier noch unvollständig)

    /// Das Feld eines Objekts oder nullptr.
    [[nodiscard]] const Json* find(std::string_view key) const;
};

/// Fehler: InvalidArgument mit Zeile und Spalte.
[[nodiscard]] Result<Json> parseJson(std::string_view text);

}  // namespace whitehole::mapping::detail
