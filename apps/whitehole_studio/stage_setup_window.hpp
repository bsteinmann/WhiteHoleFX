// stage_setup_window.hpp: das Fenster "Stage Config" (Layout, Ausgabe-Ziele, Testmodus).
//
// Warum es das gibt: Fixtures anordnen, Universen vergeben und die Verkabelung prüfen macht man beim
// Einrichten der Installation, nicht während man Effekte baut. Als Dock nahm das Layout den Platz neben
// Parameter und Vorschau ein. Als eigenes Fenster öffnet es sich bei Bedarf (F4), kann auf einen
// zweiten Bildschirm gezogen werden und lässt dem Hauptfenster seinen Platz. Es bleibt bedienbar,
// während die Vorschau im Hauptfenster sichtbar ist (kein modaler Dialog).
//
// Seiten: Fixtures (anlegen), Layout (Patchen, Verteilen und Drehen, daneben die Bühnenansicht, siehe stage_view.hpp),
// Ausgabe (Ziele und Patch) und Test (Testbilder für die ganze Bühne, daneben die Bühne mit den Farben des Testbilds).
// Das Fenster besitzt die Panels nur als Tabs; Verbindungen, Rückgängig und Speichern laufen
// unverändert über das Hauptfenster.
//
// openOutputFixture/openOutputPool springen gezielt auf die Seite Ausgabe und wählen dort die passende
// Patch-Zeile - der Rechtsklick "bearbeiten" im DMX-Ausgangs-Monitor (Schritt 3 des DMX-Patch/Monitor-
// Gitters, siehe CLAUDE.md).
#pragma once

#include <QWidget>

#include <cstdint>

#include "whitehole/mapping/layout.hpp"

class QTabWidget;

namespace whitehole::studio {

class FixtureLibraryPage;
class LayoutPanel;
class OutputPage;
class TestPanel;
class StageView;

class StageSetupWindow : public QWidget {
    Q_OBJECT

public:
    enum class Page { Fixtures, Layout, Output, Test };

    StageSetupWindow(FixtureLibraryPage* fixtures, LayoutPanel* layout, OutputPage* output, TestPanel* test, StageView* view,
                     StageView* testView, QWidget* parent);

    /// Öffnet das Fenster (beim ersten Mal an der gemerkten Stelle) und holt es nach vorn.
    void open(Page which);
    [[nodiscard]] Page page() const;
    [[nodiscard]] int pageCount() const;

    /// Öffnet die Seite Ausgabe und wählt die Patch-Zeile dieses Fixtures - der Sprung vom Rechtsklick-
    /// Menü des DMX-Ausgangs-Monitors (Schritt 3 des DMX-Patch/Monitor-Gitters, siehe CLAUDE.md).
    void openOutputFixture(mapping::FixtureId id);
    /// Dasselbe für einen DMX-Kanal-Pool-Patch, über seinen ersten Pool-Kanal.
    void openOutputPool(std::uint16_t poolChannel);

protected:
    void hideEvent(QHideEvent* event) override;

private:
    QTabWidget* tabs_;
    OutputPage* output_;  // für openOutputFixture/openOutputPool; die Seite selbst gehört dem Tab-Widget
    bool restored_{false};
};

}  // namespace whitehole::studio
