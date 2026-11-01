// stage_setup_window.hpp: das Fenster "Stage Setup" (Layout, Ausgabe-Ziele, Testmodus).
//
// Warum es das gibt: Fixtures anordnen, Universen vergeben und die Verkabelung prüfen macht man beim
// Einrichten der Installation, nicht während man Effekte baut. Als Dock nahm das Layout den Platz neben
// Parameter und Vorschau ein. Als eigenes Fenster öffnet es sich bei Bedarf (F4), kann auf einen
// zweiten Bildschirm gezogen werden und lässt dem Hauptfenster seinen Platz. Es bleibt bedienbar,
// während die Vorschau im Hauptfenster sichtbar ist (kein modaler Dialog).
//
// Das Fenster besitzt die Panels nur als Tabs; Verbindungen, Rückgängig und Speichern laufen
// unverändert über das Hauptfenster.
#pragma once

#include <QWidget>

class QTabWidget;

namespace whitehole::studio {

class LayoutPanel;
class TestPanel;

class StageSetupWindow : public QWidget {
    Q_OBJECT

public:
    enum class Page { Layout, Test };

    StageSetupWindow(LayoutPanel* layout, TestPanel* test, QWidget* parent);

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
