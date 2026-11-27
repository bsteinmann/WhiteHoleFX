// live_controls_page.hpp: der Reiter "Steuerung" im Einstellungsfenster - Belegung der Live-Funktionen auf MIDI und DMX.
//
// Warum es das gibt: GO, Live beenden, Master, Überblendzeit und manuelle Überblendung sollen ohne Zahlenraten auf eine
// Taste, einen Regler oder einen DMX-Kanal kommen. Je Funktion eine Zeile: MIDI (Profil oder gelernt, "Auf Profil
// zurücksetzen") und DMX (gelernt oder aus). Dieselbe Belegung schreibt auch das Live-Fenster (runtime::LiveBindings).
// Master über DMX ist die schon vorhandene Einstellung der Seite "DMX-Eingang"; die Zeile hier zeigt und ändert genau sie
// (Signal masterDmxChanged, das Einstellungsfenster gleicht beide Seiten ab). Darunter die Quelle der gemeinsamen
// Tempo-Uhr (Tap, fester Wert, Audio-Beat oder DMX-Kanal); getappt wird mit der Funktion "Tempo-Tap" oder der Taste TAP im Live-Fenster.
#pragma once

#include <QWidget>

#include <array>
#include <memory>
#include <optional>

#include "app_settings.hpp"
#include "whitehole/input/dmx_store.hpp"
#include "whitehole/midi/midi_store.hpp"
#include "whitehole/runtime/live_bindings.hpp"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

namespace whitehole::studio {

class LiveControlsPage : public QWidget {
    Q_OBJECT

public:
    LiveControlsPage(const AppSettings& current, std::shared_ptr<midi::MidiStore> midi,
                     std::shared_ptr<const input::DmxStore> dmx, QWidget* parent = nullptr);

    [[nodiscard]] const runtime::LiveBindings& bindings() const noexcept { return bindings_; }
    /// Stand von "Master über DMX" auf der Seite DMX-Eingang (leer = aus).
    void setMasterDmx(std::optional<input::DmxAddress> address);

    /// Was die Knöpfe tun, ohne zu warten (auch für den Rauchtest).
    void learnMidi(runtime::LiveFunction function, runtime::MidiBinding binding);
    void learnDmx(runtime::LiveFunction function, input::DmxAddress address);
    void resetMidi(runtime::LiveFunction function);
    void clearDmx(runtime::LiveFunction function);
    [[nodiscard]] QString midiText(runtime::LiveFunction function) const;
    [[nodiscard]] QString dmxText(runtime::LiveFunction function) const;
    /// Gemeinsame Tempo-Uhr: Quelle (0 = Tap, 1 = fester Wert, 2 = Audio-Beat, 3 = DMX-Kanal) und fester Wert in BPM.
    [[nodiscard]] int tempoSource() const;
    [[nodiscard]] double tempoFixedBpm() const;
    /// DMX-Kanal der Quelle "DMX-Kanal" (Protokoll 0 = Art-Net, 1 = sACN).
    [[nodiscard]] int tempoDmxProtocol() const;
    [[nodiscard]] int tempoDmxUniverse() const;
    [[nodiscard]] int tempoDmxChannel() const;

Q_SIGNALS:
    /// Die Zeile Master hat für DMX gelernt bzw. ausgeschaltet.
    void masterDmxChanged(std::optional<input::DmxAddress> address);

private:
    void showRow(runtime::LiveFunction function);

    runtime::LiveBindings bindings_;
    midi::LiveProfile profile_;
    std::optional<input::DmxAddress> master_;
    std::array<QLabel*, runtime::kLiveFunctionCount> midiLabels_{};
    std::array<QLabel*, runtime::kLiveFunctionCount> dmxLabels_{};
    QComboBox* tempoSource_{nullptr};
    QDoubleSpinBox* tempoBpm_{nullptr};
    QComboBox* tempoDmxProtocol_{nullptr};
    QSpinBox* tempoDmxUniverse_{nullptr};
    QSpinBox* tempoDmxChannel_{nullptr};
};

}  // namespace whitehole::studio
