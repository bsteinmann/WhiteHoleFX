// group_dialog.cpp: Fenster zum Anlegen und Bearbeiten einer Gruppe (siehe group_dialog.hpp).
#include "group_dialog.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "whitehole/graph/node_group.hpp"

namespace whitehole::studio {

GroupDialog::GroupDialog(const QString& title, const QString& name, const QColor& color,
                         std::vector<GroupDialogMember> members, std::vector<QString> takenNames, QWidget* parent)
    : QDialog{parent}, color_{color}, taken_{std::move(takenNames)} {
    setWindowTitle(title);
    resize(560, 520);
    auto* column = new QVBoxLayout{this};
    auto* form = new QFormLayout;
    column->addLayout(form);

    name_ = new QLineEdit{name};
    name_->setMaxLength(static_cast<int>(graph::kMaxGroupNameLength));
    form->addRow(tr("Name"), name_);

    colorButton_ = new QPushButton;
    colorButton_->setToolTip(tr("Farbe der Kachel"));
    connect(colorButton_, &QPushButton::clicked, this, [this]() {
        const QColor picked = QColorDialog::getColor(color_, this, tr("Farbe der Gruppe"));
        if (picked.isValid()) {
            color_ = picked;
            updateColorButton();
        }
    });
    form->addRow(tr("Farbe"), colorButton_);
    updateColorButton();

    column->addWidget(new QLabel{tr("Haken setzen, was auf der Kachel erscheinen soll. Verbundene Anschlüsse nach außen "
                                    "erscheinen immer.")});

    tree_ = new QTreeWidget;
    tree_->setColumnCount(2);
    tree_->setHeaderLabels({tr("Anschluss / Parameter"), tr("Beschriftung auf der Kachel")});
    tree_->setRootIsDecorated(true);
    tree_->setColumnWidth(0, 260);
    column->addWidget(tree_, 1);

    for (const GroupDialogMember& member : members) {
        auto* top = new QTreeWidgetItem{tree_, QStringList{member.title}};
        top->setFlags(Qt::ItemIsEnabled);
        const auto addRows = [&](const std::vector<GroupDialogEntry>& entries, bool parameter) {
            for (const GroupDialogEntry& entry : entries) {
                QString text;
                if (parameter) {
                    text = tr("Parameter: %1").arg(entry.name);
                } else {
                    text = entry.output ? tr("Ausgang: %1").arg(entry.name) : tr("Eingang: %1").arg(entry.name);
                }
                auto* child = new QTreeWidgetItem{top, QStringList{text}};
                child->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
                child->setCheckState(0, entry.checked ? Qt::Checked : Qt::Unchecked);
                auto* edit = new QLineEdit{entry.label};
                tree_->setItemWidget(child, 1, edit);
                rows_.push_back(Row{member.id, entry, parameter, child, edit});
            }
        };
        addRows(member.ports, false);
        addRows(member.parameters, true);
    }
    tree_->expandAll();

    library_ = new QCheckBox{tr("In der Bibliothek speichern")};
    library_->setChecked(true);
    library_->setToolTip(tr("Die Gruppe erscheint links in der Palette unter \"Gruppen\" und lässt sich in jedes Projekt einfügen."));
    column->addWidget(library_);

    auto* buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel};
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    column->addWidget(buttons);
}

void GroupDialog::updateColorButton() {
    colorButton_->setText(color_.name());
    colorButton_->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                                    .arg(color_.name(), color_.lightness() > 128 ? QStringLiteral("black") : QStringLiteral("white")));
}

void GroupDialog::hideLibraryOption() { library_->setVisible(false); }

bool GroupDialog::saveToLibrary() const { return library_->isVisibleTo(this) && library_->isChecked(); }

QString GroupDialog::name() const { return name_->text().trimmed(); }

QString GroupDialog::problem() const {
    const QString n = name();
    if (n.isEmpty()) {
        return tr("Die Gruppe braucht einen Namen.");
    }
    if (std::ranges::find(taken_, n) != taken_.end()) {
        return tr("Eine Gruppe heißt schon \"%1\".").arg(n);
    }
    return {};
}

void GroupDialog::accept() {
    const QString message = problem();
    if (!message.isEmpty()) {
        QMessageBox::warning(this, windowTitle(), message);
        return;
    }
    QDialog::accept();
}

std::vector<GroupDialogChoice> GroupDialog::chosen(bool parameters) const {
    std::vector<GroupDialogChoice> result;
    for (const Row& row : rows_) {
        if (row.parameter != parameters || row.item->checkState(0) != Qt::Checked) {
            continue;
        }
        GroupDialogEntry entry = row.entry;
        entry.label = row.label->text().trimmed();
        if (entry.label.isEmpty()) {
            entry.label = entry.name;
        }
        entry.checked = true;
        result.push_back(GroupDialogChoice{row.node, std::move(entry)});
    }
    return result;
}

std::vector<GroupDialogChoice> GroupDialog::chosenPorts() const { return chosen(false); }
std::vector<GroupDialogChoice> GroupDialog::chosenParameters() const { return chosen(true); }

}  // namespace whitehole::studio
