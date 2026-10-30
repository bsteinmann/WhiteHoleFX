// main_window.hpp: das Hauptfenster.
//
// Aufgaben: Projekt öffnen (Menü, Drag & Drop, Kommandozeile), Start/Stopp, Netzwerkausgabe ein-
// und ausschalten, Vorschau im Takt der Projekt-Bildrate aktualisieren, Status anzeigen.
//
// Sicherheit: Die Netzwerkausgabe ist nach dem Start AUS. Ein Projekt läuft und zeigt sich, ohne
// dass ein Paket das Haus verlässt; erst "Ausgabe senden" schaltet das ein.
#pragma once

#include <QMainWindow>
#include <QTimer>

#include <memory>

#include "whitehole/nodes/builtin_library.hpp"
#include "whitehole/output/udp_sender.hpp"
#include "whitehole/runtime/gated_sink.hpp"
#include "whitehole/runtime/player.hpp"

class QAction;
class QDockWidget;
class QLabel;
class QMenu;

namespace whitehole::studio {

class LayoutPanel;
class NodeEditor;
class ParameterPanel;
class PreviewWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /// `device` darf nullptr sein (Vorschau des Layouts ohne Rendern); `deviceNote` erklärt dann,
    /// warum es kein Gerät gibt. Das Gerät muss das Fenster überleben.
    MainWindow(gpu::IGpuDevice* device, const QString& deviceNote, QWidget* parent = nullptr);
    ~MainWindow() override;

    /// Lädt ein Projekt. Meldet Fehler in der Statuszeile und, wenn `interactive`, in einem Dialog.
    bool openPath(const QString& path, bool interactive = true);
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

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void buildActions();
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
    void onOpen();
    void onReload();
    void onSaveAs();
    void onTogglePlayback(bool run);
    void onToggleSending(bool send);

    std::unique_ptr<nodes::BuiltinLibrary> library_;
    std::unique_ptr<output::UdpSender> udp_;
    std::unique_ptr<runtime::GatedPacketSink> gate_;
    std::unique_ptr<runtime::Player> player_;
    QString deviceNote_;

    PreviewWidget* preview_{nullptr};
    ParameterPanel* parameters_{nullptr};
    NodeEditor* editor_{nullptr};
    QDockWidget* dock_{nullptr};
    QDockWidget* editorDock_{nullptr};
    LayoutPanel* layoutPanel_{nullptr};
    QDockWidget* layoutDock_{nullptr};
    QLabel* statusLabel_{nullptr};
    QTimer timer_;
    QTimer statusTimer_;

    QAction* openAction_{nullptr};
    QAction* reloadAction_{nullptr};
    QAction* saveAsAction_{nullptr};
    QAction* undoAction_{nullptr};
    QAction* redoAction_{nullptr};
    QAction* runAction_{nullptr};
    QAction* sendAction_{nullptr};
    QAction* quitAction_{nullptr};
};

}  // namespace whitehole::studio
