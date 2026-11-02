// cue_dialog.hpp: Fenster zum Bearbeiten eines Cues (Look, Auslöser, Delay, Fade).
//
// Warum es das gibt (Issue #21): Die Tabelle der Cuelist zeigt alles auf einen Blick, aber ein Cue hat zu viele
// Einstellungen für Tabellenzellen (je nach Auslöser Zeit, Taste oder DMX-Kanal). Der Dialog zeigt nur die Felder,
// die zum gewählten Auslöser gehören, und liefert den fertigen Cue.
#pragma once

#include <QDialog>

#include <vector>

#include "whitehole/project/cue.hpp"
#include "whitehole/project/project.hpp"

class QComboBox;
class QDoubleSpinBox;
class QKeySequenceEdit;
class QLineEdit;
class QSpinBox;

namespace whitehole::studio {

class CueDialog : public QDialog {
    Q_OBJECT

public:
    CueDialog(const project::Cue& cue, const std::vector<project::Look>& looks, QWidget* parent);

    /// Der Cue, wie ihn die Felder gerade zeigen.
    [[nodiscard]] project::Cue cue() const;

private:
    void updateEnabled();

    QLineEdit* name_{nullptr};
    QComboBox* look_{nullptr};
    QComboBox* trigger_{nullptr};
    QDoubleSpinBox* time_{nullptr};
    QKeySequenceEdit* key_{nullptr};
    QComboBox* dmxProtocol_{nullptr};
    QSpinBox* dmxUniverse_{nullptr};
    QSpinBox* dmxChannel_{nullptr};
    QSpinBox* dmxThreshold_{nullptr};
    QDoubleSpinBox* delay_{nullptr};
    QDoubleSpinBox* fade_{nullptr};
    QString keyText_;  // Taste, wie sie in der Datei steht, falls das Feld sie nicht darstellen kann
};

}  // namespace whitehole::studio
