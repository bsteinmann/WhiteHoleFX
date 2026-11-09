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
#include <QScrollArea>
#include <QSettings>
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
    factor->setKeyboardTracking(false);  // übernimmt erst bei Enter (Issue #22)
    factor->setPrefix(QStringLiteral("× "));
    factor->setValue(info.scale);
    auto* offset = new QDoubleSpinBox{row};
    offset->setRange(-graph::kMaxConnectionScale, graph::kMaxConnectionScale);
    offset->setDecimals(4);
    offset->setKeyboardTracking(false);
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

ParameterPanel::ParameterPanel(QWidget* parent) : QWidget{parent} {
    auto* outer = new QVBoxLayout{this};
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(2);
    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(4, 4, 4, 0);
    modeBox_ = new QComboBox{this};
    modeBox_->addItem(tr("Gewählter Node"));
    modeBox_->addItem(tr("Alle Nodes"));
    modeBox_->setToolTip(tr("Gewählter Node: nur der Node, den du im Editor gewählt hast, und angeheftete Nodes. "
                            "Alle Nodes: jede Gruppe, aufgeklappt nur die des gewählten und der angehefteten Nodes."));
    search_ = new QLineEdit{this};
    search_->setPlaceholderText(tr("Suchen …"));
    search_->setClearButtonEnabled(true);
    search_->setToolTip(tr("Filtert die Gruppen nach Node- oder Parametername (über alle Nodes)"));
    bar->addWidget(modeBox_);
    bar->addWidget(search_, 1);
    outer->addLayout(bar);
    scroll_ = new QScrollArea{this};
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    auto* hint = new QLabel{tr("Kein Projekt geladen."), scroll_};
    hint->setMargin(12);
    scroll_->setWidget(hint);
    outer->addWidget(scroll_, 1);

    QSettings settings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")};
    mode_ = settings.value(QStringLiteral("parameters/mode")).toString() == QStringLiteral("all") ? Mode::All : Mode::Selected;
    modeBox_->setCurrentIndex(mode_ == Mode::All ? 1 : 0);
    connect(modeBox_, &QComboBox::currentIndexChanged, this, [this](int index) { setMode(index == 1 ? Mode::All : Mode::Selected); });
    connect(search_, &QLineEdit::textChanged, this, [this]() { applyVisibility(); });
}

void ParameterPanel::setMode(Mode mode) {
    if (mode == mode_) {
        return;
    }
    mode_ = mode;
    modeBox_->setCurrentIndex(mode == Mode::All ? 1 : 0);  // Rückruf ruft setMode erneut auf und endet dort (gleicher Wert)
    QSettings settings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")};
    settings.setValue(QStringLiteral("parameters/mode"), mode == Mode::All ? QStringLiteral("all") : QStringLiteral("selected"));
    applyVisibility();
}

void ParameterPanel::setPinned(NodeId node, bool pinned) {
    if (pinned) {
        pinned_.insert(node.value());
    } else {
        pinned_.erase(node.value());
    }
    if (const auto found = entries_.find(node.value()); found != entries_.end() && found->second.pin != nullptr) {
        const QSignalBlocker blocker{found->second.pin};
        found->second.pin->setChecked(pinned);
    }
    applyVisibility();
}

bool ParameterPanel::groupShown(NodeId node) const {
    const auto found = entries_.find(node.value());
    return found != entries_.end() && found->second.group != nullptr && !found->second.group->isHidden();
}

bool ParameterPanel::groupOpen(NodeId node) const {
    const auto found = entries_.find(node.value());
    return found != entries_.end() && found->second.body != nullptr && !found->second.body->isHidden();
}

void ParameterPanel::applyVisibility() {
    const QString needle = search_->text().trimmed().toLower();
    int visibleGroups = 0;
    for (auto& [id, entry] : entries_) {
        if (entry.group == nullptr) {
            continue;
        }
        const bool selected = shown_ && shown_->value() == id;
        const bool isPinned = pinned_.contains(id);
        const bool searching = !needle.isEmpty();
        const bool visible = searching ? entry.text.contains(needle) : (mode_ == Mode::All || selected || isPinned);
        bool open = searching || mode_ == Mode::Selected || selected || isPinned;
        if (const auto manual = open_.find(id); manual != open_.end() && !searching) {
            open = manual->second;
        }
        entry.group->setVisible(visible);
        entry.body->setVisible(open);
        entry.arrow->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        visibleGroups += visible ? 1 : 0;
    }
    if (emptyHint_ != nullptr) {
        emptyHint_->setText(needle.isEmpty() ? tr("Wähle einen Node im Node-Editor, um seine Parameter zu sehen. "
                                                  "Mit „Anheften“ bleibt ein Node hier stehen.")
                                             : tr("Keine Treffer."));
        emptyHint_->setVisible(visibleGroups == 0 && !entries_.empty());
    }
}

