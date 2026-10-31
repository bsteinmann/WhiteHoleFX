// parameter_panel.cpp: das Parameterfenster (Gruppen je Node, Zeilen je Parameter).
#include "parameter_panel.hpp"

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <variant>

#include "parameter_editors.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

}  // namespace

ParameterPanel::ParameterPanel(QWidget* parent) : QScrollArea{parent} {
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    auto* hint = new QLabel{tr("Kein Projekt geladen."), this};
    hint->setMargin(12);
    setWidget(hint);
}

void ParameterPanel::rebuild(const std::vector<runtime::NodeInfo>& nodes) {
    groups_.clear();
    shown_.reset();
    auto* content = new QWidget;
    auto* column = new QVBoxLayout{content};
    if (nodes.empty()) {
        column->addWidget(new QLabel{tr("Dieses Projekt hat keine einstellbaren Parameter."), content});
    }
    for (const runtime::NodeInfo& node : nodes) {
        auto* group = new QGroupBox{fromStd(node.title), content};
        auto* grid = new QGridLayout{group};
        int row = 0;
        std::unordered_map<std::string, std::shared_ptr<Editor>> holders;
        const auto numberOf = [&node](const std::string& name) -> std::optional<double> {
            for (const runtime::ParameterInfo& p : node.parameters) {
                if (p.key == name && p.value.index() == 2) {
                    return std::get<double>(p.value);
                }
            }
            return std::nullopt;
        };
        for (const runtime::ParameterInfo& info : node.parameters) {
            auto holder = std::make_shared<Editor>();
            holders[info.key] = holder;
            const NodeId id = node.id;
            const std::string key = info.key;
            const Commit commit = [this, holder, id, key](graph::ParameterValue value) {
                if (!handler_) {
                    return;
                }
                if (const auto stored = handler_(id, key, std::move(value))) {
                    holder->show(*stored);
                    if (holder->onStored) {
                        holder->onStored(*stored);
                    }
                }
            };
            std::optional<std::array<double, 2>> sliderRange;
            if (info.value.index() == 2) {
                const auto low = numberOf(key + "Min");
                const auto high = numberOf(key + "Max");
                if (low && high) {
                    sliderRange = std::array<double, 2>{*low, *high};
                }
            }
            *holder = makeEditor(info, commit, group, sliderRange);

            auto* label = new QLabel{fromStd(info.displayName), group};
            auto* reset = new QToolButton{group};
            reset->setText(QStringLiteral("↺"));
            reset->setToolTip(tr("Auf Standardwert zurücksetzen"));
            const graph::ParameterValue defaultValue = info.defaultValue;
            QObject::connect(reset, &QToolButton::clicked, reset, [commit, defaultValue]() { commit(defaultValue); });

            if (info.driven) {
                holder->widget->setEnabled(false);
                reset->setEnabled(false);
                label->setToolTip(tr("Wird durch eine Verbindung im Graphen gesteuert."));
                holder->widget->setToolTip(label->toolTip());
            }
            grid->addWidget(label, row, 0);
            grid->addWidget(holder->widget, row, 1);
            grid->addWidget(reset, row, 2);
            ++row;
        }
        // "<name>Min" und "<name>Max" stellen den Bereich des Schiebereglers von "<name>" ein.
        for (const runtime::ParameterInfo& info : node.parameters) {
            const auto low = numberOf(info.key + "Min");
            const auto high = numberOf(info.key + "Max");
            if (info.value.index() != 2 || !low || !high) {
                continue;
            }
            const auto target = holders[info.key];
            const auto state = std::make_shared<std::array<double, 2>>(std::array<double, 2>{*low, *high});
            holders[info.key + "Min"]->onStored = [target, state](const graph::ParameterValue& v) {
                (*state)[0] = std::get<double>(v);
                if (target->setSliderRange) {
                    target->setSliderRange((*state)[0], (*state)[1]);
                }
            };
            holders[info.key + "Max"]->onStored = [target, state](const graph::ParameterValue& v) {
                (*state)[1] = std::get<double>(v);
                if (target->setSliderRange) {
                    target->setSliderRange((*state)[0], (*state)[1]);
                }
            };
        }
        grid->setColumnStretch(1, 1);
        column->addWidget(group);
        groups_.emplace(node.id.value(), group);
    }
    column->addStretch(1);
    setWidget(content);  // übernimmt den Besitz und löscht den alten Inhalt
}

bool ParameterPanel::showNode(std::optional<NodeId> node) {
    // Alte Hervorhebung weg (die Gruppe kann nach einem Neuaufbau schon nicht mehr existieren).
    if (shown_) {
        if (const auto old = groups_.find(shown_->value()); old != groups_.end() && old->second != nullptr) {
            old->second->setStyleSheet({});
        }
    }
    shown_.reset();
    if (!node) {
        return false;
    }
    const auto found = groups_.find(node->value());
    if (found == groups_.end() || found->second == nullptr) {
        return false;
    }
    QGroupBox* group = found->second;
    group->setStyleSheet(QStringLiteral(
        "QGroupBox { border: 2px solid #e8a33d; border-radius: 4px; margin-top: 1.4ex; font-weight: bold; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; color: #e8a33d; }"));
    shown_ = node;
    ensureWidgetVisible(group, 0, 24);
    return true;
}

}  // namespace whitehole::studio
