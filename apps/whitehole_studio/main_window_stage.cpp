// main_window_stage.cpp: das Fenster "Stage Setup" im Hauptfenster (Menü, Kürzel, Live-Modus, Rauchtest).
//
// Warum eine eigene Datei: main_window.cpp ist an der 400-Zeilen-Grenze. Hier steht, wie das Fenster
// geöffnet wird (Ansicht > Stage Setup, F4; Testmodus ist die zweite Seite) und dass der Live-Modus es
// schließt und danach wieder öffnet, falls es offen war.
#include <QAction>
#include <QKeySequence>
#include <QMenu>

#include "look_panel.hpp"
#include "main_window.hpp"
#include "stage_setup_window.hpp"

namespace whitehole::studio {

void MainWindow::buildStageSetup(QMenu* viewMenu) {
    stageSetup_ = new StageSetupWindow{layoutPanel_, testPanel_, this};
    stageSetupAction_ = new QAction{tr("&Stage Setup …"), this};
    stageSetupAction_->setShortcut(QKeySequence{Qt::Key_F4});
    stageSetupAction_->setToolTip(tr("Fixtures anordnen, Universen und Ziele einstellen, Testbilder (eigenes Fenster)"));
    connect(stageSetupAction_, &QAction::triggered, this, [this]() { stageSetup_->open(StageSetupWindow::Page::Layout); });
    viewMenu->addAction(stageSetupAction_);
}

QString MainWindow::exerciseStageSetup() {
    if (stageSetup_ == nullptr || stageSetup_->pageCount() != 2) {
        return QStringLiteral("das Fenster Stage Setup hat nicht die Seiten Layout und Testmodus");
    }
    if (stageSetup_->isVisible()) {
        return QStringLiteral("Stage Setup ist schon offen");
    }
    stageSetupAction_->trigger();
    if (!stageSetup_->isVisible() || stageSetup_->page() != StageSetupWindow::Page::Layout || !stageSetup_->isWindow()) {
        return QStringLiteral("Stage Setup öffnet sich nicht als eigenes Fenster");
    }
    stageSetup_->open(StageSetupWindow::Page::Test);
    if (stageSetup_->page() != StageSetupWindow::Page::Test) {
        return QStringLiteral("Stage Setup zeigt die Seite Testmodus nicht");
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
    leaveLive();
    const bool backAfterLive = stageSetup_->isVisible();
    stageSetup_->hide();
    if (addedLook) {
        lookPanel_->removeLook(0);
    }
    if (!hiddenInLive) {
        return QStringLiteral("der Live-Modus lässt Stage Setup offen");
    }
    if (!backAfterLive) {
        return QStringLiteral("Stage Setup kommt nach dem Live-Modus nicht zurück");
    }
    return {};
}

}  // namespace whitehole::studio
