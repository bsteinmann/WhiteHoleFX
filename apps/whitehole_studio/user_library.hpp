// user_library.hpp: die Benutzerbibliothek der Bauformen auf der Festplatte.
//
// Warum es diese Datei gibt: Bauformen, die man im Reiter Fixtures anlegt, sollen nicht verschwinden, wenn man ein neues
// Projekt beginnt oder eine neue Programmversion installiert. Neben den Bauformen im Projekt (dort stehen sie, damit die
// Projektdatei für sich allein vollständig bleibt) gibt es deshalb eine Datei im Benutzerverzeichnis
// (Linux ~/.local/share/WhiteHoleFX, Windows %APPDATA%/WhiteHoleFX, Datei fixture-library.whfx). Ein neues Projekt
// bekommt ihren Inhalt; für ein geöffnetes Projekt holt der Knopf „Aus Benutzerbibliothek ergänzen“ fehlende Bauformen.
//
// Die Datei wird atomar geschrieben (QSaveFile). Lässt sie sich nicht lesen (defekt oder von einer neueren Version mit
// Syntax, die dieses Programm nicht kennt), bleibt sie unangetastet und der Fehler wird gemeldet: lieber nichts schreiben
// als eine fremde Bibliothek überschreiben.
//
// Der Pfad lässt sich mit der Umgebungsvariable WHITEHOLEFX_FIXTURE_LIBRARY ersetzen (Tests).
#pragma once

#include <QString>

#include <memory>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/graph/node_registry.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::studio::userlib {

/// Pfad der Bibliotheksdatei.
[[nodiscard]] QString path();

/// Liest die Bibliothek. Fehlt die Datei, ist die Liste leer (kein Fehler).
[[nodiscard]] Result<std::vector<project::FixtureType>> load(std::shared_ptr<const graph::INodeRegistry> registry);

/// Legt die Bauform an oder ersetzt die gleichnamige.
[[nodiscard]] Result<void> upsert(std::shared_ptr<const graph::INodeRegistry> registry, const project::FixtureType& type);
/// Entfernt die Bauform (nicht vorhanden: nichts zu tun).
[[nodiscard]] Result<void> remove(std::shared_ptr<const graph::INodeRegistry> registry, const std::string& name);
/// Benennt um (nicht vorhanden: nichts zu tun; Zielname schon belegt: der alte Eintrag wird ersetzt).
[[nodiscard]] Result<void> rename(std::shared_ptr<const graph::INodeRegistry> registry, const std::string& from,
                                  const std::string& to);

/// Setzt die Bauformen der Datei hinter die Kopfzeile von `projectText`, damit ein neues Projekt sie mitbekommt.
/// Gibt `projectText` unverändert zurück, wenn es keine Datei gibt oder sie leer ist. Die Prüfung, ob das Ergebnis
/// gültig ist, macht der Aufrufer beim Laden (und fällt sonst auf den Originaltext zurück).
[[nodiscard]] QString withLibraryTypes(const QString& projectText);

}  // namespace whitehole::studio::userlib
