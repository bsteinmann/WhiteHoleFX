// output_page.cpp: Umsetzung zu output_page.hpp.
#include "output_page.hpp"

#include <QScrollArea>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>

#include "output_form.hpp"
#include "patch_table.hpp"

namespace whitehole::studio {

OutputPage::OutputPage(runtime::Player& player, QWidget* parent)
    : QWidget{parent}, form_{new OutputForm{player}}, patches_{new PatchTable{player}} {
    auto* split = new QSplitter{Qt::Vertical, this};
    auto* outer = new QVBoxLayout{this};
    outer->setContentsMargins(6, 6, 6, 6);
    outer->addWidget(split);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(form_);
    split->addWidget(scroll);
    split->addWidget(patches_);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({260, 420});

    const auto forwardEdit = [this]() {
        const bool was = editing_;
        editing_ = true;
        Q_EMIT edited();
        editing_ = was;
    };
    connect(form_, &OutputForm::edited, this, [this, forwardEdit]() {
        forwardEdit();
        patches_->rebuild();  // ein Ziel kam hinzu oder fiel weg: Zielnummern in der Tabelle bleiben gültig
    });
    connect(patches_, &PatchTable::edited, this, forwardEdit);
    connect(form_, &OutputForm::failed, this, &OutputPage::failed);
    connect(form_, &OutputForm::notice, this, &OutputPage::notice);
    connect(patches_, &PatchTable::failed, this, &OutputPage::failed);
    rebuild();
}

void OutputPage::rebuild() {
    if (editing_) {
        return;
    }
    dirty_ = false;
    form_->rebuild();
    patches_->rebuild();
}

void OutputPage::refresh() {
    if (isVisible()) {
        rebuild();
    } else {
        dirty_ = true;
    }
}

void OutputPage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (dirty_) {
        rebuild();
    }
}

QString OutputPage::exercise() {
    rebuild();
    if (const QString note = form_->exercise(); !note.isEmpty()) {
        return note;
    }
    return patches_->exercise();
}

}  // namespace whitehole::studio
