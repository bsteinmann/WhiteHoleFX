// midi_profile_box.hpp: Auswahl und Verwaltung der Live-Profile für den MIDI-Controller (Seite "MIDI").
//
// Warum es das gibt: Welche Pads Looks abrufen und welche Taste GO ist, legt ein Profil fest (midi/live_profile.hpp).
// Zwei Profile sind vorgefertigt und schreibgeschützt; eigene entstehen als Kopie, lassen sich bearbeiten, umbenennen
// und löschen. Gespeichert wird beim Nutzer (Programmeinstellungen, AppSettings::midiProfiles), nicht im Projekt.
// Eigene Datei, damit midi_settings_page.cpp klein bleibt.
#pragma once

#include <QGroupBox>
#include <QStringList>

#include <vector>

#include "whitehole/midi/live_profile.hpp"

class QComboBox;
class QPushButton;
class QSpinBox;

namespace whitehole::studio {

/// Alle Profile: erst die vorgefertigten, dann die eigenen aus `userLines` (unlesbare Zeilen fallen weg).
[[nodiscard]] std::vector<midi::LiveProfile> allLiveProfiles(const QStringList& userLines);
/// Das Profil mit diesem Namen, sonst das erste vorgefertigte.
[[nodiscard]] midi::LiveProfile findLiveProfile(const QStringList& userLines, const QString& name);

class MidiProfileBox : public QGroupBox {
    Q_OBJECT

public:
    MidiProfileBox(const QString& active, const QStringList& userLines, QWidget* parent = nullptr);

    /// Name des gewählten Profils.
    [[nodiscard]] QString active() const;
    /// Die eigenen Profile als Textzeilen (zum Speichern).
    [[nodiscard]] QStringList userProfiles() const;

    /// Die Aktionen hinter den Knöpfen ohne Rückfrage (für den Rauchtest). false: Name leer oder schon vergeben,
    /// bzw. ein vorgefertigtes Profil soll umbenannt oder gelöscht werden.
    bool copyCurrent(const QString& name);
    bool renameCurrent(const QString& name);
    bool removeCurrent();
    bool select(const QString& name);

private:
    void fillCombo(const QString& select);
    void showCurrent();
    void storeFields();
    [[nodiscard]] int currentIndex() const;
    [[nodiscard]] bool nameFree(const QString& name) const;

    std::vector<midi::LiveProfile> profiles_;
    QComboBox* combo_{nullptr};
    QSpinBox* columns_{nullptr};
    QSpinBox* go_{nullptr};
    QSpinBox* stop_{nullptr};
    QSpinBox* master_{nullptr};
    QSpinBox* fade_{nullptr};
    QSpinBox* crossfade_{nullptr};
    QPushButton* rename_{nullptr};
    QPushButton* remove_{nullptr};
    bool updating_{false};
};

}  // namespace whitehole::studio
