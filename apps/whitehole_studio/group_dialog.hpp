// group_dialog.hpp: Fenster zum Anlegen und Bearbeiten einer Gruppe von Nodes.
//
// Warum es das gibt: Eine Gruppe zeigt als Kachel nur, was man von außen braucht. Welche Ein- und Ausgänge und
// welche Parameter das sind, wählt man hier: pro Node eine Zeile mit Haken, daneben die Beschriftung auf der Kachel.
// Der Dialog kennt weder Player noch Graph; der Editor füllt ihn mit `GroupDialogMember` und baut aus der Antwort die
// Gruppe (node_editor_group.cpp).
#pragma once

#include <QColor>
#include <QDialog>
#include <QString>

#include <string>
#include <vector>

#include "whitehole/core/ids.hpp"

class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace whitehole::studio {

/// Ein wählbarer Port oder Parameter eines Mitglieds.
struct GroupDialogEntry {
    std::string key;
    QString name;   // so heißt er im Node
    QString label;  // Beschriftung auf der Kachel (änderbar)
    bool output{false};
    bool checked{false};
};

struct GroupDialogMember {
    NodeId id;
    QString title;
    std::vector<GroupDialogEntry> ports;
    std::vector<GroupDialogEntry> parameters;
};

/// Die Antwort: für jedes Mitglied die angehakten Einträge mit ihrer Beschriftung.
struct GroupDialogChoice {
    NodeId node;
    GroupDialogEntry entry;
};

class GroupDialog : public QDialog {
    Q_OBJECT

public:
    /// `takenNames`: Namen, die schon andere Gruppen tragen (nicht erlaubt).
    GroupDialog(const QString& title, const QString& name, const QColor& color, std::vector<GroupDialogMember> members,
                std::vector<QString> takenNames, QWidget* parent);

    [[nodiscard]] QString name() const;
    [[nodiscard]] QColor color() const noexcept { return color_; }
    [[nodiscard]] std::vector<GroupDialogChoice> chosenPorts() const;
    [[nodiscard]] std::vector<GroupDialogChoice> chosenParameters() const;
    /// Prüft Name und Auswahl wie ein Klick auf OK; leer = in Ordnung, sonst der Hinweis an den Nutzer.
    [[nodiscard]] QString problem() const;

    void accept() override;

private:
    struct Row {
        NodeId node;
        GroupDialogEntry entry;
        bool parameter{false};
        QTreeWidgetItem* item{nullptr};
        QLineEdit* label{nullptr};
    };
    [[nodiscard]] std::vector<GroupDialogChoice> chosen(bool parameters) const;
    void updateColorButton();

    QLineEdit* name_{nullptr};
    QPushButton* colorButton_{nullptr};
    QTreeWidget* tree_{nullptr};
    QColor color_;
    std::vector<Row> rows_;
    std::vector<QString> taken_;
};

}  // namespace whitehole::studio
