// main_window.cpp: Fenster, Menüs, Takt.
#include "main_window.hpp"

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QSpinBox>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QStatusBar>
#include <QToolBar>
#include <QUrl>

#include <algorithm>
#include <cmath>

#include "parameter_panel.hpp"
#include "preview_widget.hpp"

namespace whitehole::studio {

namespace {

/// Ersatz-Ziel, falls sich kein UDP-Socket öffnen lässt: Die Vorschau läuft trotzdem.
class DiscardSink final : public output::IPacketSink {
public:
    Result<void> send(const output::Endpoint&, std::span<const std::uint8_t>) override { return {}; }
};

DiscardSink gDiscard;

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

}  // namespace

MainWindow::MainWindow(gpu::IGpuDevice* device, const QString& deviceNote, QWidget* parent)
    : QMainWindow{parent}, deviceNote_{deviceNote} {
    library_ = nodes::BuiltinLibrary::create().value();
    auto sender = output::UdpSender::create();
    if (sender.ok()) {
        udp_ = std::move(sender).value();
    }
    output::IPacketSink& inner =
        udp_ ? static_cast<output::IPacketSink&>(*udp_) : static_cast<output::IPacketSink&>(gDiscard);
    gate_ = std::make_unique<runtime::GatedPacketSink>(inner);
    player_ = std::make_unique<runtime::Player>(device, *library_, *gate_);

    preview_ = new PreviewWidget{this};
    setCentralWidget(preview_);
    connect(preview_, &PreviewWidget::resized, this, [this](QSize) { refreshImage(); });

    parameters_ = new ParameterPanel{this};
    parameters_->setChangeHandler(
        [this](NodeId node, const std::string& key,
               graph::ParameterValue value) -> std::optional<graph::ParameterValue> {
            const auto stored = player_->setParameter(node, key, std::move(value));
            if (!stored.ok()) {
                statusBar()->showMessage(fromStd(stored.error().message), 8000);
                return std::nullopt;
            }
            return stored.value();
        });
    dock_ = new QDockWidget{tr("Parameter"), this};
    dock_->setWidget(parameters_);
    addDockWidget(Qt::RightDockWidgetArea, dock_);

    statusLabel_ = new QLabel{this};
    statusBar()->addPermanentWidget(statusLabel_);

    buildActions();
    setAcceptDrops(true);
    setWindowTitle(tr("WhiteHoleFX"));

    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &MainWindow::tick);
    statusTimer_.setInterval(250);
    connect(&statusTimer_, &QTimer::timeout, this, &MainWindow::updateStatus);
    statusTimer_.start();

    preview_->setMessage(tr("Projekt öffnen (Strg+O) oder eine .whfx-Datei hierher ziehen.\n\n%1").arg(deviceNote_));
    updateActions();
    updateStatus();
}

MainWindow::~MainWindow() {
    timer_.stop();
    player_->stop();
}