void ParameterPanel::rebuild(const std::vector<runtime::NodeInfo>& nodes) {
    entries_.clear();
    open_.clear();
    shown_.reset();
    auto* content = new QWidget;
    auto* column = new QVBoxLayout{content};
    emptyHint_ = new QLabel{content};
    emptyHint_->setWordWrap(true);
    emptyHint_->setMargin(12);
    column->addWidget(emptyHint_);
    if (nodes.empty()) {
        column->addWidget(new QLabel{tr("Dieses Projekt hat keine einstellbaren Parameter."), content});
    }
    for (const runtime::NodeInfo& node : nodes) {
        auto* group = new QGroupBox{fromStd(node.title), content};
        auto* groupLayout = new QVBoxLayout{group};
        auto* body = new QWidget{group};
        auto* grid = new QGridLayout{body};
        grid->setContentsMargins(0, 0, 0, 0);
        auto* header = new QHBoxLayout;
        auto* arrow = new QToolButton{group};
        arrow->setAutoRaise(true);
        arrow->setArrowType(Qt::DownArrow);
        arrow->setToolTip(tr("Parameter dieses Nodes auf- oder zuklappen"));
        auto* pin = new QToolButton{group};
        pin->setText(tr("Anheften"));
        pin->setCheckable(true);
        pin->setChecked(pinned_.contains(node.id.value()));
        pin->setToolTip(tr("Hält diesen Node im Fenster, auch wenn du im Editor einen anderen wählst"));
        header->addWidget(arrow);
        header->addStretch(1);
        header->addWidget(pin);
        groupLayout->setContentsMargins(6, 6, 6, 6);
        groupLayout->addLayout(header);
        groupLayout->addWidget(body);
        QString searchText = fromStd(node.title);
        const std::uint64_t nodeKey = node.id.value();
        QObject::connect(arrow, &QToolButton::clicked, this, [this, nodeKey]() {
            const auto found = entries_.find(nodeKey);
            if (found != entries_.end() && found->second.body != nullptr) {
                open_[nodeKey] = found->second.body->isHidden();  // umkehren
                applyVisibility();
            }
        });
        QObject::connect(pin, &QToolButton::toggled, this, [this, nodeId = node.id](bool on) { setPinned(nodeId, on); });
        int row = 0;
        if (node.parameters.empty()) {
            grid->addWidget(new QLabel{tr("Dieser Node hat keine einstellbaren Parameter."), body}, row++, 0);
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
            *holder = makeEditor(info, commit, body, sliderRange);

            searchText += QLatin1Char{' '} + fromStd(info.displayName) + QLatin1Char{' '} + fromStd(info.key);
            auto* label = new QLabel{fromStd(info.displayName), body};
            auto* reset = new QToolButton{body};
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
                grid->addWidget(makeScaleRow(info, body), row, 0, 1, 3);
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
        entries_[node.id.value()] = Entry{group, body, arrow, pin, searchText.toLower()};
    }
    column->addStretch(1);
    scroll_->setWidget(content);  // übernimmt den Besitz und löscht den alten Inhalt
    applyVisibility();
}

bool ParameterPanel::showNode(std::optional<NodeId> node) {
    // Alte Hervorhebung weg (die Gruppe kann nach einem Neuaufbau schon nicht mehr existieren).
    if (shown_) {
        if (const auto old = entries_.find(shown_->value()); old != entries_.end() && old->second.group != nullptr) {
            old->second.group->setStyleSheet({});
        }
    }
    shown_.reset();
    if (!node) {
        applyVisibility();
        return false;
    }
    const auto found = entries_.find(node->value());
    if (found == entries_.end() || found->second.group == nullptr) {
        applyVisibility();
        return false;
    }
    QGroupBox* group = found->second.group;
    group->setStyleSheet(QStringLiteral(
        "QGroupBox { border: 2px solid #e8a33d; border-radius: 4px; margin-top: 1.4ex; font-weight: bold; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; color: #e8a33d; }"));
    shown_ = node;
    open_.erase(node->value());  // der neu gewählte Node klappt wieder nach der Regel der Ansicht auf
    applyVisibility();
    scroll_->ensureWidgetVisible(group, 0, 24);
    return true;
}

}  // namespace whitehole::studio
