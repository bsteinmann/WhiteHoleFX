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
#include "whitehole/runtime/connection_scale.hpp"

namespace whitehole::studio {

namespace {

QString fromStd(const std::string& text) { return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size())); }

}  // namespace

QWidget* ParameterPanel::makeScaleRow(const runtime::ParameterInfo& info, QWidget* parent) {
    auto* row = new QWidget{parent};
    auto* layout = new QHBoxLayout{row};
    layout->setContentsMargins(12, 0, 0, 0);
    const QString help = tr("Skalierung am Kabel: Wert × Faktor + Offset, danach begrenzt auf den Bereich des Parameters. "
                            "So steuert ein Wert von 0 bis 1 zum Beispiel einen Winkel von 0 bis 360.");
    auto* title = new QLabel{tr("Kabel:"), row};
    auto* factor = new QDoubleSpinBox{row};
    factor->setRange(-graph::kMaxConnectionScale, graph::kMaxConnectionScale);
    factor->setDecimals(4);
    factor->setPrefix(QStringLiteral("× "));
    factor->setValue(info.scale);
    auto* offset = new QDoubleSpinBox{row};
    offset->setRange(-graph::kMaxConnectionScale, graph::kMaxConnectionScale);
    offset->setDecimals(4);
    offset->setPrefix(QStringLiteral("+ "));
    offset->setValue(info.offset);
    for (QWidget* widget : {static_cast<QWidget*>(title), static_cast<QWidget*>(factor), static_cast<QWidget*>(offset)}) {
        widget->setToolTip(help);
    }
    layout->addWidget(title);
    layout->addWidget(factor);
    layout->addWidget(offset);
    const ConnectionId connection = info.connection;
    const auto apply = [this, connection, factor, offset]() {
        if (scaleHandler_) {
            scaleHandler_(connection, factor->value(), offset->value());
        }
    };
    QObject::connect(factor, &QDoubleSpinBox::editingFinished, row, apply);
    QObject::connect(offset, &QDoubleSpinBox::editingFinished, row, apply);
    if (const auto suggestion = runtime::suggestConnectionScale(info)) {
        auto* fit = new QToolButton{row};
        fit->setText(tr("An Bereich anpassen"));
        fit->setToolTip(tr("Spannt einen Wert von 0 bis 1 über den ganzen Bereich des Parameters"));
        const auto [fitFactor, fitOffset] = *suggestion;
        QObject::connect(fit, &QToolButton::clicked, row, [factor, offset, fitFactor, fitOffset, apply]() {
            factor->setValue(fitFactor);
            offset->setValue(fitOffset);
            apply();
        });
        layout->addWidget(fit);
    }
    auto* reset = new QToolButton{row};
    reset->setText(QStringLiteral("↺"));
    reset->setToolTip(tr("Skalierung zurücksetzen (× 1, + 0)"));
    QObject::connect(reset, &QToolButton::clicked, row, [factor, offset, apply]() {
        factor->setValue(1.0);
        offset->setValue(0.0);
        apply();
    });
    layout->addWidget(reset);
    layout->addStretch(1);
    return row;
}

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
        if (node.parameters.empty()) {
            grid->addWidget(new QLabel{tr("Dieser Node hat keine einstellbaren Parameter."), group}, row++, 0);
        }
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
            if (info.driven && (info.value.index() == 1 || info.value.index() == 2)) {
                grid->addWidget(makeScaleRow(info, group), row, 0, 1, 3);
                ++row;
            }
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
