// dmx_monitor_window.hpp: das Fenster "DMX-Monitor" (Eingang und Ausgang als Reiter).
//
// Warum es das gibt: Eingangs- und Ausgangs-Monitor lagen bisher als zwei einzelne Docks neben Editor und
// Vorschau - dort nahmen sie Platz weg, den man beim Bauen von Effekten eigentlich für den Node-Editor und
// die Parameter braucht, genau wie Layout/Ausgabe/Test vor "Stage Config" (siehe stage_setup_window.hpp,
// dasselbe Muster: eigenes, nicht-modales Fenster statt Dock, öffnet sich bei Bedarf, lässt sich auf einen
// zweiten Bildschirm ziehen, bleibt bedienbar während die Vorschau im Hauptfenster sichtbar ist). Anders
// als Stage Config schließt der Live-Modus dieses Fenster nicht - ein reiner Monitor stört die Bühne nicht,
// im Gegensatz zu Layout/Ausgabe, die das Projekt verändern könnten.
#pragma once

#include <QWidget>

class QTabWidget;

namespace whitehole::studio {

class DmxMonitorPanel;
class DmxOutputMonitorPanel;

class DmxMonitorWindow : public QWidget {
    Q_OBJECT

public:
    enum class Page { Input, Output };

    DmxMonitorWindow(DmxMonitorPanel* input, DmxOutputMonitorPanel* output, QWidget* parent);

    /// Öffnet das Fenster (beim ersten Mal an der gemerkten Stelle) und holt es nach vorn.
    void open(Page which);
    [[nodiscard]] Page page() const;
    [[nodiscard]] int pageCount() const;

protected:
    void hideEvent(QHideEvent* event) override;

private:
    QTabWidget* tabs_;
    bool restored_{false};
};

}  // namespace whitehole::studio
