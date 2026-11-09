// main_window_stage.cpp: das Menü und das Fenster "Stage Config" im Hauptfenster (Kürzel, Live-Modus, Rauchtest).
//
// Warum eine eigene Datei: main_window.cpp ist an der 400-Zeilen-Grenze. Hier steht, wie das Fenster
// geöffnet wird (Menü Stage Config, Layout = F4; Fixtures, Ausgabe und Test sind weitere Seiten) und dass der Live-Modus es
// schließt und danach wieder öffnet, falls es offen war.
#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QStatusBar>

#include "fixture_library_page.hpp"
#include "layout_panel.hpp"
#include "look_panel.hpp"
#include "output_page.hpp"
#include "main_window.hpp"
#include "stage_setup_window.hpp"
#include "stage_view.hpp"
#include "test_panel.hpp"

namespace whitehole::studio {

void MainWindow::buildStageConfig(QMenu* menu) {
    stageMenu_ = menu;
    stageView_ = new StageView{*player_, false};
    testView_ = new StageView{*player_, false};
    testView_->setTestPreview(true);
    connect(stageView_, &StageView::fixturesMoved, this, &MainWindow::onLayoutEdited);
    connect(stageView_, &StageView::failed, this, [this](const QString& message) { statusBar()->showMessage(message, 8000); });
    connect(testPanel_, &TestPanel::patternChanged, testView_, &StageView::testPatternChanged);
    fixturePage_ = new FixtureLibraryPage{*player_};
    connect(fixturePage_, &FixtureLibraryPage::edited, this, &MainWindow::onLayoutEdited);
    connect(fixturePage_, &FixtureLibraryPage::notice, this, [this](const QString& message) { statusBar()->showMessage(message, 8000); });
    connect(fixturePage_, &FixtureLibraryPage::projectNeeded, this, [this]() { newProject(false); });
    connect(fixturePage_, &FixtureLibraryPage::failed, this,
            [this](const QString& message) { statusBar()->showMessage(message, 8000); });
    outputPage_ = new OutputPage{*player_};
    connect(outputPage_, &OutputPage::edited, this, &MainWindow::onLayoutEdited);
    connect(outputPage_, &OutputPage::failed, this,
            [this](const QString& message) { statusBar()->showMessage(message, 8000); });
    connect(outputPage_, &OutputPage::notice, this,
            [this](const QString& message) { statusBar()->showMessage(message, 12000); });
    stageSetup_ = new StageSetupWindow{fixturePage_, layoutPanel_, outputPage_, testPanel_, stageView_, testView_, this};
    connect(layoutPanel_, &LayoutPanel::selectionChanged, this, &MainWindow::refreshStageViews);

    auto* addFixtureAction = new QAction{tr("Fixture &hinzufügen …"), this};
    addFixtureAction->setToolTip(tr("Matrix, Strip oder 3D-Gitter mit Größe x y z, Lage, Verkabelung und Patch anlegen"));
    connect(addFixtureAction, &QAction::triggered, this, [this]() { layoutPanel_->addFromDialog(); });

    stageSetupAction_ = new QAction{tr("&Layout …"), this};
    stageSetupAction_->setShortcut(QKeySequence{Qt::Key_F4});
    stageSetupAction_->setToolTip(tr("Fixtures auf der Bühne verteilen und drehen (eigenes Fenster)"));

    auto* outputAction = new QAction{tr("&Ausgabe …"), this};
    outputAction->setToolTip(tr("Ziele (Controller) und Patch: welches Fixture auf welches Universe sendet"));
    connect(outputAction, &QAction::triggered, this, [this]() { stageSetup_->open(StageSetupWindow::Page::Output); });
    connect(stageSetupAction_, &QAction::triggered, this, [this]() { stageSetup_->open(StageSetupWindow::Page::Layout); });

    auto* testModeAction = new QAction{tr("&Testmodus …"), this};
    testModeAction->setToolTip(tr("Testbilder auf die LEDs schicken, um Verkabelung und Patch zu prüfen"));
    connect(testModeAction, &QAction::triggered, this, [this]() { stageSetup_->open(StageSetupWindow::Page::Test); });

    auto* fixturesAction = new QAction{tr("&Fixtures …"), this};
    fixturesAction->setToolTip(tr("Bibliothek der Bauformen: Matrix, Kreis, Strip, Säule mit Vorschau und Test"));
    connect(fixturesAction, &QAction::triggered, this, [this]() { stageSetup_->open(StageSetupWindow::Page::Fixtures); });

    menu->addAction(fixturesAction);
    menu->addAction(addFixtureAction);
    menu->addSeparator();
    menu->addAction(stageSetupAction_);
    menu->addAction(outputAction);
    menu->addAction(testModeAction);
}

void MainWindow::refreshStageViews() {
    if (stageView_ == nullptr) {
        return;
    }
    fixturePage_->refresh();
    outputPage_->refresh();
    stageView_->setSelection(layoutPanel_->selectedIds());
    stageView_->refresh();
    testView_->refresh();
    layoutPanel_->refreshView();
}

QString MainWindow::exerciseStageSetup() {
    if (stageSetup_ == nullptr || stageSetup_->pageCount() != 4) {
        return QStringLiteral("das Fenster Stage Config hat nicht die Seiten Fixtures, Layout, Ausgabe und Test");
    }
    if (const QString note = stageView_->exercise(); !note.isEmpty()) {
        return note;
    }
    if (const QString note = fixturePage_->exercise(); !note.isEmpty()) {
        return note;
    }
    if (stageSetup_->isVisible()) {
        return QStringLiteral("Stage Config ist schon offen");
    }
    stageSetup_->open(StageSetupWindow::Page::Fixtures);
    if (!stageSetup_->isVisible() || stageSetup_->page() != StageSetupWindow::Page::Fixtures) {
        return QStringLiteral("Stage Config zeigt die Seite Fixtures nicht");
    }
    stageSetup_->hide();
    stageSetupAction_->trigger();
    if (!stageSetup_->isVisible() || stageSetup_->page() != StageSetupWindow::Page::Layout || !stageSetup_->isWindow()) {
        return QStringLiteral("Stage Config öffnet sich nicht als eigenes Fenster");
    }
    stageSetup_->open(StageSetupWindow::Page::Test);
    if (stageSetup_->page() != StageSetupWindow::Page::Test) {
        return QStringLiteral("Stage Config zeigt die Seite Test nicht");
    }
    // Testbild über die ganze Bühne: Die Testansicht zeigt dessen Farben, ohne Testbild die Anlage.
    {
        runtime::TestPattern pattern;
        pattern.kind = runtime::TestPatternKind::Chase;
        if (!player_->setTestPattern(pattern).ok()) {
            return QStringLiteral("Test: Testbild lässt sich nicht einschalten");
        }
        testView_->testPatternChanged();
        const bool shown = testView_->showsTestPattern();
        player_->clearTestPattern();
        testView_->testPatternChanged();
        const bool gone = !testView_->showsTestPattern();
        testPanel_->reset();
        if (!shown || !gone) {
            return QStringLiteral("Test: die Bühne im Reiter Test zeigt das Testbild nicht (oder zeigt es danach noch)");
        }
    }
    stageSetup_->open(StageSetupWindow::Page::Output);
    if (stageSetup_->page() != StageSetupWindow::Page::Output) {
        return QStringLiteral("Stage Config zeigt die Seite Ausgabe nicht");
    }
    if (const QString note = outputPage_->exercise(); !note.isEmpty()) {
        return note;
    }
    // Der Live-Modus schließt das Fenster und stellt es danach wieder her. Er braucht einen Look.
    const bool addedLook = lookPanel_->lookCount() == 0;
    if (addedLook && !lookPanel_->saveLookAs(QStringLiteral("StageTest"))) {
        return QStringLiteral("für den Live-Test lässt sich kein Look speichern");
    }
    if (!enterLive(0)) {
        return QStringLiteral("der Live-Modus startet nicht (Stage-Test)");
    }
    const bool hiddenInLive = !stageSetup_->isVisible();
    const bool menuLockedInLive = !stageMenu_->isEnabled();  // auf der Bühne wird nichts umgebaut
    leaveLive();
    const bool backAfterLive = stageSetup_->isVisible();
    stageSetup_->hide();
    if (addedLook) {
        lookPanel_->removeLook(0);
    }
    if (!menuLockedInLive || !stageMenu_->isEnabled()) {
        return QStringLiteral("das Menü Stage Config ist im Live-Modus nicht gesperrt oder danach nicht wieder frei");
    }
    if (!hiddenInLive) {
        return QStringLiteral("der Live-Modus lässt Stage Config offen");
    }
    if (!backAfterLive) {
        return QStringLiteral("Stage Config kommt nach dem Live-Modus nicht zurück");
    }
    return {};
}

}  // namespace whitehole::studio
