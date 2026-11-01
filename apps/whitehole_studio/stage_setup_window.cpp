// stage_setup_window.cpp: Umsetzung zu stage_setup_window.hpp. Das Fenster "Stage Setup" mit den Seiten Layout und Testmodus.
#include "stage_setup_window.hpp"

#include <QHideEvent>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>

#include "layout_panel.hpp"
#include "test_panel.hpp"

namespace whitehole::studio {

namespace {

QSettings settings() { return QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}; }

}  // namespace

StageSetupWindow::StageSetupWindow(LayoutPanel* layout, TestPanel* test, QWidget* parent)
    : QWidget{parent, Qt::Window}, tabs_{new QTabWidget{this}} {
    setWindowTitle(tr("Stage Setup"));
    auto* box = new QVBoxLayout{this};
    box->setContentsMargins(0, 0, 0, 0);
    box->addWidget(tabs_);
    tabs_->addTab(layout, tr("Layout"));
    tabs_->addTab(test, tr("Testmodus"));
    resize(560, 760);
}

void StageSetupWindow::open(Page which) {
    if (!restored_) {
        restored_ = true;
        restoreGeometry(settings().value(QStringLiteral("stageSetup/geometry")).toByteArray());
    }
    tabs_->setCurrentIndex(which == Page::Test ? 1 : 0);
    show();
    raise();
    activateWindow();
}

StageSetupWindow::Page StageSetupWindow::page() const {
    return tabs_->currentIndex() == 1 ? Page::Test : Page::Layout;
}

int StageSetupWindow::pageCount() const { return tabs_->count(); }

void StageSetupWindow::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (restored_ && !event->spontaneous()) {
        settings().setValue(QStringLiteral("stageSetup/geometry"), saveGeometry());
    }
}

}  // namespace whitehole::studio
