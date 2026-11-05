// fixture_template.hpp: Fixture-Vorlagen für die persönliche Fixture-Bibliothek.
//
// Warum es das gibt: Wer oft dieselben Teile baut (eine 5×5-Matrix, ein Band mit 78 Pixeln, eine Matrix mit
// mehr Abstand), will sie nicht jedes Mal neu anlegen. Eine Vorlage hält die Form samt Verkabelung und die
// Farbeinstellungen des Patches (Protokoll, Farbreihenfolge, Gamma, Helligkeit). Universe und erster Kanal
// gehören nicht dazu: Sie hängen vom Projekt ab und werden beim Hinzufügen automatisch vergeben.
//
// Dateiformat: eine normale Projektdatei mit genau einem Fixture. So bleibt es lesbar, tauschbar und geht
// durch denselben Leser und Schreiber wie jedes Projekt.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/mapping/fixture.hpp"
#include "whitehole/output/output_plan.hpp"

namespace whitehole::project {

struct FixtureTemplate {
    std::string name;
    mapping::FixtureShape shape{mapping::MatrixShape{}};
    /// Nur Protokoll, Ziel, Farbreihenfolge, Gamma, Helligkeit zählen; `fixture`, Universe, Kanal werden ignoriert.
    std::optional<output::FixturePatch> patch;
};

/// Fehler: InvalidArgument (Form ungültig oder Name leer).
[[nodiscard]] Result<std::string> writeFixtureTemplate(const FixtureTemplate& tpl);

/// Fehler: InvalidArgument, wenn der Text kein Projekt mit genau einem Fixture ist.
[[nodiscard]] Result<FixtureTemplate> parseFixtureTemplate(std::string_view text);

/// Die persönliche Fixture-Bibliothek: ein Ordner mit je einer Datei pro Vorlage (Endung .whfx).
class FixtureLibrary {
public:
    explicit FixtureLibrary(std::filesystem::path folder) : folder_{std::move(folder)} {}

    /// Alle lesbaren Vorlagen, nach Namen sortiert. Kaputte Dateien werden übersprungen.
    [[nodiscard]] std::vector<FixtureTemplate> list() const;
    /// Legt die Vorlage an oder ersetzt eine mit gleichem Namen. Fehler: IoError, InvalidArgument.
    [[nodiscard]] Result<void> save(const FixtureTemplate& tpl) const;
    /// Löscht die Vorlage mit diesem Namen. Fehler: NotFound.
    [[nodiscard]] Result<void> remove(const std::string& name) const;

private:
    std::filesystem::path folder_;
};

}  // namespace whitehole::project
