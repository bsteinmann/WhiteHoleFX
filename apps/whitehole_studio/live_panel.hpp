// live_panel.hpp: die Bedienfläche für den Live-Betrieb.
//
// Warum es das gibt: Auf der Bühne muss ein Look mit einem Klick oder einer Taste abrufbar sein, ohne
// Editor, Parameter und Menüs. Dieses Panel zeigt für jeden gespeicherten Look einen großen Knopf (Tasten
// 1-9 für die ersten neun), dazu GO, die Überblendzeit und einen Balken für den laufenden Fade.
//
// Zwei Arten zu schalten:
//   - Direkt (Standard): ein Klick blendet sofort zum Look über - ein Handgriff, kaum Fehlbedienung.
//   - Flash (je Look, Rechtsklick): der Look läuft nur, solange der Knopf gedrückt ist; Vorrang vor "Mit GO".
//   - Mit GO: ein Klick lädt den Look nur vor ("Next", orange); GO (Leertaste) startet dann die Überblendung.
//     So kann der Shader vorher bereitgestellt werden, und der Fade selbst läuft ohne Ruckler.
// Farben: grün = läuft, orange = vorgeladen, gelb = blendet gerade ein.
//
// Einstellungen: Die Schaltfläche "Einstellungen" oben rechts blendet ein, was auf der Bühne nur stört - unter jedem
// Look "Flash" samt Zeiten zum Ein- und Zurückblenden, dazu die Zeile zum Belegen der Steuerung (Lernen). Aus = reine Auftrittsansicht.
//
// Die Logik steckt im Player (liveTrigger/liveArm/liveGo); das Panel ruft nur auf und zeigt liveStatus().
#pragma once

#include <QWidget>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "live_learn.hpp"
#include "whitehole/runtime/player.hpp"

class QCheckBox;
class QSpinBox;
class QDoubleSpinBox;
class QGridLayout;
class QIcon;
class QLabel;
class QProgressBar;
class QShortcut;
class QMenu;
class QPushButton;

namespace whitehole::studio {

class LivePanel : public QWidget {
    Q_OBJECT

public:
    explicit LivePanel(runtime::Player& player, QWidget* parent = nullptr);

    /// Baut die Felder des Rasters aus dem Projekt neu auf (nach Änderungen der Looks oder des Rasters). Hat ein älteres
    /// Projekt noch kein Raster, entsteht es hier aus den Looks.
    void rebuild();
    /// Spalten des Rasters (1 bis 16); ändert die Größe des Raster im Projekt und baut neu. Löst gridChanged nicht aus.
    void setColumns(int columns);
    [[nodiscard]] int columns() const { return columns_; }
    /// Zeilen des Rasters (1 bis 16); wie setColumns.
    void setRows(int rows);
    [[nodiscard]] int rows() const { return rows_; }
    /// Spalten und Zeilen auf einmal (ein Neuaufbau). Belegte Felder behalten Zeile und Spalte, belegte Felder außerhalb rücken
    /// in freie Felder nach; was dann noch keinen Platz hat, wird aus dem Raster genommen (der Look bleibt im Projekt).
    /// Fragt nicht nach (für Programmaufrufe, z. B. beim Wiederherstellen der Einstellungen). Löst gridChanged nicht aus.
    void setGrid(int columns, int rows);
    /// Wie setGrid, aber für eine Eingabe des Nutzers (die Zahlenfelder unter „Einstellungen“): Gingen dabei Looks aus dem
    /// Raster verloren, fragt das Panel vorher nach; bei „Abbrechen“ bleiben Raster und Zahlenfelder, wie sie waren.
    /// Liefert false, wenn abgebrochen wurde.
    bool requestGrid(int columns, int rows);
    /// Die Rückfrage von requestGrid, mit der Zahl der Looks, die aus dem Raster fielen (true = trotzdem verkleinern). Ohne
    /// eigene Funktion erscheint ein Dialog; der Rauchtest setzt eine, weil ein Dialog ohne Publikum blockieren würde.
    void setShrinkConfirm(std::function<bool(int lost)> confirm) { confirmShrink_ = std::move(confirm); }
    /// Vorgabe für ein Projekt ohne Raster (rebuild() legt es dann mit dieser Größe aus den Looks an); ändert ein vorhandenes
    /// Raster nicht und baut nichts neu. Hat das Projekt eines, gilt dessen Größe (rebuild() zieht die Felder nach).
    void setDefaultGrid(int columns, int rows);
    /// Haken „Vorschau teilen“ (links Ausgabe, rechts vorgeladener Look). Löst splitPreviewChanged nicht aus.
    void setSplitPreview(bool on);
    [[nodiscard]] bool splitPreview() const;
    /// Gleicht Farben, Name und Balken mit dem Zustand des Players ab (jeden Frame; billig).
    void refresh();

