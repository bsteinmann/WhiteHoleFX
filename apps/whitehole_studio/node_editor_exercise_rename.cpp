// node_editor_exercise_rename.cpp: Teil des Rauchtests (node_editor_exercise.cpp), ausgelagert wegen
// der 400-Zeilen-Grenze.
//
// Prüft den eigenen Node-Namen (vom Nutzer gewünscht: "DMX nodes mussen benamt werden können, sonst
// gerät man schnell durcheinander") - renameNodeInteractive selbst öffnet einen echten QInputDialog
// (würde im Rauchtest ohne Publikum blockieren), darum hier direkt über Player::renameNode, wie die
// UI-Methode es nach der Eingabe auch täte.
#include <QString>

#include "node_editor.hpp"

namespace whitehole::studio {

QString NodeEditor::exerciseRename(NodeId sink, const QString& defaultTitle) {
    // defaultTitle kommt vom Aufrufer (direkt nach dem Anlegen abgelesen): im geladenen Projekt kann schon
    // ein weiterer Node desselben Typs stecken, dann zeigt auch dieser unbenannte Node "Mirror 1"/"Mirror 2"
    // statt schlicht "Mirror" (siehe node_display_name.hpp) - ein fester Vergleichswert wäre hier falsch.
    NodeItem* item = itemOf(sink);
    if (item == nullptr || item->title() != defaultTitle) {
        return QStringLiteral("Node-Titel ohne eigenen Namen zeigt nicht den Typnamen");
    }
    if (!player_.renameNode(sink, "Mein Mirror").ok()) {
        return QStringLiteral("renameNode schlägt fehl");
    }
    rebuild();
    item = itemOf(sink);
    if (item == nullptr || item->title() != QStringLiteral("Mein Mirror")) {
        return QStringLiteral("Node-Titel zeigt nach renameNode nicht den eigenen Namen");
    }
    if (!player_.renameNode(sink, "").ok()) {
        return QStringLiteral("renameNode (leerer Name) schlägt fehl");
    }
    rebuild();
    item = itemOf(sink);
    if (item == nullptr || item->title() != defaultTitle) {
        return QStringLiteral("Leerer Name löscht den eigenen Namen nicht wieder (Titel fällt nicht auf den Typ zurück)");
    }
    return {};
}

}  // namespace whitehole::studio
