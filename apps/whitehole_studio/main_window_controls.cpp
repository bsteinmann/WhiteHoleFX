// main_window_controls.cpp: Master, Blackout, Banner und die Liste "Zuletzt geöffnet".
//
// Warum eine eigene Datei: Das sind Bedienelemente für den Betrieb, nicht für das Projekt. Master
// und Blackout liegen im Player (nicht im Projekt, nicht im Undo); hier stehen nur Regler, Schalter
// und Anzeige. Die Listenlogik der zuletzt geöffneten Dateien steckt in runtime/recent_files.
#include <QAction>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QSettings>
#include <QSlider>
#include <QStatusBar>
#include <QToolBar>
#include <QWidget>

#include <cmath>
#include <string>
#include <vector>

#include "main_window.hpp"
#include "preview_widget.hpp"
#include "test_panel.hpp"
#include "whitehole/runtime/recent_files.hpp"

namespace whitehole::studio {

namespace {

QSettings settings() { return QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}; }

}  // namespace

void MainWindow::buildControls(QToolBar* bar) {
    blackoutAction_ = new QAction{tr("&Blackout"), this};
    blackoutAction_->setCheckable(true);
    blackoutAction_->setShortcut(QKeySequence{Qt::CTRL | Qt::Key_B});
    blackoutAction_->setToolTip(tr("Alle LEDs aus; die Pakete laufen weiter (Strg+B)"));
    connect(blackoutAction_, &QAction::toggled, this, &MainWindow::onBlackout);

    masterSlider_ = new QSlider{Qt::Horizontal, this};
    masterSlider_->setRange(0, 100);
    masterSlider_->setValue(100);
    masterSlider_->setFixedWidth(140);
    masterSlider_->setToolTip(tr("Gesamthelligkeit (wirkt auf Vorschau und Ausgabe, nicht im Projekt gespeichert)"));
    masterLabel_ = new QLabel{tr("100 %"), this};
    masterLabel_->setMinimumWidth(40);
    connect(masterSlider_, &QSlider::valueChanged, this, &MainWindow::onMaster);

    auto* spacer = new QWidget{this};
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bar->addWidget(spacer);  // schiebt Master und Blackout an den rechten Rand
    bar->addWidget(new QLabel{tr(" Master "), this});
    bar->addWidget(masterSlider_);
    bar->addWidget(masterLabel_);
    bar->addAction(blackoutAction_);
}

void MainWindow::onMaster(int percent) {
    const float level = player_->setMasterBrightness(static_cast<float>(percent) / 100.0F);
    masterLabel_->setText(tr("%1 %").arg(std::lround(level * 100.0F)));
    updateStatus();
}

void MainWindow::onBlackout(bool on) {
    player_->setBlackout(on);
    updateBanner();
    updateStatus();
}

void MainWindow::updateBanner() {
    QStringList parts;
    if (player_->blackout()) {
        parts << tr("BLACKOUT");
    }
    const QString test = testPanel_->description();
    if (!test.isEmpty()) {
        parts << tr("TESTMODUS: %1").arg(test);
    }
    preview_->setBanner(parts.join(QStringLiteral("  ·  ")));
}

QString MainWindow::exerciseControls() {
    masterSlider_->setValue(40);
    if (std::abs(player_->masterBrightness() - 0.4F) > 1e-4F) {
        return QStringLiteral("Master: der Regler erreicht den Player nicht");
    }
    blackoutAction_->setChecked(true);
    if (!player_->blackout()) {
        return QStringLiteral("Blackout: der Schalter erreicht den Player nicht");
    }
    for (int i = 0; i < 3; ++i) {
        tick();  // mit Master und Blackout läuft die Ausgabe weiter
    }
    blackoutAction_->setChecked(false);
    masterSlider_->setValue(100);
    if (player_->blackout() || std::abs(player_->masterBrightness() - 1.0F) > 1e-4F) {
        return QStringLiteral("Master/Blackout: Zurücksetzen gescheitert");
    }
    return {};
}

// --- Zuletzt geöffnet ------------------------------------------------------------------------------------

void MainWindow::fillRecentMenu() {
    recentMenu_->clear();
    const QStringList files = settings().value(QStringLiteral("recentFiles")).toStringList();
    for (const QString& path : files) {
        QString label = QFileInfo{path}.fileName();
        label.replace(QLatin1Char('&'), QStringLiteral("&&"));  // sonst wäre "&" ein Tastenkürzel
        QAction* action = recentMenu_->addAction(label);
        action->setToolTip(path);
        action->setStatusTip(path);
        connect(action, &QAction::triggered, this, [this, path]() { openPath(path); });
    }
    if (files.isEmpty()) {
        recentMenu_->addAction(tr("(leer)"))->setEnabled(false);
        return;
    }
    recentMenu_->addSeparator();
    connect(recentMenu_->addAction(tr("Liste leeren")), &QAction::triggered, this, []() {
        QSettings s = settings();
        s.remove(QStringLiteral("recentFiles"));
    });
}

void MainWindow::rememberRecent(const QString& path) {
    QSettings s = settings();
    std::vector<std::string> list;
    for (const QString& entry : s.value(QStringLiteral("recentFiles")).toStringList()) {
        list.push_back(entry.toUtf8().toStdString());
    }
    list = runtime::addRecentFile(std::move(list), path.toUtf8().toStdString());
    QStringList out;
    for (const std::string& entry : list) {
        out << QString::fromUtf8(entry.c_str());
    }
    s.setValue(QStringLiteral("recentFiles"), out);
}

void MainWindow::forgetRecent(const QString& path) {
    QSettings s = settings();
    std::vector<std::string> list;
    for (const QString& entry : s.value(QStringLiteral("recentFiles")).toStringList()) {
        list.push_back(entry.toUtf8().toStdString());
    }
    list = runtime::removeRecentFile(std::move(list), path.toUtf8().toStdString());
    QStringList out;
    for (const std::string& entry : list) {
        out << QString::fromUtf8(entry.c_str());
    }
    s.setValue(QStringLiteral("recentFiles"), out);
}

}  // namespace whitehole::studio