void MainWindow::buildActions() {
    openAction_ = new QAction{tr("&Öffnen …"), this};
    openAction_->setShortcut(QKeySequence::Open);
    connect(openAction_, &QAction::triggered, this, &MainWindow::onOpen);

    reloadAction_ = new QAction{tr("&Neu laden"), this};
    reloadAction_->setShortcut(QKeySequence{Qt::Key_F5});
    connect(reloadAction_, &QAction::triggered, this, &MainWindow::onReload);

    saveAsAction_ = new QAction{tr("Speichern &unter …"), this};
    saveAsAction_->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction_, &QAction::triggered, this, &MainWindow::onSaveAs);

    quitAction_ = new QAction{tr("&Beenden"), this};
    quitAction_->setShortcut(QKeySequence::Quit);
    connect(quitAction_, &QAction::triggered, this, &QWidget::close);

    runAction_ = new QAction{tr("&Start"), this};
    runAction_->setCheckable(true);
    runAction_->setShortcut(QKeySequence{Qt::Key_Space});
    connect(runAction_, &QAction::triggered, this, &MainWindow::onTogglePlayback);

    sendAction_ = new QAction{tr("&Ausgabe senden"), this};
    sendAction_->setCheckable(true);
    sendAction_->setToolTip(tr("Pakete (Art-Net, sACN, DDP) wirklich ins Netz schicken"));
    connect(sendAction_, &QAction::triggered, this, &MainWindow::onToggleSending);

    QMenu* file = menuBar()->addMenu(tr("&Datei"));
    file->addAction(openAction_);
    file->addAction(reloadAction_);
    file->addAction(saveAsAction_);
    file->addSeparator();
    file->addAction(quitAction_);

    QMenu* view = menuBar()->addMenu(tr("&Ansicht"));
    view->addAction(dock_->toggleViewAction());

    QMenu* run = menuBar()->addMenu(tr("&Wiedergabe"));
    run->addAction(runAction_);
    run->addAction(sendAction_);

    QToolBar* bar = addToolBar(tr("Wiedergabe"));
    bar->setMovable(false);
    bar->addAction(openAction_);
    bar->addAction(runAction_);
    bar->addAction(sendAction_);
}

void MainWindow::updateActions() {
    const runtime::PlayerStatus s = player_->status();
    reloadAction_->setEnabled(!s.path.empty());
    saveAsAction_->setEnabled(s.loaded);
    runAction_->setEnabled(s.loaded);
    runAction_->setChecked(s.running);
    runAction_->setText(s.running ? tr("&Stopp") : tr("&Start"));
    sendAction_->setChecked(gate_->enabled());
}

void MainWindow::updateStatus() {
    const runtime::PlayerStatus s = player_->status();
    QString text;
    if (!s.loaded) {
        text = tr("Kein Projekt");
    } else if (!s.running) {
        text = tr("%1 LEDs, angehalten").arg(s.ledCount);
    } else {
        text = tr("%1 LEDs, Frame %2 (verworfen %3), Pakete %4, Sendefehler %5")
                   .arg(s.ledCount)
                   .arg(s.frames)
                   .arg(s.framesDropped)
                   .arg(s.packets)
                   .arg(s.sendErrors);
    }
    text += gate_->enabled() ? tr("   |   AUSGABE AN") : tr("   |   Ausgabe aus (%1 Pakete nicht gesendet)").arg(gate_->suppressed());
    statusLabel_->setText(text);
    if (s.lastError) {
        statusBar()->showMessage(fromStd(s.lastError->message));
    }
}

void MainWindow::reportError(const QString& title, const std::string& message, bool interactive) {
    const QString text = fromStd(message);
    statusBar()->showMessage(title + QStringLiteral(": ") + text, 15000);
    if (interactive) {
        QMessageBox::critical(this, title, text);
    }
}

bool MainWindow::openPath(const QString& path, bool interactive) {
    timer_.stop();
    const Result<void> r = player_->load(path.toUtf8().toStdString());
    if (!r.ok()) {
        reportError(tr("Projekt laden"), r.error().message, interactive);
        updateActions();
        return false;
    }
    preview_->setMessage({});
    parameters_->rebuild(player_->nodes());
    setWindowTitle(tr("WhiteHoleFX – %1").arg(QFileInfo{path}.fileName()));
    statusBar()->showMessage(tr("Geladen: %1").arg(path), 5000);
    updateActions();
    refreshImage();
    return true;
}

bool MainWindow::startPlayback(bool interactive) {
    const Result<void> r = player_->start();
    if (!r.ok()) {
        reportError(tr("Start"), r.error().message, interactive);
        updateActions();
        return false;
    }
    const int ms = std::max(1, static_cast<int>(std::lround(1000.0 / player_->framesPerSecond())));
    timer_.start(ms);
    updateActions();
    return true;
}

void MainWindow::stopPlayback() {
    timer_.stop();
    player_->stop();
    updateActions();
    refreshImage();
}

