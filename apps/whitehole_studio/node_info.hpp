// node_info.hpp: Beschreibungstexte der Node-Typen (Info-Zeichen "i" im Node-Kopf, Tooltip der Palette).
//
// Warum es das gibt: Wer einen Node nicht kennt, soll nicht in der Dokumentation nachlesen müssen. Der Text steht
// deutsch im Quelltext (node_info_texts.cpp) und wird wie die übrigen Oberflächentexte übersetzt.
#pragma once

#include <QString>

#include <span>
#include <string>
#include <vector>

#include "whitehole/graph/node_registry.hpp"

namespace whitehole::studio {

struct NodeInfoEntry {
    const char* typeId;
    const char* text;  // deutscher Quelltext, mit QT_TR_NOOP markiert
};

/// Alle Einträge (in node_info_texts.cpp).
[[nodiscard]] std::span<const NodeInfoEntry> nodeInfoEntries();

/// Die übersetzte Beschreibung eines Node-Typs, bei `columns` Zeichen umbrochen (für Tooltips ohne Rich Text).
/// Leer, wenn der Typ keinen Eintrag hat.
[[nodiscard]] QString nodeInfoText(const std::string& typeId, int columns = 64);

/// Umbruch an Leerzeichen; vorhandene Zeilenumbrüche bleiben.
[[nodiscard]] QString wrapWords(const QString& text, int columns);

/// Für den Rauchtest: Typ-IDs der Registrierung ohne Beschreibung, und Einträge ohne Typ (Tippfehler).
[[nodiscard]] std::vector<std::string> nodeInfoProblems(const graph::INodeRegistry& registry);

}  // namespace whitehole::studio
