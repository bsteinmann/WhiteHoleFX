// main_window_exercise.cpp: Rauchtests des Hauptfensters für die CI (--exercise).
//
// Sie bedienen die Oberfläche so, wie ein Mensch es täte (Regler, Node-Auswahl, Neu, Farbkästchen),
// und melden, was nicht wie erwartet reagiert. Eigene Datei, weil sie mit dem normalen Betrieb
// nichts zu tun haben und main_window.cpp sonst zu lang wird.
#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QSpinBox>

#include "layout_panel.hpp"
#include "preview_controller.hpp"
#include "main_window.hpp"
#include "node_editor.hpp"
#include "parameter_panel.hpp"

namespace whitehole::studio {

int MainWindow::exerciseParameters() {
    int changed = 0;
    for (QSlider* slider : parameters_->findChildren<QSlider*>()) {
        if (slider->isEnabled()) {
            slider->setValue((slider->minimum() + slider->maximum()) / 3);
            ++changed;
        }
    }
    for (QDoubleSpinBox* spin : parameters_->findChildren<QDoubleSpinBox*>()) {
        if (spin->isEnabled()) {
            spin->setValue(spin->value() + spin->singleStep());
            ++changed;
        }
    }
    for (QSpinBox* spin : parameters_->findChildren<QSpinBox*>()) {
        if (spin->isEnabled()) {
            spin->setValue(spin->value() + 1);
            ++changed;
        }
    }
    for (QCheckBox* check : parameters_->findChildren<QCheckBox*>()) {
        // "Transparent" prüft exerciseTransparency gezielt; hier würde es das Bild schwärzen.
        if (check->isEnabled() && check->text() != QStringLiteral("Transparent")) {
            check->toggle();
            ++changed;
        }
    }
    return changed;
}

QString MainWindow::exerciseEditor() {
    QString ok = editor_->exercise();
    if (ok.isEmpty()) {
        ok = layoutPanel_->exercise();
    }
    if (ok.isEmpty()) {
        ok = previewController_->exercise();
    }
    if (ok.isEmpty()) {
        // Alles zurücknehmen und wiederholen: dieselben Schritte wie Strg+Z und Strg+Umschalt+Z.
        int undone = 0;
        while (player_->canUndo() && undone < 10000) {
            onUndo();
            ++undone;
        }
        int redone = 0;
        while (player_->canRedo() && redone < 10000) {
            onRedo();
            ++redone;
        }
        if (undone == 0 || undone != redone) {
            ok = tr("Rückgängig/Wiederholen: %1 zurückgenommen, %2 wiederholt").arg(undone).arg(redone);
        }
    }
    for (int i = 0; i < 5; ++i) {
        tick();
        QApplication::processEvents();
    }
    return ok;
}

QString MainWindow::exerciseSelection() {
    const std::vector<runtime::NodeInfo> nodes = player_->nodes();
    if (nodes.size() < 2) {
        return tr("Auswahl: zu wenige Nodes mit Parametern im Projekt (%1)").arg(nodes.size());
    }
    for (const runtime::NodeInfo& info : nodes) {
        if (!editor_->selectNode(info.id)) {
            return tr("Auswahl: Node %1 fehlt im Editor").arg(QString::fromStdString(info.title));
        }
        if (parameters_->shownNode() != info.id) {
            return tr("Auswahl: Klick auf \"%1\" hebt dessen Parameter nicht hervor").arg(QString::fromStdString(info.title));
        }
    }
    // Ein Neuaufbau (jede Graphänderung) darf die Auswahl nicht verlieren.
    const NodeId last = nodes.back().id;
    editor_->rebuild();
    refreshParameters();
    if (editor_->selectedNode() != last || parameters_->shownNode() != last) {
        return tr("Auswahl: nach dem Neuaufbau ist die Auswahl weg");
    }
    return {};
}

QString MainWindow::exerciseNewProject() {
    if (!newProject(false)) {
        return tr("Neu: Vorlage lässt sich nicht anlegen: %1").arg(problem());
    }
    if (player_->nodes().empty() || player_->status().ledCount == 0) {
        return tr("Neu: Vorlage hat keine Nodes oder keine LEDs");
    }
    if (player_->modified()) {
        return tr("Neu: frisches Projekt gilt schon als geändert");
    }
    for (int i = 0; i < 5; ++i) {
        tick();
        QApplication::processEvents();
    }
    return problem();
}

QString MainWindow::exerciseTransparency() {
    // Das Kästchen "Transparent" jeder Farbe muss Alpha 0 setzen und wieder auf 1 stellen.
    const auto alphaOfFirstColor = [this]() -> std::optional<float> {
        for (const runtime::NodeInfo& node : player_->nodes()) {
            for (const runtime::ParameterInfo& parameter : node.parameters) {
                if (const auto* color = std::get_if<ColorRgba>(&parameter.value)) {
                    return color->a;
                }
            }
        }
        return std::nullopt;
    };
    QCheckBox* box = nullptr;
    for (QCheckBox* check : parameters_->findChildren<QCheckBox*>()) {
        if (check->text() == QStringLiteral("Transparent") && check->isEnabled()) {
            box = check;
            break;
        }
    }
    if (box == nullptr) {
        return tr("Transparent: kein Kästchen gefunden");
    }
    box->setChecked(true);
    for (int i = 0; i < 3; ++i) {
        tick();
        QApplication::processEvents();
    }
    const std::optional<float> clear = alphaOfFirstColor();
    if (!clear || *clear != 0.0F) {
        return tr("Transparent: Alpha ist nach dem Anhaken nicht 0");
    }
    QCheckBox* again = nullptr;
    for (QCheckBox* check : parameters_->findChildren<QCheckBox*>()) {
        if (check->text() == QStringLiteral("Transparent") && check->isChecked()) {
            again = check;
            break;
        }
    }
    if (again == nullptr) {
        return tr("Transparent: Kästchen zeigt den Wert nicht an");
    }
    again->setChecked(false);
    const std::optional<float> opaque = alphaOfFirstColor();
    if (!opaque || *opaque != 1.0F) {
        return tr("Transparent: Alpha ist nach dem Abhaken nicht 1");
    }
    for (int i = 0; i < 3; ++i) {
        tick();
        QApplication::processEvents();
    }
    return problem();
}

}  // namespace whitehole::studio
