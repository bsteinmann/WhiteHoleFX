// main_window.cpp: Fenster, Menüs, Takt.
#include "main_window.hpp"

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QApplication>
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

#include "layout_panel.hpp"
#include "node_editor.hpp"
#include "parameter_panel.hpp"
#include "preview_controller.hpp"
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

    layoutPanel_ = new LayoutPanel{*player_, this};
    connect(layoutPanel_, &LayoutPanel::edited, this, &MainWindow::onLayoutEdited);
    connect(layoutPanel_, &LayoutPanel::failed, this,
            [this](const QString& message) { statusBar()->showMessage(message, 8000); });
    previewController_ = new PreviewController{*player_, preview_, layoutPanel_, this};
    connect(previewController_, &PreviewController::edited, this, &MainWindow::onLayoutEdited);
    connect(previewController_, &PreviewController::failed, this,
            [this](const QString& message) { statusBar()->showMessage(message, 8000); });
    layoutDock_ = new QDockWidget{tr("Layout"), this};
    layoutDock_->setWidget(layoutPanel_);
    addDockWidget(Qt::RightDockWidgetArea, layoutDock_);
    tabifyDockWidget(layoutDock_, dock_);  // Layout und Parameter teilen sich den Platz rechts
    dock_->raise();

    editor_ = new NodeEditor{*player_, this};
    connect(editor_, &NodeEditor::edited, this, &MainWindow::onGraphEdited);
    connect(editor_, &NodeEditor::selectionChanged, this, &MainWindow::onNodeSelected);
    connect(editor_, &NodeEditor::notice, this, [this](const QString& message) { statusBar()->showMessage(message, 8000); });
    connect(editor_, &NodeEditor::failed, this, [this](const QString& message) { statusBar()->showMessage(message, 8000); });
    editorDock_ = new QDockWidget{tr("Nodes"), this};
    editorDock_->setWidget(editor_);
    addDockWidget(Qt::BottomDockWidgetArea, editorDock_);
    resizeDocks({editorDock_}, {300}, Qt::Vertical);

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

void MainWindow::updateActions() {
    const runtime::PlayerStatus s = player_->status();
    reloadAction_->setEnabled(!s.path.empty());
    saveAction_->setEnabled(s.loaded);
    saveAsAction_->setEnabled(s.loaded);
    runAction_->setEnabled(s.loaded);
    runAction_->setChecked(s.running);
    runAction_->setText(s.running ? tr("&Stopp") : tr("&Start"));
    sendAction_->setChecked(gate_->enabled());
    updateUndoActions();
}

void MainWindow::updateTitle() {
    const runtime::PlayerStatus s = player_->status();
    if (!s.loaded) {
        setWindowTitle(tr("WhiteHoleFX"));
        return;
    }
    const QString name = s.path.empty() ? tr("unbenannt") : QFileInfo{fromStd(s.path)}.fileName();
    setWindowTitle(tr("WhiteHoleFX – %1%2").arg(name, player_->modified() ? QStringLiteral(" *") : QString{}));
}

void MainWindow::refreshParameters() {
    parameters_->rebuild(player_->nodes());
    parameters_->showNode(editor_->selectedNode());
}

void MainWindow::onNodeSelected() {
    const std::optional<NodeId> node = editor_->selectedNode();
    const bool shown = parameters_->showNode(node);
    if (shown && dock_->isVisible()) {
        dock_->raise();  // Layout und Parameter teilen sich den Platz: Parameter nach vorn
    } else if (node && !shown) {
        statusBar()->showMessage(tr("Dieser Node hat keine einstellbaren Parameter."), 4000);
    }
}

void MainWindow::onGraphEdited(bool structural) {
    if (structural) {
        refreshParameters();
    }
    updateTitle();
    updateActions();
}

bool MainWindow::confirmDiscard() {
    if (!player_->modified()) {
        return true;
    }
    const auto answer = QMessageBox::question(this, tr("Ungespeicherte Änderungen"),
                                              tr("Das Projekt wurde geändert. Änderungen verwerfen?"),
                                              QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
    return answer == QMessageBox::Discard;
}

void MainWindow::updateUndoActions() {
    undoAction_->setEnabled(player_->canUndo());
    redoAction_->setEnabled(player_->canRedo());
}

void MainWindow::onLayoutEdited() {
    if (timer_.isActive()) {
        timer_.setInterval(std::max(1, static_cast<int>(std::lround(1000.0 / player_->framesPerSecond()))));
    }
    updateTitle();
    updateActions();
    refreshImage();
}

void MainWindow::afterRestore() {
    layoutPanel_->rebuild();
    editor_->rebuild();
    refreshParameters();
    updateTitle();
    updateActions();
}

void MainWindow::onUndo() {
    const Result<void> r = player_->undo();
    if (r.ok()) {
        afterRestore();
    }
}

void MainWindow::onRedo() {
    const Result<void> r = player_->redo();
    if (r.ok()) {
        afterRestore();
    }
}

void MainWindow::updateStatus() {
    updateTitle();
    updateUndoActions();
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
    if (previewController_ != nullptr) {
        options.frame = previewController_->frozenFrame();  // während des Ziehens bleibt die Ansicht stehen
    }
    const auto image = player_->image(options);
    if (!image.ok()) {
        preview_->setMessage(fromStd(image.error().message));
        return;
    }
    const preview::Image& img = image.value();
    const QImage view{img.rgba.data(), static_cast<int>(img.width), static_cast<int>(img.height),
                      static_cast<qsizetype>(img.width) * 4, QImage::Format_RGBA8888};
    preview_->setImage(view.copy());  // eigener Speicher: das Bild überlebt den Rückgabewert
    if (previewController_ != nullptr) {
        previewController_->imageRendered(img.frame);
    }
}

QString MainWindow::problem() const {
    const auto error = player_->status().lastError;
    return error ? fromStd(error->message) : QString{};
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
    if (!confirmDiscard()) {
        event->ignore();
        return;
    }
    stopPlayback();
    event->accept();
}

}  // namespace whitehole::studio
