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
// Die Palette kennt nur das Verzeichnis der Node-Typen, nicht den Graphen.
#pragma once

#include <QString>
#include <QWidget>

#include <memory>

#include "whitehole/graph/node_registry.hpp"

class QLabel;
class QLineEdit;
class QTreeWidget;

namespace whitehole::studio {

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
    /// Zeigt die Palette als "Papierkorb": rot mit dem Hinweis, dass Loslassen den Node entfernt.
    void setRemoveHighlight(bool on);

    [[nodiscard]] QSize sizeHint() const override { return {230, 300}; }

Q_SIGNALS:
    /// Doppelklick oder Enter auf einem Eintrag.
    void addRequested(const QString& typeId);

private:
    void fill(const QString& filter);

    std::shared_ptr<const graph::INodeRegistry> registry_;
    QLineEdit* search_{nullptr};
    QTreeWidget* tree_{nullptr};
    QLabel* hint_{nullptr};
};

}  // namespace whitehole::studio
