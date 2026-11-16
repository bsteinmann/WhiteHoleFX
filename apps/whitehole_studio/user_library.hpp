// user_library.hpp: die Benutzerbibliothek der Fixtures auf der Festplatte.
//
// Warum es diese Datei gibt: Fixtures, die man im Reiter Fixtures anlegt, sollen nicht verschwinden, wenn man ein neues
// Projekt beginnt oder eine neue Programmversion installiert. Neben den Fixtures im Projekt (dort stehen sie, damit die
// Projektdatei für sich allein vollständig bleibt) gibt es deshalb eine Datei im Benutzerverzeichnis
// (Linux ~/.local/share/WhiteHoleFX, Windows %APPDATA%/WhiteHoleFX, Datei fixture-library.whfx). Im Reiter Fixtures
// erscheinen Projekt- und Bibliothek-Fixtures als eine gemeinsame Liste (siehe fixture_catalog.hpp); „In Bibliothek
// speichern“ legt ein Fixture der Show dort ab, jede Bearbeitung eines nur dort gespeicherten Fixtures holt es
// automatisch in die Show zurück.
//
// Die Datei wird atomar geschrieben (QSaveFile). Lässt sie sich nicht lesen (defekt oder von einer neueren Version mit
// Syntax, die dieses Programm nicht kennt), bleibt sie unangetastet und der Fehler wird gemeldet: lieber nichts schreiben
// als eine fremde Bibliothek überschreiben.
//
// Der Pfad lässt sich mit der Umgebungsvariable WHITEHOLEFX_FIXTURE_LIBRARY ersetzen (Tests).
// Dieselbe Datei-Logik gilt für die Controller-Bibliothek weiter unten.
#pragma once

#include <QString>

#include <memory>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/graph/node_registry.hpp"
#include "whitehole/output/output_pipeline.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::studio::userlib {

/// Pfad der Bibliotheksdatei.
[[nodiscard]] QString path();

/// Liest die Bibliothek. Fehlt die Datei, ist die Liste leer (kein Fehler).
[[nodiscard]] Result<std::vector<project::FixtureType>> load(std::shared_ptr<const graph::INodeRegistry> registry);

/// Legt das Fixture an oder ersetzt das gleichnamige.
[[nodiscard]] Result<void> upsert(std::shared_ptr<const graph::INodeRegistry> registry, const project::FixtureType& type);
/// Entfernt das Fixture (nicht vorhanden: nichts zu tun).
[[nodiscard]] Result<void> remove(std::shared_ptr<const graph::INodeRegistry> registry, const std::string& name);
/// Benennt um (nicht vorhanden: nichts zu tun; Zielname schon belegt: der alte Eintrag wird ersetzt).
[[nodiscard]] Result<void> rename(std::shared_ptr<const graph::INodeRegistry> registry, const std::string& from,
                                  const std::string& to);

// --- Controller -----------------------------------------------------------------------------------------------
// Gleiche Idee für Controller (Name, Protokoll, Adresse, Port): Datei controller-library.whfx im selben Ordner, Pfad
// ersetzbar mit WHITEHOLEFX_CONTROLLER_LIBRARY. Jeder Eintrag hat einen eindeutigen Namen.

[[nodiscard]] QString controllerPath();
[[nodiscard]] Result<std::vector<output::Destination>> loadControllers(std::shared_ptr<const graph::INodeRegistry> registry);
/// Legt den Controller unter seinem Namen an oder ersetzt den gleichnamigen. Fehler: Name leer.
[[nodiscard]] Result<void> upsertController(std::shared_ptr<const graph::INodeRegistry> registry,
                                            const output::Destination& controller);
[[nodiscard]] Result<void> removeController(std::shared_ptr<const graph::INodeRegistry> registry, const std::string& name);

}  // namespace whitehole::studio::userlib
