// stage_setup_window.cpp: Umsetzung zu stage_setup_window.hpp. Das Fenster "Stage Config" mit den Seiten Layout, Testmodus und Ansicht.
#include "stage_setup_window.hpp"

#include <QHideEvent>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>

#include "fixture_library_page.hpp"
#include "layout_panel.hpp"
#include "output_page.hpp"
#include "stage_view.hpp"
#include "test_panel.hpp"

namespace whitehole::studio {

namespace {

QSettings settings() { return QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}; }

}  // namespace

StageSetupWindow::StageSetupWindow(FixtureLibraryPage* fixtures, LayoutPanel* layout, OutputPage* output, TestPanel* test,
                                   StageView* view, QWidget* parent)
    : QWidget{parent, Qt::Window}, tabs_{new QTabWidget{this}} {
    setWindowTitle(tr("Stage Config"));
    auto* box = new QVBoxLayout{this};
    box->setContentsMargins(0, 0, 0, 0);
    box->addWidget(tabs_);
    tabs_->addTab(fixtures, tr("Fixtures"));
    auto* split = new QSplitter{Qt::Horizontal};  // links Liste und Formular, rechts die Bühne im Raum
    split->addWidget(layout);
    split->addWidget(view);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({400, 600});
    tabs_->addTab(split, tr("Layout"));
    tabs_->addTab(output, tr("Ausgabe"));
    tabs_->addTab(test, tr("Test"));
    resize(1000, 760);
}

void StageSetupWindow::open(Page which) {
    if (!restored_) {
        restored_ = true;
        restoreGeometry(settings().value(QStringLiteral("stageSetup/geometry")).toByteArray());
    }
    tabs_->setCurrentIndex(static_cast<int>(which));
    show();
    raise();
    activateWindow();
}

StageSetupWindow::Page StageSetupWindow::page() const {
    switch (tabs_->currentIndex()) {
        case 1: return Page::Layout;
        case 2: return Page::Output;
        case 3: return Page::Test;
        default: return Page::Fixtures;
    }
}

int StageSetupWindow::pageCount() const { return tabs_->count(); }

void StageSetupWindow::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (restored_ && !event->spontaneous()) {
        settings().setValue(QStringLiteral("stageSetup/geometry"), saveGeometry());
    }
}

}  // namespace whitehole::studio
