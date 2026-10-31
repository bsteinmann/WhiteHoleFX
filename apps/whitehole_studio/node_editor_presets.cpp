// node_editor_presets.cpp: Presets im Node-Editor (speichern, einfügen, löschen).
//
// Die Arbeit (Text, Datei, Einfügen als ein Schritt) steckt in runtime/node_preset und
// Player::pasteNodes. Hier geht es um die Bedienung: Namensabfrage, Rückfrage beim Ersetzen, Menüs.
#include <QAction>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>

#include "node_editor.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }
std::string toStd(const QString& text) { return text.toUtf8().toStdString(); }

}  // namespace

bool NodeEditor::savePreset(const QString& name) {
    if (presets_ == nullptr || player_.project() == nullptr) {
        return false;
    }
    const std::vector<NodeId> ids = selectedNodes();
    if (ids.empty()) {
        fail("Es sind keine Nodes ausgewählt");
        return false;
    }
    const runtime::NodeClipboard clip = runtime::copyNodes(player_.project()->graph, ids);
    const auto text = runtime::presetToText(clip, player_.registry());
    if (!text.ok()) {
        fail(text.error().message);
        return false;
    }
    if (const auto saved = presets_->save(toStd(name), text.value()); !saved.ok()) {
        fail(saved.error().message);
        return false;
    }
    Q_EMIT notice(tr("Preset \"%1\" gespeichert (%2 Nodes).").arg(name).arg(clip.nodes.size()));
    return true;
}

bool NodeEditor::savePresetInteractive() {
    if (presets_ == nullptr || selectedNodes().empty()) {
        return false;
    }
    bool accepted = false;
    const QString name =
        QInputDialog::getText(this, tr("Als Preset speichern"), tr("Name des Presets:"), QLineEdit::Normal,
                              QString{}, &accepted)
            .trimmed();
    if (!accepted || name.isEmpty()) {
        return false;
    }
    if (!runtime::isValidPresetName(toStd(name))) {
        fail("Ungültiger Name (keine Sonderzeichen wie / \\ : * ? \" < > |, höchstens 64 Zeichen)");
        return false;
    }
    if (presets_->contains(toStd(name)) &&
        QMessageBox::question(this, tr("Preset ersetzen"), tr("Es gibt schon ein Preset \"%1\". Ersetzen?").arg(name),
                              QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) {
        return false;
    }
    return savePreset(name);
}

bool NodeEditor::insertPreset(const QString& name, const QPointF& scenePos) {
    if (presets_ == nullptr || player_.project() == nullptr) {
        return false;
    }
    const auto text = presets_->load(toStd(name));
    if (!text.ok()) {
        fail(text.error().message);
        return false;
    }
    const auto clip = runtime::presetFromText(text.value(), player_.registry());
    if (!clip.ok()) {
        fail("Preset \"" + toStd(name) + "\": " + clip.error().message);
        return false;
    }
    const Vec2 corner = clip.value().topLeft();
    return pasteWithOffset(clip.value(),
                           scenePos - QPointF{static_cast<qreal>(corner.x), static_cast<qreal>(corner.y)});
}

bool NodeEditor::deletePreset(const QString& name) {
    if (presets_ == nullptr) {
        return false;
    }
    if (const auto removed = presets_->remove(toStd(name)); !removed.ok()) {
        fail(removed.error().message);
        return false;
    }
    Q_EMIT notice(tr("Preset \"%1\" gelöscht.").arg(name));
    return true;
}

void NodeEditor::buildPresetMenus(QMenu& menu, const QPointF& scenePos) {
    if (presets_ == nullptr) {
        return;
    }
    const std::vector<std::string> names = presets_->list();
    QMenu* insert = menu.addMenu(tr("Preset einfügen"));
    QMenu* remove = menu.addMenu(tr("Preset löschen"));
    if (names.empty()) {
        insert->addAction(tr("(keine Presets)"))->setEnabled(false);
        remove->setEnabled(false);
        return;
    }
    for (const std::string& stdName : names) {
        const QString name = fromStd(stdName);
        QString label = name;
        label.replace(QLatin1Char('&'), QStringLiteral("&&"));  // sonst wäre "&" ein Tastenkürzel
        connect(insert->addAction(label), &QAction::triggered, this, [this, name, scenePos]() { insertPreset(name, scenePos); });
        connect(remove->addAction(label), &QAction::triggered, this, [this, name]() {
            if (QMessageBox::question(this, tr("Preset löschen"), tr("Preset \"%1\" löschen?").arg(name),
                                      QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Yes) {
                deletePreset(name);
            }
        });
    }
}

}  // namespace whitehole::studio
