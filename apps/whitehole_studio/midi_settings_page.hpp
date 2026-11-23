// midi_settings_page.hpp: die Seite "MIDI" im Einstellungsfenster (Eingangs- und Ausgangsport, letzte Nachricht,
// Live-Profil für den Controller).
//
// Warum es das gibt: Welcher Controller (z. B. Akai APC mini) angesteckt ist, hängt vom Rechner und vom Auftritt ab,
// nicht vom Projekt - wie bei Netzwerkkarte und Audio-Gerät. Die Seite wählt die Ports über ihren Namen, damit die Wahl
// ein Ab- und Anstecken überlebt, und zeigt die zuletzt empfangene Nachricht, damit man sofort sieht, ob das Gerät ankommt.
// Eigene Datei, damit settings_dialog.cpp klein bleibt.
#pragma once

#include <QStringList>
#include <QWidget>

#include <memory>

#include "whitehole/midi/midi_store.hpp"

class QComboBox;
class QLabel;
class QTimer;

namespace whitehole::studio {

class MidiProfileBox;

class MidiSettingsPage : public QWidget {
    Q_OBJECT

public:
    /// `input`/`output`: gemerkte Portnamen (leer = aus). `store`: liefert die letzte Nachricht (darf fehlen).
    MidiSettingsPage(const QString& input, const QString& output, std::shared_ptr<midi::MidiStore> store,
                     QWidget* parent = nullptr, const QString& profile = {}, const QStringList& profiles = {});

    [[nodiscard]] QString input() const;
    [[nodiscard]] QString output() const;
    [[nodiscard]] MidiProfileBox* profiles() const noexcept { return profiles_; }

private:
    void refreshPorts();
    void showLast();

    QComboBox* input_{nullptr};
    QComboBox* output_{nullptr};
    QLabel* last_{nullptr};
    MidiProfileBox* profiles_{nullptr};
    QTimer* timer_{nullptr};
    std::shared_ptr<midi::MidiStore> store_;
};

}  // namespace whitehole::studio
