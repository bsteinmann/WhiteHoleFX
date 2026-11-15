// dmx_monitor_window.cpp: Umsetzung zu dmx_monitor_window.hpp. Das Fenster "DMX-Monitor" mit den Reitern
// Eingang und Ausgang, nach demselben Muster wie stage_setup_window.cpp.
#include "dmx_monitor_window.hpp"

#include <QHideEvent>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>

#include "dmx_monitor_panel.hpp"
#include "dmx_output_monitor_panel.hpp"

namespace whitehole::studio {

namespace {

QSettings settings() { return QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}; }

}  // namespace

DmxMonitorWindow::DmxMonitorWindow(DmxMonitorPanel* input, DmxOutputMonitorPanel* output, QWidget* parent)
    : QWidget{parent, Qt::Window}, tabs_{new QTabWidget{this}} {
    setWindowTitle(tr("DMX-Monitor"));
    auto* box = new QVBoxLayout{this};
    box->setContentsMargins(0, 0, 0, 0);
    box->addWidget(tabs_);
    tabs_->addTab(input, tr("Eingang"));
    tabs_->addTab(output, tr("Ausgang"));
    resize(900, 650);
}

void DmxMonitorWindow::open(Page which) {
    if (!restored_) {
        restored_ = true;
        restoreGeometry(settings().value(QStringLiteral("dmxMonitor/geometry")).toByteArray());
    }
    tabs_->setCurrentIndex(static_cast<int>(which));
    show();
    raise();
    activateWindow();
}

DmxMonitorWindow::Page DmxMonitorWindow::page() const {
    return tabs_->currentIndex() == 1 ? Page::Output : Page::Input;
}

int DmxMonitorWindow::pageCount() const { return tabs_->count(); }

void DmxMonitorWindow::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (restored_ && !event->spontaneous()) {
        settings().setValue(QStringLiteral("dmxMonitor/geometry"), saveGeometry());
    }
}

}  // namespace whitehole::studio
