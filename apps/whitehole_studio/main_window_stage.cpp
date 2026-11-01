// main_window_stage.cpp: das Menü und das Fenster "Stage Config" im Hauptfenster (Kürzel, Live-Modus, Rauchtest).
//
// Warum eine eigene Datei: main_window.cpp ist an der 400-Zeilen-Grenze. Hier steht, wie das Fenster
// geöffnet wird (Menü Stage Config, Layout und Patch = F4; Testmodus ist die zweite Seite) und dass der Live-Modus es
// schließt und danach wieder öffnet, falls es offen war.
#include <QAction>
#include <QKeySequence>
#include <QMenu>

#include "layout_panel.hpp"
#include "look_panel.hpp"
#include "main_window.hpp"
#include "stage_setup_window.hpp"

namespace whitehole::studio {

void MainWindow::buildStageConfig(QMenu* menu) {
    stageMenu_ = menu;
    stageSetup_ = new StageSetupWindow{layoutPanel_, testPanel_, this};

    auto* addFixtureAction = new QAction{tr("Fixture &hinzufügen …"), this};
    addFixtureAction->setToolTip(tr("Matrix, Strip oder 3D-Gitter mit Größe x y z, Lage, Verkabelung und Patch anlegen"));
    connect(addFixtureAction, &QAction::triggered, this, [this]() { layoutPanel_->addFromDialog(); });

    auto* importPointsAction = new QAction{tr("Punktwolke &importieren …"), this};
    importPointsAction->setToolTip(tr("LED-Positionen aus einer CSV- oder JSON-Datei laden"));
    connect(importPointsAction, &QAction::triggered, this, [this]() { layoutPanel_->importPoints(); });

    stageSetupAction_ = new QAction{tr("&Layout und Patch …"), this};
    stageSetupAction_->setShortcut(QKeySequence{Qt::Key_F4});
    stageSetupAction_->setToolTip(tr("Fixtures anordnen, Universen und Ziele einstellen (eigenes Fenster)"));
    connect(stageSetupAction_, &QAction::triggered, this, [this]() { stageSetup_->open(StageSetupWindow::Page::Layout); });

    auto* testModeAction = new QAction{tr("&Testmodus …"), this};
    testModeAction->setToolTip(tr("Testbilder auf die LEDs schicken, um Verkabelung und Patch zu prüfen"));
    connect(testModeAction, &QAction::triggered, this, [this]() { stageSetup_->open(StageSetupWindow::Page::Test); });

    menu->addAction(addFixtureAction);
    menu->addAction(importPointsAction);
    menu->addSeparator();
    menu->addAction(stageSetupAction_);
    menu->addAction(testModeAction);
}

QString MainWindow::exerciseStageSetup() {
    if (stageSetup_ == nullptr || stageSetup_->pageCount() != 2) {
        return QStringLiteral("das Fenster Stage Config hat nicht die Seiten Layout und Testmodus");
    }
    if (stageSetup_->isVisible()) {
        return QStringLiteral("Stage Config ist schon offen");
    }
    stageSetupAction_->trigger();
    if (!stageSetup_->isVisible() || stageSetup_->page() != StageSetupWindow::Page::Layout || !stageSetup_->isWindow()) {
        return QStringLiteral("Stage Config öffnet sich nicht als eigenes Fenster");
    }
    stageSetup_->open(StageSetupWindow::Page::Test);
    if (stageSetup_->page() != StageSetupWindow::Page::Test) {
        return QStringLiteral("Stage Config zeigt die Seite Testmodus nicht");
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
