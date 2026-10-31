// node_editor_search.cpp: Suchfeld zum Hinzufügen von Nodes (Strg+Leertaste oder Rechtsklick).
//
// Das Menü "Node hinzufügen" wird mit jedem neuen Node-Typ länger; Tippen ist schneller. Ein Popup
// mit Eingabefeld und Trefferliste: Pfeiltasten wählen, Enter legt den Node an der Maus an.
#include <QDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <tuple>

#include "node_editor.hpp"
#include "node_search.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

/// Leitet Pfeiltasten aus dem Eingabefeld an die Liste weiter, damit man beim Tippen bleibt.
class ArrowKeys final : public QObject {
public:
    ArrowKeys(QListWidget* list, QObject* parent) : QObject{parent}, list_{list} {}

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent*>(event)->key();
            if (key == Qt::Key_Down || key == Qt::Key_Up) {
                const int rows = list_->count();
                if (rows > 0) {
                    const int step = key == Qt::Key_Down ? 1 : -1;
                    list_->setCurrentRow(std::clamp(list_->currentRow() + step, 0, rows - 1));
                }
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QListWidget* list_;
};

}  // namespace

std::vector<const graph::NodeDescriptor*> filterNodeTypes(std::vector<const graph::NodeDescriptor*> all,
                                                          const QString& text) {
    const QStringList words = text.toLower().split(QLatin1Char{' '}, Qt::SkipEmptyParts);
    std::erase_if(all, [&](const graph::NodeDescriptor* type) {
        const QString haystack =
            (fromStd(type->displayName) + QLatin1Char{' '} + fromStd(type->category) + QLatin1Char{' '} +
             fromStd(type->typeId))
                .toLower();
        return !std::ranges::all_of(words, [&](const QString& word) { return haystack.contains(word); });
    });
    std::ranges::sort(all, [](const graph::NodeDescriptor* a, const graph::NodeDescriptor* b) {
        return std::tie(a->category, a->displayName) < std::tie(b->category, b->displayName);
    });
    return all;
}

void NodeEditor::showSearch(const QPointF& scenePos) {
    const project::Project* project = player_.project();
    if (project == nullptr) {
        return;
    }
    const std::vector<const graph::NodeDescriptor*> all = project->graph.registry().all();

    QDialog dialog{this, Qt::Popup};
    auto* layout = new QVBoxLayout{&dialog};
    layout->setContentsMargins(4, 4, 4, 4);
    auto* edit = new QLineEdit{&dialog};
    edit->setPlaceholderText(tr("Node suchen … (Pfeiltasten, Enter)"));
    auto* list = new QListWidget{&dialog};
    layout->addWidget(edit);
    layout->addWidget(list);
    edit->installEventFilter(new ArrowKeys{list, &dialog});

    const auto refill = [&, list](const QString& text) {
        list->clear();
        for (const graph::NodeDescriptor* type : filterNodeTypes(all, text)) {
            auto* item = new QListWidgetItem{fromStd(type->displayName) + QStringLiteral("   –   ") + fromStd(type->category), list};
            item->setData(Qt::UserRole, fromStd(type->typeId));
        }
        list->setCurrentRow(0);
    };
    refill({});
    connect(edit, &QLineEdit::textChanged, &dialog, refill);
    connect(edit, &QLineEdit::returnPressed, &dialog, [&]() {
        if (list->currentItem() != nullptr) {
            dialog.accept();
        }
    });
    connect(list, &QListWidget::itemActivated, &dialog, [&]() { dialog.accept(); });

    dialog.resize(380, 320);
    dialog.move(viewport()->mapToGlobal(mapFromScene(scenePos)));
    edit->setFocus();
    if (dialog.exec() == QDialog::Accepted && list->currentItem() != nullptr) {
        addNodeAt(list->currentItem()->data(Qt::UserRole).toString().toStdString(), scenePos);
    }
}

}  // namespace whitehole::studio
