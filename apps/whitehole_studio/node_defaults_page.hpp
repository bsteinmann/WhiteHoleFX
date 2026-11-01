// node_defaults_page.hpp: die Einstellungsseite "Node-Standardwerte".
//
// Warum es das gibt: Wer Nodes immer wieder mit denselben Werten braucht, stellt sie hier einmal ein; neue
// Nodes starten damit statt mit dem Wert aus der Bibliothek (Player::addNode wendet sie an). Links die Node-Typen
// nach Kategorie mit Suchfeld, rechts die Parameter des gewählten Typs mit denselben Bedienelementen wie im
// Parameterfenster und einem "↺" je Zeile, das auf den Bibliothekswert zurückstellt.
//
// Die Seite bearbeitet eine Kopie; ein Wert, der dem Bibliothekswert gleicht, ist kein eigener Standardwert
// und wird entfernt. Eigene Datei, weil settings_dialog.cpp sonst die Grenze von 400 Zeilen reißt.
#pragma once

#include <QString>
#include <QWidget>

#include <map>
#include <string>

#include "whitehole/graph/node_registry.hpp"
#include "whitehole/runtime/node_defaults.hpp"

class QLineEdit;
class QScrollArea;
class QTreeWidget;
class QTreeWidgetItem;

namespace whitehole::studio {

class NodeDefaultsPage : public QWidget {
    Q_OBJECT

public:
    NodeDefaultsPage(const graph::INodeRegistry& registry, const runtime::NodeDefaults& current, QWidget* parent);

    /// Die Standardwerte, wie die Seite sie gerade zeigt.
    [[nodiscard]] const runtime::NodeDefaults& defaults() const noexcept { return defaults_; }

    /// Zeigt die Parameter dieses Node-Typs; liefert die Zahl der gebauten Zeilen (0 bei unbekanntem Typ).
    int showType(const QString& typeId);

private:
    void rebuildRows(const graph::NodeDescriptor& descriptor);
    void store(const std::string& typeId, const graph::ParameterDescriptor& parameter, const graph::ParameterValue& value);
    void markType(const std::string& typeId);
    void applyFilter(const QString& text);
    void resetAll();

    const graph::INodeRegistry& registry_;
    runtime::NodeDefaults defaults_;
    QLineEdit* filter_{nullptr};
    QTreeWidget* tree_{nullptr};
    QScrollArea* rows_{nullptr};
    std::map<std::string, QTreeWidgetItem*> items_;
};

}  // namespace whitehole::studio
