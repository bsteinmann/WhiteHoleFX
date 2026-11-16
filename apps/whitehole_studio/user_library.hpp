// user_library.hpp: die Benutzerbibliothek der Controller auf der Festplatte.
//
// Warum es diese Datei gibt: Ein Controller-Aufbau (Name, Protokoll, Adresse, Port), den man in mehreren Projekten
// braucht, soll nicht verschwinden, wenn man ein neues Projekt beginnt oder eine neue Programmversion installiert.
// Neben den Controllern im Projekt gibt es deshalb eine Datei im Benutzerverzeichnis (Linux
// ~/.local/share/WhiteHoleFX, Windows %APPDATA%/WhiteHoleFX, Datei controller-library.whfx).
//
// Fixtures haben seit dem Umstieg auf Export/Import (Reiter Fixtures) keine solche automatische Bibliothek mehr -
// jedes Fixture liegt nur im Projekt, Export/Import läuft über einen frei wählbaren Dateipfad (siehe
// fixture_library_page.cpp, project::writeFixtureExport/parseFixtureExport).
//
// Die Datei wird atomar geschrieben (QSaveFile). Lässt sie sich nicht lesen (defekt oder von einer neueren Version mit
// Syntax, die dieses Programm nicht kennt), bleibt sie unangetastet und der Fehler wird gemeldet: lieber nichts schreiben
// als eine fremde Bibliothek überschreiben.
//
// Der Pfad lässt sich mit der Umgebungsvariable WHITEHOLEFX_CONTROLLER_LIBRARY ersetzen (Tests).
#pragma once

#include <QString>

#include <memory>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/graph/node_registry.hpp"
#include "whitehole/output/output_pipeline.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::studio::userlib {

[[nodiscard]] QString controllerPath();
[[nodiscard]] Result<std::vector<output::Destination>> loadControllers(std::shared_ptr<const graph::INodeRegistry> registry);
/// Legt den Controller unter seinem Namen an oder ersetzt den gleichnamigen. Fehler: Name leer.
[[nodiscard]] Result<void> upsertController(std::shared_ptr<const graph::INodeRegistry> registry,
                                            const output::Destination& controller);
[[nodiscard]] Result<void> removeController(std::shared_ptr<const graph::INodeRegistry> registry, const std::string& name);

}  // namespace whitehole::studio::userlib