    /// Das Feld `index` (zeilenweise) wird abgerufen: Look direkt überblenden oder (Modus "Mit GO") vorladen. Leeres Feld: false.
    bool trigger(int index);
    /// Startet die Überblendung zum vorgeladenen Look.
    bool go();
    /// Zahl der Felder des Rasters (belegte und leere).
    [[nodiscard]] int buttonCount() const { return static_cast<int>(buttons_.size()); }
    void setUseGo(bool useGo);
    [[nodiscard]] bool useGo() const;
    /// Eigene Überblendzeit statt der des Looks; leer = Zeit des Looks.
    void setFadeOverride(std::optional<double> seconds);
    /// Die eingestellte eigene Überblendzeit; leer = Zeit des Looks.
    [[nodiscard]] std::optional<double> fadeOverride() const;

    // --- Drücken/Loslassen, Flash, Lernen (live_panel_looks.cpp) ------------------------------------------------
    /// Knopf eines Feldes gedrückt bzw. losgelassen (Player::liveCellDown/-Up: ein Flash-Feld kehrt beim Loslassen zurück).
    bool press(int index);
    void release(int index);
    /// Wie das *Feld* ausgelöst wird (umschalten/flashen, Zeit zum Zurückblenden; leer = Zeit des Looks).
    bool setLookTrigger(int index, project::LookTrigger trigger, std::optional<double> returnSeconds);
    /// Zeit, in der ein Flash-Feld beim Drücken einblendet (leer = Zeit des Looks bzw. die eigene Zeit des Live-Fensters).
    bool setLookFlashIn(int index, std::optional<double> seconds);
    /// Legt den Look mit dem Namen `lookName` auf Feld `index` (ein belegtes Feld behält Modus und Zeiten, ein leeres übernimmt
    /// die Vorgaben des Looks) und baut das Raster neu auf. false bei unbekanntem Look oder Feld.
    bool assignLook(int index, const std::string& lookName);
    /// Leert das Feld (der Look bleibt im Projekt) und baut das Raster neu auf.
    bool clearCell(int index);
    /// Das Rechtsklick-Menü eines Feldes: Look legen/wechseln, leeren, Modus und Zeiten (aufgebaut, nicht gezeigt; der Aufrufer
    /// besitzt es).
    [[nodiscard]] QMenu* lookMenu(int index);
    /// Gelernte Quelle für eine Live-Funktion (dieselbe Belegung wie im Reiter "Steuerung").
    using LearnHandler = std::function<void(runtime::LiveFunction, const LearnedSource&)>;
    /// Füllt die Zeile "Steuerung belegen" der Einstellungsleiste mit einem Lernen-Knopf je Funktion (MIDI und DMX).
    void setLearning(std::shared_ptr<midi::MidiStore> midi, std::shared_ptr<const input::DmxStore> dmx, LearnHandler handler);

