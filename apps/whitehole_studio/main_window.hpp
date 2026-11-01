// main_window.hpp: das Hauptfenster.
//
// Aufgaben: Projekt öffnen (Menü, Drag & Drop, Kommandozeile), Start/Stopp, Netzwerkausgabe ein-
// und ausschalten, Vorschau im Takt der Projekt-Bildrate aktualisieren, Status anzeigen.
//
// Sicherheit: Die Netzwerkausgabe ist nach dem Start AUS. Ein Projekt läuft und zeigt sich, ohne
// dass ein Paket das Haus verlässt; erst "Ausgabe senden" schaltet das ein.
#pragma once

#include <QKeySequence>
#include <QMainWindow>
#include <QTimer>

#include <array>
#include <memory>
#include <vector>

#include "whitehole/audio/audio_input.hpp"
#include "whitehole/input/dmx_receiver.hpp"
#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/output/udp_sender.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/node_preset.hpp"
#include "whitehole/runtime/player.hpp"

class QAction;
class QActionGroup;
class QDockWidget;
class QLabel;
class QMenu;
class QSlider;
class QToolBar;

namespace whitehole::studio {

class LayoutPanel;
class LivePanel;
class LookPanel;
class NodeEditor;
class NodePalette;
class ParameterPanel;
class PreviewController;
class PreviewWidget;
class TestPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /// `device` darf nullptr sein (Vorschau des Layouts ohne Rendern); `deviceNote` erklärt dann,
    /// warum es kein Gerät gibt. Das Gerät muss das Fenster überleben.
    MainWindow(gpu::IGpuDevice* device, const QString& deviceNote, QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Lädt ein Projekt. Meldet Fehler in der Statuszeile und, wenn `interactive`, in einem Dialog.
    bool openPath(const QString& path, bool interactive = true);
    /// Legt ein neues Projekt an (Vorlage: Matrix mit Farbverlauf). Fragt bei ungespeicherten
    /// Änderungen nach, wenn `interactive`. Lief die Wiedergabe, läuft sie weiter.
    bool newProject(bool interactive = true);
    bool startPlayback(bool interactive = true);
    void stopPlayback();
    /// Ein Frame samt Vorschau (der Timer ruft das auf; Tests und --grab auch).
    void tick();
    void refreshImage();

    /// Rauchtest: stellt jeden aktiven Regler einmal um (wie ein Mensch es täte) und liefert die
    /// Zahl der Änderungen. Für --exercise in der CI.
    int exerciseParameters();
    /// Letzter Fehler des Players als Text, leer wenn alles in Ordnung ist.
    [[nodiscard]] QString problem() const;
    /// Rauchtest des Node-Editors (siehe NodeEditor::exercise) samt Rückgängig/Wiederholen über
    /// alles, was bisher geändert wurde; danach ein paar Frames.
    /// Leer bei Erfolg, sonst die Meldung des gescheiterten Schritts.
    QString exerciseEditor();
    /// Rauchtest: ein Klick auf einen Node muss dessen Gruppe im Parameterfenster hervorheben.
    /// Leer bei Erfolg, sonst die Meldung des gescheiterten Schritts.
    QString exerciseSelection();
    /// Rauchtest: "Neu" legt ein lauffähiges Projekt an. Danach ist das neue Projekt geladen.
    QString exerciseNewProject();
    /// Rauchtest: das Kästchen "Transparent" einer Farbe setzt Alpha auf 0 und wieder auf 1.
    QString exerciseTransparency();
    /// Rauchtest: Master-Regler und Blackout-Schalter erreichen den Player; danach ist alles zurückgesetzt.
    QString exerciseControls();
    /// Rauchtest: Eingang-Schalter startet den Empfang; ein Art-Net-Paket über Loopback kommt im Speicher an.
    QString exerciseDmx();
    /// Rauchtest: eine WAV-Datei als Audioquelle wählen; Frames kommen im Speicher an; ausschalten.
    QString exerciseAudio();
    /// Rauchtest: Node-Palette (Filter, Ablegen, Doppelklick, Rückgängig).
    QString exercisePalette();
    /// Rauchtest: Looks speichern/ordnen/umbenennen, Live-Modus mit Überblendung, GO und Rückkehr.
    QString exerciseLive();
    /// Rauchtest: Umschalter 2D/3D, Orbit mit der Maus, Zoom, feste Ansichten.
    QString exerciseView3d();
    /// Live-Modus ein- und ausschalten (F9). Eintritt scheitert ohne Looks.
    bool enterLive(int first = 0);
    void leaveLive();
    [[nodiscard]] bool liveMode() const noexcept { return liveMode_; }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void buildActions();
    /// Looks und Live-Betrieb (main_window_live.cpp).
    void buildLive();
    /// Vorschau-Ansicht (main_window_view3d.cpp): 2D zum Bearbeiten oder eine der 3D-Ansichten.
    enum class ViewMode { Edit2d, Free3d, Front, Side, Top };
    void buildView3d(QMenu* menu);
    void setViewMode(ViewMode mode);
    void onCameraChanged(const preview::Camera3d& camera);
    void resetLive();
    void hideEditDocks();  // Live-Modus: Editor-Docks verstecken ...
    void showEditDocks();  // ... und danach wieder zeigen
    /// Dock "Nodes": Palette und Node-Editor (main_window_editor.cpp).
    void buildEditorDock();
    /// Füllt das Menü "Beispiele" aus den Ordnern neben dem Programm; leer, wenn es keine gibt.
    void fillExamplesMenu(QMenu* menu);
    void updateActions();
    void updateStatus();
    void reportError(const QString& title, const std::string& message, bool interactive);
    void onGraphEdited(bool structural);
    void onLayoutEdited();
    void updateTitle();
    /// Fragt bei ungespeicherten Änderungen nach. Wahr, wenn weitergemacht werden darf.
    bool confirmDiscard();
    void onUndo();
    void onRedo();
    void afterRestore();
    void updateUndoActions();
    void onNew();
    void onOpen();
    void onReload();
    void onSave();
    void onSaveAs();
    /// Gemeinsamer Rest nach dem Laden oder Anlegen eines Projekts: alle Ansichten neu aufbauen.
    void afterLoad(const QString& message);
    /// Baut das Parameterfenster neu auf und hebt den ausgewählten Node wieder hervor.
    void refreshParameters();
    /// Ein Node im Editor wurde angeklickt: Parameterfenster zeigt seine Parameter.
    void onNodeSelected();
    /// Master-Regler, Blackout-Schalter und die Liste "Zuletzt geöffnet" (main_window_controls.cpp).
    void buildControls(QToolBar* bar);
    void onMaster(int percent);
    void onBlackout(bool on);
    /// Banner oben rechts in der Vorschau: BLACKOUT und/oder TESTMODUS.
    void updateBanner();
    void fillRecentMenu();
    void rememberRecent(const QString& path);
    void forgetRecent(const QString& path);
    /// Ein Testbild wurde ein- oder ausgeschaltet: Banner in der Vorschau und Statuszeile anpassen.
    void onTestPatternChanged();
    /// DMX-Eingang (main_window_dmx.cpp): Menü "Eingang", Empfänger starten/stoppen, Statustext.
    void buildDmxInput();
    void applyDmxInput();
    void syncDmxUniverses();
    [[nodiscard]] QString dmxStatusText() const;
    /// Audio-Eingang (main_window_audio.cpp): Untermenü "Audio" im Menü "Eingang".
    /// `choice`: "off", "default", "device:<Name>" oder "file:<Pfad>". Wahr, wenn eine Quelle läuft.
    void buildAudioInput(QMenu* inputMenu);
    void fillAudioMenu();
    bool applyAudioChoice(const QString& choice);
    [[nodiscard]] QString audioStatusText() const;
    void onTogglePlayback(bool run);
    void onToggleSending(bool send);

    std::unique_ptr<nodes::BuiltinLibrary> library_;
    std::unique_ptr<output::UdpSender> udp_;
    std::unique_ptr<runtime::GatedPacketSink> gate_;
    std::unique_ptr<runtime::Player> player_;
    std::unique_ptr<runtime::PresetStore> presets_;  // Ordner mit Node-Presets im Datenordner des Nutzers
    std::unique_ptr<input::DmxReceiver> dmxReceiver_;  // nach library_: liest/schreibt dessen Speicher
    input::DmxReceiverConfig dmxConfig_;               // Ports; nur der Rauchtest ändert sie
    std::unique_ptr<audio::AudioInput> audioInput_;    // nach library_: schreibt in dessen Audio-Speicher
    QMenu* audioMenu_{nullptr};
    QString audioChoice_{QStringLiteral("off")};       // gewählte Quelle, siehe applyAudioChoice
    QString deviceNote_;

    PreviewWidget* preview_{nullptr};
    PreviewController* previewController_{nullptr};
    ParameterPanel* parameters_{nullptr};
    NodeEditor* editor_{nullptr};
    NodePalette* palette_{nullptr};
    QDockWidget* dock_{nullptr};
    QDockWidget* editorDock_{nullptr};
    LayoutPanel* layoutPanel_{nullptr};
    QDockWidget* layoutDock_{nullptr};
    TestPanel* testPanel_{nullptr};
    QDockWidget* testDock_{nullptr};
    LookPanel* lookPanel_{nullptr};
    QDockWidget* lookDock_{nullptr};
    LivePanel* livePanel_{nullptr};
    QDockWidget* liveDock_{nullptr};
    QAction* liveAction_{nullptr};
    QAction* saveLookAction_{nullptr};
    QToolBar* fileBar_{nullptr};
    QKeySequence runShortcut_;  // Leertaste: im Live-Modus gehört sie dem GO-Knopf
    std::vector<QDockWidget*> hiddenByLive_;  // Docks, die der Live-Modus versteckt hat (kommen danach zurück)
    bool liveMode_{false};
    ViewMode viewMode_{ViewMode::Edit2d};
    QActionGroup* viewGroup_{nullptr};
    std::array<QAction*, 5> viewActions_{};
    preview::Camera3d freeCamera_;  // zuletzt frei gedrehte Kamera: "3D" kehrt dorthin zurück
    QLabel* statusLabel_{nullptr};
    QTimer timer_;
    QTimer statusTimer_;

    QAction* newAction_{nullptr};
    QAction* openAction_{nullptr};
    QAction* saveAction_{nullptr};
    QAction* reloadAction_{nullptr};
    QAction* saveAsAction_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
    QAction* runAction_{nullptr};
    QAction* sendAction_{nullptr};
    QAction* quitAction_{nullptr};
    QAction* blackoutAction_{nullptr};
    QSlider* masterSlider_{nullptr};
    QLabel* masterLabel_{nullptr};
    QMenu* recentMenu_{nullptr};
    QAction* artnetInAction_{nullptr};
    QAction* sacnInAction_{nullptr};
};

}  // namespace whitehole::studio
