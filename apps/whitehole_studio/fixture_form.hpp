// fixture_form.hpp: Formular für ein Fixture (Name, Lage, Drehung, welches Fixture es ist).
//
// "Typ wird Pflicht" (vom Nutzer entschieden, siehe CLAUDE.md): die Form selbst wird hier nicht mehr
// bearbeitet, nur noch Name, Lage, Drehung und welches Fixture (aus der Bibliothek, Reiter Fixtures)
// gilt - ändern lässt sich die Form nur noch dort. Ein Fixture aus einer älteren Projektdatei ohne
// eigenes Fixture zeigt stattdessen einen Hinweis; "Neues Fixture aus dieser Form …" legt für genau
// diesen Fall (oder um ein bestehendes Fixture als Vorlage für ein neues zu nehmen) eines aus der
// aktuellen Form an und weist es zu. Jede Änderung geht über den Player, der danach den Ausgabeplan prüft. Wird
// sie abgelehnt (Universe belegt, Name vergeben), meldet `failed` den Grund und das Formular springt
// auf den gespeicherten Wert zurück.
#pragma once

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "form_helpers.hpp"
#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

class FixtureForm : public QWidget {
    Q_OBJECT

public:
    explicit FixtureForm(runtime::Player& player, QWidget* parent = nullptr);

    /// Zeigt das Fixture (oder, wenn es nicht (mehr) existiert, ein leeres Formular).
    void showFixture(mapping::FixtureId id);
    [[nodiscard]] mapping::FixtureId current() const noexcept { return id_; }

    /// Rauchtest für die CI: ändert Ursprung und Drehung des angezeigten Fixtures über die Felder und legt
    /// über "Neues Fixture …" ein Fixture aus dessen Form an. Leer bei Erfolg, sonst der gescheiterte Schritt.
    QString exercise();
    /// Rauchtest für ein Fixture, das schon ein zugewiesenes Fixture hat: dreht um die Achsen y und z und
    /// wechselt auf ein anderes Fixture. Leer bei Erfolg.
    QString exerciseTyped();

Q_SIGNALS:
    void edited();
    void failed(const QString& message);

private:
    struct Controls {
        QLineEdit* name{nullptr};
        form::Vec3Boxes origin;
        form::Vec3Boxes rotation;  // Drehung in Grad um x, y, z
        QComboBox* type{nullptr};  // Fixture aus der Bibliothek; letzter Eintrag legt ein neues an
    };

    void build();
    void report(const Result<void>& result, bool rebuildAfter = false);

    void applyName();
    void applyOrigin();
    void applyRotation();
    void applyType();

    runtime::Player& player_;
    mapping::FixtureId id_;
    QVBoxLayout* outer_{nullptr};
    QWidget* content_{nullptr};
    Controls c_;
};

}  // namespace whitehole::studio
