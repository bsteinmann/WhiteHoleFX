// main_window_editor.cpp: das Dock "Nodes" (Palette links, Editor rechts davon).
//
// Warum eine eigene Datei: main_window.cpp ist an der 400-Zeilen-Grenze, und der Editor-Aufbau wächst
// mit der Palette weiter.
#include <QApplication>
#include <QDockWidget>
#include <QMimeData>
#include <QPoint>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTemporaryDir>

#include <filesystem>
#include <string>

#include "main_window.hpp"
#include "node_editor.hpp"
#include "node_items.hpp"
#include "node_palette.hpp"

namespace whitehole::studio {

void MainWindow::buildEditorDock() {
    editor_ = new NodeEditor{*player_, this};
    {
        const QByteArray folder = (QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/presets")).toUtf8();
        presets_ = std::make_unique<runtime::PresetStore>(std::filesystem::path{
            std::u8string{reinterpret_cast<const char8_t*>(folder.constData()), static_cast<std::size_t>(folder.size())}});
        editor_->setPresetStore(presets_.get());
        const QByteArray groupFolder = (QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/groups")).toUtf8();
        groupLibrary_ = std::make_unique<runtime::PresetStore>(std::filesystem::path{std::u8string{
            reinterpret_cast<const char8_t*>(groupFolder.constData()), static_cast<std::size_t>(groupFolder.size())}});
        editor_->setGroupStore(groupLibrary_.get());
    }
    connect(editor_, &NodeEditor::edited, this, &MainWindow::onGraphEdited);
    connect(editor_, &NodeEditor::selectionChanged, this, &MainWindow::onNodeSelected);
    connect(editor_, &NodeEditor::notice, this, [this](const QString& message) { statusBar()->showMessage(message, 8000); });
    connect(editor_, &NodeEditor::failed, this, [this](const QString& message) { statusBar()->showMessage(message, 8000); });

    palette_ = new NodePalette{player_->registry(), this};
    editor_->setDeleteZone(palette_);
    connect(editor_, &NodeEditor::deleteZoneHover, palette_, &NodePalette::setRemoveHighlight);
    connect(editor_, &NodeEditor::wireDragBegan, palette_, [this](int type, bool fromOutput) {
        palette_->setWireNeed(static_cast<graph::PortType>(type), fromOutput);
    });
    connect(editor_, &NodeEditor::wireDragEnded, palette_, &NodePalette::clearWireNeed);
    connect(palette_, &NodePalette::addRequested, this,
            [this](const QString& typeId) { editor_->addNodeAtCenter(typeId.toStdString()); });
    palette_->setLibrary(presets_.get(), groupLibrary_.get());
    connect(editor_, &NodeEditor::libraryChanged, palette_, &NodePalette::refreshLibrary);
    connect(palette_, &NodePalette::libraryAddRequested, this, [this](const QString& kind, const QString& name) {
        editor_->insertLibraryItem(kind, name, editor_->sceneCenter() - QPointF{70.0, 20.0});
    });

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
    if (const QString styles = checkPortStyles(); !styles.isEmpty()) {
        return tr("Palette: %1").arg(styles);
    }
    palette_->setFilter({});
    // Beim Start sind alle Kategorien zugeklappt (nur die Favoriten dürfen offen sein).
    const QStringList savedFavorites = palette_->favorites();
    for (const QString& id : savedFavorites) {
        palette_->setFavorite(id, false);
    }
    if (palette_->expandedCategoryCount() != 0) {
        return tr("Palette: Kategorien sind nicht zugeklappt (%1 offen)").arg(palette_->expandedCategoryCount());
    }
    const int all = palette_->visibleTypeCount();
    palette_->setFavorite(QStringLiteral("wh.generator.solid"), true);
    const bool favoriteWorks = palette_->isFavorite(QStringLiteral("wh.generator.solid")) &&
                               palette_->visibleTypeCount() == all && palette_->expandedCategoryCount() == 1;
    palette_->setFavorite(QStringLiteral("wh.generator.solid"), false);
    for (const QString& id : savedFavorites) {
        palette_->setFavorite(id, true);
    }
    if (!favoriteWorks) {
        return tr("Palette: Favoriten-Gruppe verhält sich falsch");
    }
    palette_->setFilter(QStringLiteral("gradient"));
    const int some = palette_->visibleTypeCount();
    palette_->setFilter({});
    if (all < 10 || some < 1 || some >= all) {
        return tr("Palette: Filter zeigt %1 von %2 Typen").arg(some).arg(all);
    }
    // Bibliothek: Presets und Gruppen erscheinen als eigene Rubriken, ohne die Typen zu verändern
    {
        QTemporaryDir temp;
        if (!temp.isValid()) {
            return tr("Palette: kein temporärer Ordner für die Bibliothek");
        }
        const QByteArray folder = temp.path().toUtf8();
        const std::filesystem::path root{
            std::u8string{reinterpret_cast<const char8_t*>(folder.constData()), static_cast<std::size_t>(folder.size())}};
        const runtime::PresetStore testPresets{root / "p"};
        const runtime::PresetStore testGroups{root / "g"};
        (void)testPresets.save("Eins", "x");
        (void)testGroups.save("Zwei", "x");
        (void)testGroups.save("Drei", "x");
        palette_->setLibrary(&testPresets, &testGroups);
        const int entries = palette_->libraryEntryCount();
        const int typesWithLibrary = palette_->visibleTypeCount();
        const int openWithLibrary = palette_->expandedCategoryCount();
        palette_->setFilter(QStringLiteral("zwei"));
        const int filtered = palette_->libraryEntryCount();
        palette_->setFilter({});
        palette_->setLibrary(presets_.get(), groupLibrary_.get());
        if (entries != 3 || filtered != 1 || typesWithLibrary != all || openWithLibrary != 0) {
            return tr("Palette: Bibliothek zeigt %1 Einträge (erwartet 3), gefiltert %2 (erwartet 1), %3 Typen statt %4, %5 offene Rubriken")
                .arg(entries).arg(filtered).arg(typesWithLibrary).arg(all).arg(openWithLibrary);
        }
    }
    // Farbige Punkte und Hervorhebung beim Kabelziehen
    {
        const bool wasColored = palette_->colored();
        palette_->setColored(true);
        const int dots = palette_->dotTypeCount();
        palette_->setColored(false);
        const int noDots = palette_->dotTypeCount();
        palette_->setColored(wasColored);
        if (dots < all - 5 || noDots != 0) {
            return tr("Palette: %1 Punkte mit Farbe, %2 ohne (von %3 Typen)").arg(dots).arg(noDots).arg(all);
        }
        palette_->setWireNeed(graph::PortType::Text, true);  // an Text lässt sich fast nichts anschließen
        const int dimmedForText = palette_->dimmedTypeCount();
        palette_->setWireNeed(graph::PortType::Field, true);
        const int dimmedForField = palette_->dimmedTypeCount();
        palette_->clearWireNeed();
        if (dimmedForText <= dimmedForField || dimmedForField == 0 || palette_->dimmedTypeCount() != 0) {
            return tr("Palette: abgedunkelt für Text %1, für Bild %2, danach %3")
                .arg(dimmedForText)
                .arg(dimmedForField)
                .arg(palette_->dimmedTypeCount());
        }
    }
    const int before = editor_->nodeCount();
    // Ziehen und Ablegen aus der Palette: derselbe Weg wie mit der Maus (Typ-ID im MIME-Typ).
    QMimeData mime;
    mime.setData(QString::fromLatin1(kNodeTypeMime), QByteArray{"wh.generator.solid"});
    if (!editor_->handleNodeDrop(&mime, QPoint{60, 60}) || editor_->nodeCount() != before + 1) {
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

void MainWindow::onStrictInputs(bool on) {
    const Result<void> r = player_->setStrictInputs(on);
    if (!r.ok()) {
        reportError(tr("Strenge Eingänge"), r.error().message, false);
    }
    afterRestore();  // setzt auch den Haken im Menü auf den Stand des Players zurück, falls es nicht ging
}

}  // namespace whitehole::studio
