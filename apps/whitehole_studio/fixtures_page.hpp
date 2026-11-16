// fixtures_page.hpp: Seite "Fixtures" im Stage-Config-Fenster: Liste aller Fixture-Exemplare dieser Show.
//
// Warum es diese Datei gibt: diese Seite zeigte früher (FixtureLibraryPage) die Bauformen - auf Wunsch des
// Nutzers zeigt sie jetzt stattdessen echte Fixture-Exemplare (mapping::Fixture, nicht project::FixtureType):
// einfach eine Liste, in der alle Fixtures der Show stehen. Anlegen (Dialog "Fixture hinzufügen", "Matrix"/
// "Strip", aus einer Bauform), Lage/Drehung/Bauform ändern, Ausrichten und Entfernen bleibt Aufgabe des Reiters
// Layout; dort steht auch die Bauform-Bearbeitung selbst (siehe layout_panel.hpp). Diese Seite hält keinen
// eigenen Zustand, sie liest nur aus dem Player.
#pragma once

#include <QString>
#include <QWidget>

#include "whitehole/runtime/player.hpp"

class QListWidget;

namespace whitehole::studio {

class FixturesPage : public QWidget {
    Q_OBJECT

public:
    explicit FixturesPage(runtime::Player& player, QWidget* parent = nullptr);

    /// Liest die Liste neu aus dem Player (nach Laden, Undo, Redo, Anlegen/Entfernen anderswo).
    void rebuild();
    /// Wie rebuild(), aber nur wenn die Seite sichtbar ist; sonst beim nächsten Anzeigen.
    void refresh();

    /// Rauchtest für die CI: legt ein Fixture über den Player an, prüft, dass es in der Liste auftaucht
    /// (Name, Bauform bzw. „eigene Form“, LED-Zahl), entfernt es wieder und prüft, dass es verschwindet.
    QString exercise();

protected:
    void showEvent(QShowEvent* event) override;

private:
    void fillList();

    runtime::Player& player_;
    QListWidget* list_;
    bool dirty_{false};  // Projekt hat sich geändert, während die Seite unsichtbar war
};

}  // namespace whitehole::studio