void MainWindow::tick() {
    const Result<void> r = player_->step();
    if (!r.ok()) {
        timer_.stop();
        reportError(tr("Frame"), r.error().message, false);
        updateActions();
    }
    refreshImage();
}

void MainWindow::refreshImage() {
    if (player_->project() == nullptr) {
        return;
    }
    preview::PreviewOptions options;
    options.width = static_cast<std::uint32_t>(std::clamp(preview_->width(), 64, 4096));
    options.height = static_cast<std::uint32_t>(std::clamp(preview_->height(), 64, 4096));
    const auto image = player_->image(options);
    if (!image.ok()) {
        preview_->setMessage(fromStd(image.error().message));
        return;
    }
    const preview::Image& img = image.value();
    const QImage view{img.rgba.data(), static_cast<int>(img.width), static_cast<int>(img.height),
                      static_cast<qsizetype>(img.width) * 4, QImage::Format_RGBA8888};
    preview_->setImage(view.copy());  // eigener Speicher: das Bild überlebt den Rückgabewert
}

int MainWindow::exerciseParameters() {
    int changed = 0;
    for (QSlider* slider : parameters_->findChildren<QSlider*>()) {
        if (slider->isEnabled()) {
            slider->setValue((slider->minimum() + slider->maximum()) / 3);
            ++changed;
        }
    }
    for (QDoubleSpinBox* spin : parameters_->findChildren<QDoubleSpinBox*>()) {
        if (spin->isEnabled()) {
            spin->setValue(spin->value() + spin->singleStep());
            ++changed;
        }
    }
    for (QSpinBox* spin : parameters_->findChildren<QSpinBox*>()) {
        if (spin->isEnabled()) {
            spin->setValue(spin->value() + 1);
            ++changed;
        }
    }
    for (QCheckBox* check : parameters_->findChildren<QCheckBox*>()) {
        if (check->isEnabled()) {
            check->toggle();
            ++changed;
        }
    }
    return changed;
}

QString MainWindow::problem() const {
    const auto error = player_->status().lastError;
    return error ? fromStd(error->message) : QString{};
}

void MainWindow::onOpen() {
    const QString start = player_->status().path.empty() ? QString{} : fromStd(player_->status().path);
    const QString path = QFileDialog::getOpenFileName(this, tr("Projekt öffnen"), start,
                                                      tr("WhiteHoleFX-Projekte (*.whfx);;Alle Dateien (*)"));
    if (!path.isEmpty()) {
        openPath(path);
    }
}

void MainWindow::onReload() {
    const std::string path = player_->status().path;
    if (path.empty()) {
        return;
    }
    const bool wasRunning = player_->status().running;
    if (openPath(fromStd(path)) && wasRunning) {
        startPlayback();
    }
}

void MainWindow::onSaveAs() {
    const QString start = player_->status().path.empty() ? QString{} : fromStd(player_->status().path);
    QString path = QFileDialog::getSaveFileName(this, tr("Projekt speichern unter"), start,
                                                tr("WhiteHoleFX-Projekte (*.whfx)"));
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo{path}.suffix().isEmpty()) {
        path += QStringLiteral(".whfx");
    }
    const Result<void> r = player_->save(path.toUtf8().toStdString());
    if (!r.ok()) {
        reportError(tr("Speichern"), r.error().message, true);
        return;
    }
    setWindowTitle(tr("WhiteHoleFX – %1").arg(QFileInfo{path}.fileName()));
    statusBar()->showMessage(tr("Gespeichert: %1").arg(path), 5000);
    updateActions();
}

void MainWindow::onTogglePlayback(bool run) {
    if (run) {
        startPlayback();
    } else {
        stopPlayback();
    }
}

void MainWindow::onToggleSending(bool send) {
    gate_->setEnabled(send);
    updateActions();
    updateStatus();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        openPath(urls.first().toLocalFile());
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    stopPlayback();
    event->accept();
}

}  // namespace whitehole::studio
