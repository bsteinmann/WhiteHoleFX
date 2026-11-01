// settings_dialog.hpp: das Fenster "Einstellungen" (Netzwerk, DMX-Eingang, Audio, Node-Standardwerte).
//
// Warum es das gibt: Was zum Rechner und zum Auftritt gehört, nicht zum Projekt, steht an einer Stelle:
// welche Netzwerkkarte sendet und empfängt, welche DMX-Protokolle gehört werden, welches Audiogerät
// die Audio-Nodes speist. Der Dialog bearbeitet eine Kopie der AppSettings; angewendet und gespeichert
// wird erst bei "OK" durch das Hauptfenster.
#pragma once

#include <QDialog>

#include "app_settings.hpp"
#include "whitehole/graph/node_registry.hpp"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QRadioButton;
class QSpinBox;

namespace whitehole::studio {

class NodeDefaultsPage;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(const AppSettings& current, const graph::INodeRegistry& registry, QWidget* parent);

    /// Die Einstellungen, wie sie die Felder gerade zeigen.
    [[nodiscard]] AppSettings settings() const;
    /// Die Seite "Node-Standardwerte" (für den Rauchtest).
    [[nodiscard]] NodeDefaultsPage* nodeDefaultsPage() const noexcept { return defaultsPage_; }

private:
    QWidget* buildNetworkPage(const AppSettings& current);
    QWidget* buildDmxPage(const AppSettings& current);
    QWidget* buildAudioPage(const AppSettings& current);
    void chooseWav();

    NodeDefaultsPage* defaultsPage_{nullptr};
    QComboBox* outputCard_{nullptr};
    QComboBox* inputCard_{nullptr};
    QCheckBox* artnet_{nullptr};
    QCheckBox* sacn_{nullptr};
    QSpinBox* artnetPort_{nullptr};
    QSpinBox* sacnPort_{nullptr};
    QRadioButton* audioOff_{nullptr};
    QRadioButton* audioDefault_{nullptr};
    QRadioButton* audioDevice_{nullptr};
    QRadioButton* audioFile_{nullptr};
    QComboBox* audioDevices_{nullptr};
    QLineEdit* audioPath_{nullptr};
};

}  // namespace whitehole::studio
