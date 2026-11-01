// stage_setup_window.cpp: Umsetzung zu stage_setup_window.hpp. Das Fenster "Stage Config" mit den Seiten Layout, Testmodus und Ansicht.
#include "stage_setup_window.hpp"

#include <QHideEvent>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>

#include "layout_panel.hpp"
#include "stage_view.hpp"
#include "test_panel.hpp"

namespace whitehole::studio {

namespace {

QSettings settings() { return QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}; }

}  // namespace

StageSetupWindow::StageSetupWindow(LayoutPanel* layout, TestPanel* test, StageView* view, QWidget* parent)
    : QWidget{parent, Qt::Window}, tabs_{new QTabWidget{this}} {
    setWindowTitle(tr("Stage Config"));
    auto* box = new QVBoxLayout{this};
    box->setContentsMargins(0, 0, 0, 0);
    box->addWidget(tabs_);
    tabs_->addTab(layout, tr("Layout"));
    tabs_->addTab(test, tr("Testmodus"));
    tabs_->addTab(view, tr("Ansicht"));
    resize(560, 760);
}

void StageSetupWindow::open(Page which) {
    if (!restored_) {
        restored_ = true;
        restoreGeometry(settings().value(QStringLiteral("stageSetup/geometry")).toByteArray());
    }
    tabs_->setCurrentIndex(which == Page::View ? 2 : (which == Page::Test ? 1 : 0));
    show();
    raise();
    activateWindow();
}

StageSetupWindow::Page StageSetupWindow::page() const {
    switch (tabs_->currentIndex()) {
        case 1: return Page::Test;
        case 2: return Page::View;
        default: return Page::Layout;
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
