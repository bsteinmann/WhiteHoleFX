// node_palette.hpp: Fenster mit allen verfügbaren Nodes (neben dem Node-Editor).
//
// Warum es das gibt: Das Rechtsklick-Menü und die Suche (Strg+Leertaste) reichen für wenige Nodes. Es
// werden mehr, und wer nicht weiß, wie ein Node heißt, will durchblättern. Die Palette listet alle
// Typen nach Kategorien, filtert beim Tippen und gibt einen Hinweis zu Ein- und Ausgängen.
//
// Bedienung:
//   hinzufügen   einen Eintrag auf die Fläche des Node-Editors ziehen (Drag & Drop), oder
//                doppelklicken (der Node erscheint in der Mitte der Ansicht)
//   entfernen    einen Node aus dem Editor auf die Palette ziehen und loslassen (der Editor meldet das
//                über NodeEditor::setDeleteZone; die Palette färbt sich dabei rot)
//
// Favoriten: Per Rechtsklick lässt sich ein Node-Typ als Favorit markieren. Favoriten erscheinen oben in der
// Gruppe "Favoriten" (und mit Stern in ihrer Kategorie) und werden in den Einstellungen gemerkt
// (QSettings "palette/favorites"). Die Kategorien sind beim Start zugeklappt, damit die lange Liste
// übersichtlich bleibt; wer sucht, sieht die Treffer aufgeklappt. Welche Kategorien man geöffnet hat,
// merkt sich die Palette, solange sie läuft.
//
// Farbe und Hervorhebung: Vor jedem Namen steht ein kleiner Punkt in der Farbe dessen, was der Node liefert
// (Bild blau, Wert grün, Farbe rosa, Audio himmelblau, sonst grau; siehe graph/port_family.hpp). Zieht man ein
// Kabel aus einem Port, bleiben nur die Nodes hell, die sich daran anschließen lassen, die übrigen werden
// abgedunkelt. Die Punkte lassen sich im Menü Ansicht abschalten ("palette/colored" in den Einstellungen).
//
// Die Palette kennt nur das Verzeichnis der Node-Typen, nicht den Graphen.
#pragma once

#include <QFont>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <memory>
#include <optional>

#include "whitehole/graph/node_registry.hpp"
#include "whitehole/graph/port_type.hpp"

class QLabel;
class QTreeWidgetItem;
class QLineEdit;
class QTreeWidget;

namespace whitehole::runtime {
class PresetStore;
}

namespace whitehole::studio {

/// MIME-Typ eines Bibliothekseintrags (Preset oder Gruppe): Inhalt "preset" oder "group", Zeilenumbruch, Name (UTF-8).
inline constexpr const char* kLibraryMime = "application/x-whitehole-library";
/// Rollen der Bibliothek im Baum: Art ("preset" / "group") an der Rubrik und an jedem Eintrag, Name nur am Eintrag.
inline constexpr int kLibraryKindRole = Qt::UserRole + 3;
inline constexpr int kLibraryNameRole = Qt::UserRole + 4;

/// Name des MIME-Typs, mit dem ein Node-Typ per Drag & Drop wandert (Inhalt: die Typ-ID als UTF-8).
inline constexpr const char* kNodeTypeMime = "application/x-whitehole-node-type";

class NodePalette : public QWidget {
    Q_OBJECT

public:
    explicit NodePalette(std::shared_ptr<const graph::INodeRegistry> registry, QWidget* parent = nullptr);

    /// Zeigt nur Typen, bei denen jedes Wort im Namen, in der Kategorie oder in der Typ-ID steckt.
    void setFilter(const QString& text);
    /// Zahl der Node-Typen, die gerade sichtbar sind (ohne Kategorien).
    [[nodiscard]] int visibleTypeCount() const;
    /// Favoriten: Typ-IDs, in den Einstellungen gespeichert.
    [[nodiscard]] QStringList favorites() const { return favorites_; }
    [[nodiscard]] bool isFavorite(const QString& typeId) const { return favorites_.contains(typeId); }
    void setFavorite(const QString& typeId, bool on);
    /// Zahl der aufgeklappten Kategorien (nach dem Start: nur "Favoriten", falls es welche gibt).
    [[nodiscard]] int expandedCategoryCount() const;
    /// Zeigt die Palette als "Papierkorb": rot mit dem Hinweis, dass Loslassen den Node entfernt.
    void setRemoveHighlight(bool on);

    /// Farbige Punkte vor den Namen an/aus (wird gemerkt, Standard: an).
    [[nodiscard]] bool colored() const noexcept { return colored_; }
    void setColored(bool on);
    /// Während ein Kabel gezogen wird: nur Nodes hervorheben, die sich anschließen lassen. `fromOutput`: das
    /// Kabel kommt aus einem Ausgang vom Typ `type` (gesucht: Nodes mit passendem Eingang); sonst aus einem
    /// Eingang (gesucht: Nodes mit passendem Ausgang).
    void setWireNeed(graph::PortType type, bool fromOutput);
    void clearWireNeed();
    /// Für die Prüfung (--exercise): Zahl der Typen mit Punkt und der abgedunkelten Typen (Favoriten-Kopien nicht doppelt).
    [[nodiscard]] int dotTypeCount() const;
    [[nodiscard]] int dimmedTypeCount() const;

    /// Bibliothek: Presets und Gruppen aus je einem Ordner (gehört dem Aufrufer) als eigene Rubriken unter den Favoriten.
    void setLibrary(const runtime::PresetStore* presets, const runtime::PresetStore* groups);
    /// Liest die Ordner neu ein (nach Speichern oder Löschen).
    void refreshLibrary();
    /// Zahl der Einträge der Bibliothek, die gerade sichtbar sind (für die Prüfung).
    [[nodiscard]] int libraryEntryCount() const;

    [[nodiscard]] QSize sizeHint() const override { return {230, 300}; }

Q_SIGNALS:
    /// Doppelklick oder Enter auf einem Eintrag.
    void addRequested(const QString& typeId);
    /// Doppelklick oder Enter auf einem Eintrag der Bibliothek: `kind` "preset" oder "group".
    void libraryAddRequested(const QString& kind, const QString& name);

private:
    void fill(const QString& filter);
    void applyStyles();
    void showContextMenu(const QPoint& position);
    void fillLibrary(bool filtered, const QString& filter, const QFont& bold);
    void showLibraryMenu(const QTreeWidgetItem& item, const QPoint& globalPosition);

    std::shared_ptr<const graph::INodeRegistry> registry_;
    QLineEdit* search_{nullptr};
    QTreeWidget* tree_{nullptr};
    QLabel* hint_{nullptr};
    QStringList favorites_;
    const runtime::PresetStore* presetStore_{nullptr};
    const runtime::PresetStore* groupStore_{nullptr};
    QSet<QString> expanded_;  // Namen der Kategorien, die der Nutzer geöffnet hat
    bool filling_{false};
    bool colored_{true};
    struct WireNeed {
        graph::PortType type;
        bool fromOutput;
    };
    std::optional<WireNeed> need_;
};

}  // namespace whitehole::studio
