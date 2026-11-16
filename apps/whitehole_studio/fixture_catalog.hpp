// fixture_catalog.hpp: die eine Liste aller Fixtures, die der Nutzer sehen soll - unabhängig davon, ob sie schon in
// dieser Show verwendet wird oder (noch) nur in der Bibliothek auf der Festplatte liegt (user_library.hpp).
//
// Warum es diese Datei gibt: der Reiter Fixtures zeigt eine einzige Liste statt zweier getrennter (vom Nutzer
// gewünscht, nach dem Vorbild einer Lichtpult-Fixture-Bibliothek: man denkt in "Fixtures", nicht getrennt in "schon
// in der Show" und "gespeichert"). Diese Liste braucht aber beide Seiten der Wahrheit (project::FixtureType dieser
// Show plus Benutzerbibliothek) und muss Einträge mit demselben Namen zusammenführen, nicht doppelt zeigen - daher
// ein kleiner, eigener Zusammenführungsschritt statt das in jeder Oberflächen-Datei einzeln zu wiederholen.
#pragma once

#include <QString>

#include <vector>

#include "whitehole/mapping/fixture.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

/// Ein Fixture, wie es in der zusammengeführten Liste erscheint: Name und Form, plus wo es herkommt. Beides kann
/// wahr sein (schon in der Show verwendet UND in der Bibliothek gespeichert) - das ist der Normalfall für ein
/// wiederverwendetes Fixture.
struct FixtureEntry {
    QString name;
    mapping::FixtureShape shape;
    bool inProject{false};
    bool inLibrary{false};
};

/// Alle Fixtures dieser Show und/oder der Bibliothek, nach Name eindeutig (ein Name erscheint höchstens einmal; ist
/// er in beiden, zählt die Form der Show als die gezeigte - sie könnte seit dem letzten Speichern abweichen).
/// Alphabetisch sortiert, damit die Liste nicht bei jedem Neuaufbau durcheinanderspringt.
[[nodiscard]] std::vector<FixtureEntry> mergedFixtures(const runtime::Player& player);

}  // namespace whitehole::studio
