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
class QLabel;

namespace whitehole::studio {

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

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void buildActions();
    void updateActions();
    void updateStatus();
    void reportError(const QString& title, const std::string& message, bool interactive);
    void onOpen();
    void onReload();
    void onTogglePlayback(bool run);
    void onToggleSending(bool send);

    std::unique_ptr<nodes::BuiltinLibrary> library_;
    std::unique_ptr<output::UdpSender> udp_;
    std::unique_ptr<runtime::GatedPacketSink> gate_;
    std::unique_ptr<runtime::Player> player_;
    QString deviceNote_;

    PreviewWidget* preview_{nullptr};
    QLabel* statusLabel_{nullptr};
    QTimer timer_;
    QTimer statusTimer_;

    QAction* openAction_{nullptr};
    QAction* reloadAction_{nullptr};
    QAction* runAction_{nullptr};
    QAction* sendAction_{nullptr};
    QAction* quitAction_{nullptr};
};

}  // namespace whitehole::studio
