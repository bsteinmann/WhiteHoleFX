// node_preset.hpp: Presets für Nodes (ausgewählte Nodes samt Kabeln als Datei merken und wieder einfügen).
//
// Warum es das gibt: Eine Kombination, die sich bewährt hat (zum Beispiel "Plasma mit Outline und Farbwechsel"),
// soll man nicht jedes Mal neu bauen. Ein Preset ist dieselbe Auswahl wie die Zwischenablage, aber auf
// der Platte und über Sitzungen und Projekte hinweg. Das Dateiformat ist das Projektformat, nur mit
// `node`- und `connect`-Zeilen (kein Layout, kein `output`): Man kann ein Preset im Texteditor lesen
// und mit `whitehole_cli --check` als Projekt-Fragment prüfen, und es gibt kein zweites Format.
// Eingefügt wird ein Preset als ganz normale Nodes (Player::pasteNodes, ein Schritt für Rückgängig);
// danach ist es nicht mehr mit dem Preset verbunden.
//
// Reine Daten- und Dateiarbeit ohne Qt, daher testbar.
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "whitehole/core/result.hpp"
#include "whitehole/graph/node_registry.hpp"
#include "whitehole/runtime/node_clipboard.hpp"

namespace whitehole::runtime {

/// Dateiendung eines Presets (mit Punkt).
inline constexpr const char* kPresetExtension = ".whfxnodes";

/// Text eines Presets. Fehler: InvalidArgument (leer), sonst die der Graph-Operationen (unbekannter Typ).
[[nodiscard]] Result<std::string> presetToText(const NodeClipboard& clipboard,
                                               std::shared_ptr<const graph::INodeRegistry> registry);

/// Liest ein Preset. Fehler: die von parseProject (mit Zeilennummer), InvalidArgument, wenn es keinen
/// Node enthält.
[[nodiscard]] Result<NodeClipboard> presetFromText(std::string_view text,
                                                   std::shared_ptr<const graph::INodeRegistry> registry);

/// Ein Name taugt als Dateiname auf allen Plattformen: nicht leer, höchstens 64 Zeichen, keines von
/// `\ / : * ? " < > |` und keine Steuerzeichen, kein Punkt oder Leerzeichen am Rand.
[[nodiscard]] bool isValidPresetName(std::string_view name);

/// Ein Ordner mit Presets, eine Datei je Preset (`<Name>.whfxnodes`).
class PresetStore {
public:
    explicit PresetStore(std::filesystem::path directory) : directory_{std::move(directory)} {}

    [[nodiscard]] const std::filesystem::path& directory() const noexcept { return directory_; }

    /// Namen aller Presets, alphabetisch (ohne Rücksicht auf Groß-/Kleinschreibung). Ein Ordner, den es
    /// nicht gibt, ist eine leere Liste.
    [[nodiscard]] std::vector<std::string> list() const;
    /// Legt den Ordner bei Bedarf an und ersetzt ein gleichnamiges Preset. Fehler: InvalidArgument
    /// (Name), IoError.
    Result<void> save(const std::string& name, std::string_view text) const;
    /// Fehler: InvalidArgument (Name), NotFound, IoError.
    [[nodiscard]] Result<std::string> load(const std::string& name) const;
    /// Fehler: InvalidArgument (Name), NotFound, IoError.
    Result<void> remove(const std::string& name) const;
    [[nodiscard]] bool contains(const std::string& name) const;

private:
    [[nodiscard]] std::filesystem::path fileOf(const std::string& name) const;

    std::filesystem::path directory_;
};

}  // namespace whitehole::runtime
