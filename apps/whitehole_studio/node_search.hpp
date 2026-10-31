// node_search.hpp: Suche nach Node-Typen für das Suchfeld im Node-Editor (Strg+Leertaste).
//
// Getrennt vom Fenster, damit die Trefferregel ohne Bildschirm geprüft werden kann (--exercise).
#pragma once

#include <QString>

#include <vector>

#include "whitehole/graph/node_descriptor.hpp"

namespace whitehole::studio {

/// Alle Typen, bei denen jedes Wort der Eingabe (ohne Beachtung der Groß-/Kleinschreibung) im
/// Anzeigenamen, in der Kategorie oder in der Typ-ID steckt. Leere Eingabe: alle. Sortiert nach
/// Kategorie und Namen.
[[nodiscard]] std::vector<const graph::NodeDescriptor*> filterNodeTypes(
    std::vector<const graph::NodeDescriptor*> all, const QString& text);

}  // namespace whitehole::studio