    // --- Einstellungen ein-/ausblenden (live_panel_looks.cpp) --------------------------------------------------
    /// Blendet die Einstellungsleiste (Belegung lernen) und unter jedem Look "Flash" samt Zurückblenden ein oder aus.
    /// Standard: aus, damit die Auftrittsansicht aufgeräumt bleibt. Das Hauptfenster blendet mit ihr die Menüleiste ein.
    void setSettingsShown(bool shown);
    [[nodiscard]] bool settingsShown() const { return settingsShown_; }
    /// Das Feld "Flash" unter dem Look-Knopf (nur sichtbar, solange die Einstellungen offen sind); nullptr bei ungültigem Index.
    [[nodiscard]] QCheckBox* flashCheck(int index) const;

Q_SIGNALS:
    void failed(const QString& message);
    /// Der Knopf "Zurück zum Editor" (oder Esc).
    void leaveRequested();
    /// Die Einstellungen wurden ein- oder ausgeblendet (Schaltfläche oben rechts oder setSettingsShown).
    void settingsToggled(bool shown);
    /// Der Nutzer hat die Rasterbreite in den Einstellungen geändert.
    void gridChanged(int columns, int rows);
    void splitPreviewChanged(bool on);

private:
    [[nodiscard]] static QString styleFor(const QString& color);
    [[nodiscard]] static QString emptyStyle();  // leeres Feld: gestrichelter Rahmen
    void syncGridBoxes();                        // Spalten-/Zeilenfelder auf columns_/rows_ ziehen, ohne Signale
    std::function<bool(int)> confirmShrink_;     // Rückfrage von requestGrid; leer = Dialog
    bool changeCell(int index, const project::LiveCell& value);  // Player::setLiveCell, Fehler melden
    [[nodiscard]] QString buttonText(int index) const;
    /// Standbild des Looks als Icon für den Knopf; leer, wenn der Look keines hat (ältere Projekte).
    [[nodiscard]] QIcon thumbnailIcon(int index) const;
    [[nodiscard]] QString flashInText(int index) const;
    [[nodiscard]] QString returnText(int index) const;
    /// Gleicht Knopftext, Flash-Haken und Zurückblend-Knopf eines Looks mit dem Player ab.
    void syncLookOptions(int index);
    /// Fragt die Zeit zum Einblenden (beim Drücken) oder Zurückblenden (beim Loslassen) eines Flash-Looks ab.
    void askFlashSeconds(int index, bool returnTime);

    runtime::Player& player_;
    QGridLayout* grid_{nullptr};
    QWidget* gridHost_{nullptr};
    std::vector<QWidget*> cells_;            // je Look: Knopf plus Reihe mit den Einstellungen darunter
    std::vector<QPushButton*> buttons_;      // die Look-Knöpfe selbst
    std::vector<QWidget*> optionRows_;       // Reihe "Flash / Zurückblenden" unter dem Knopf (nur bei offenen Einstellungen)
    std::vector<QCheckBox*> flashChecks_;
    std::vector<QPushButton*> inButtons_;
    std::vector<QPushButton*> returnButtons_;
    std::vector<QShortcut*> digitKeys_;  // Tasten 1 bis 9 (aus, solange die Cuelist läuft)
    QLabel* nowLabel_{nullptr};
    QLabel* nextLabel_{nullptr};
    QProgressBar* progress_{nullptr};
    QPushButton* goButton_{nullptr};
    QCheckBox* useGo_{nullptr};
    QCheckBox* customFade_{nullptr};
    QDoubleSpinBox* fade_{nullptr};
    QPushButton* settingsToggle_{nullptr};  // "Einstellungen", oben rechts neben "Zurück zum Editor"
    QWidget* settingsBar_{nullptr};         // Hinweis und Zeile "Steuerung belegen", Standard: ausgeblendet
    QWidget* learnRow_{nullptr};            // gefüllt von setLearning
    bool settingsShown_{false};
    int columns_{4};
    int rows_{4};
    QSpinBox* columnsBox_{nullptr};
    QSpinBox* rowsBox_{nullptr};
    QCheckBox* splitCheck_{nullptr};
};

}  // namespace whitehole::studio
