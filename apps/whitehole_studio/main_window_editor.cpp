// main_window_editor.cpp: das Dock "Nodes" (Palette links, Editor rechts davon).
//
// Warum eine eigene Datei: main_window.cpp ist an der 400-Zeilen-Grenze, und der Editor-Aufbau wächst
// mit der Palette weiter.
#include <QApplication>
#include <QDockWidget>
#include <QDropEvent>
#include <QMimeData>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>

#include <filesystem>
#include <string>

#include "main_window.hpp"
#include "node_editor.hpp"
#include "node_palette.hpp"

namespace whitehole::studio {

void MainWindow::buildEditorDock() {
    editor_ = new NodeEditor{*player_, this};
    {
        const QByteArray folder = (QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/presets")).toUtf8();
        presets_ = std::make_unique<runtime::PresetStore>(std::filesystem::path{
            std::u8string{reinterpret_cast<const char8_t*>(folder.constData()), static_cast<std::size_t>(folder.size())}});
        editor_->setPresetStore(presets_.get());
    }
    connect(editor_, &NodeEditor::edited, this, &MainWindow::onGraphEdited);
    connect(editor_, &NodeEditor::selectionChanged, this, &MainWindow::onNodeSelected);
    connect(editor_, &NodeEditor::notice, this, [this](const QString& message) { statusBar()->showMessage(message, 8000); });
    connect(editor_, &NodeEditor::failed, this, [this](const QString& message) { statusBar()->showMessage(message, 8000); });

    palette_ = new NodePalette{player_->registry(), this};
    editor_->setDeleteZone(palette_);
    connect(editor_, &NodeEditor::deleteZoneHover, palette_, &NodePalette::setRemoveHighlight);
    connect(palette_, &NodePalette::addRequested, this,
            [this](const QString& typeId) { editor_->addNodeAtCenter(typeId.toStdString()); });

    auto* split = new QSplitter{Qt::Horizontal, this};
    split->addWidget(palette_);
    split->addWidget(editor_);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({230, 800});
    split->setChildrenCollapsible(true);

    editorDock_ = new QDockWidget{tr("Nodes"), this};
    editorDock_->setWidget(split);
    addDockWidget(Qt::BottomDockWidgetArea, editorDock_);
    resizeDocks({editorDock_}, {300}, Qt::Vertical);
}

QString MainWindow::exercisePalette() {
    if (palette_ == nullptr || editor_ == nullptr) {
        return tr("Palette: nicht gebaut");
    }
    palette_->setFilter({});
    const int all = palette_->visibleTypeCount();
    palette_->setFilter(QStringLiteral("gradient"));
    const int some = palette_->visibleTypeCount();
    palette_->setFilter({});
    if (all < 10 || some < 1 || some >= all) {
        return tr("Palette: Filter zeigt %1 von %2 Typen").arg(some).arg(all);
    }
    const int before = editor_->nodeCount();
    // Ziehen und Ablegen aus der Palette: derselbe Weg wie mit der Maus (Typ-ID im MIME-Typ).
    QMimeData mime;
    mime.setData(QString::fromLatin1(kNodeTypeMime), QByteArray{"wh.generator.solid"});
    QDropEvent drop{QPointF{60.0, 60.0}, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier};
    QApplication::sendEvent(editor_->viewport(), &drop);
    if (editor_->nodeCount() != before + 1) {
        return tr("Palette: Ablegen hat keinen Node angelegt (%1 -> %2)").arg(before).arg(editor_->nodeCount());
    }
    if (!editor_->addNodeAtCenter("wh.generator.solid").has_value() || editor_->nodeCount() != before + 2) {
        return tr("Palette: Doppelklick-Weg legt keinen Node an");
    }
    // Rückgängig muss beide wieder entfernen
    onUndo();
    onUndo();
    if (editor_->nodeCount() != before) {
        return tr("Palette: Rückgängig lässt %1 statt %2 Nodes").arg(editor_->nodeCount()).arg(before);
    }
    for (int i = 0; i < 3; ++i) {
        tick();
        QApplication::processEvents();
    }
    return problem();
}

}  // namespace whitehole::studio
