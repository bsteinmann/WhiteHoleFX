// node_editor_live.cpp: Live-Anzeige im Node-Editor (Pegelbalken in den Nodes, leuchtende Kabel).
//
// Warum es das gibt: Beim Einstellen von Audio-, LFO- und Mathe-Nodes sieht man sonst nicht, was an den Ausgängen
// ankommt. Das Hauptfenster ruft refreshLive() nach jedem Frame; der Editor holt die Werte vom Player und gibt sie
// an die Nodes (Balken) und Kabel (Helligkeit) weiter. Nur Ausgänge von CPU-Nodes haben Werte; Bild-Nodes bleiben
// unverändert. Ein Gruppen-Kachel zeigt die Werte der Mitglieder an ihren herausgeführten Ausgängen.
#include <QSettings>

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

#include "node_editor.hpp"

namespace whitehole::studio {

namespace {

constexpr const char* kLiveKey = "editor/live";

std::string keyOf(std::uint64_t node, const std::string& port) { return std::to_string(node) + '/' + port; }

/// Linear nach sRGB (grob), damit die Farbe so aussieht wie auf den LEDs und in der Vorschau.
double toDisplay(float linear) { return std::sqrt(std::clamp(static_cast<double>(linear), 0.0, 1.0)); }

LiveDisplay displayOf(const runtime::LiveSignal& signal, graph::PortType type, runtime::LiveRange& range) {
    LiveDisplay display;
    switch (signal.kind) {
        case runtime::LiveSignalKind::Number:
            display.kind = LiveDisplay::Kind::Level;
            display.level = range.update(signal.number);
            break;
        case runtime::LiveSignalKind::Bars: {
            display.kind = LiveDisplay::Kind::Bars;
            float peak = 0.0F;
            for (std::size_t i = 0; i < display.bars.size(); ++i) {
                // Wellen laufen von -1 bis 1: der Betrag zeigt die Aussteuerung.
                display.bars[i] = std::min(1.0F, std::abs(signal.bars.bands[i]));
                peak = std::max(peak, display.bars[i]);
            }
            display.level = peak;
            (void)type;
            break;
        }
        case runtime::LiveSignalKind::Monitor:
            break;  // gehört zum Node, nicht zu einem Ausgang (refreshLive)
        case runtime::LiveSignalKind::Color:
            display.kind = LiveDisplay::Kind::Color;
            display.color = QColor::fromRgbF(toDisplay(signal.color.r), toDisplay(signal.color.g), toDisplay(signal.color.b));
            display.level = std::max({signal.color.r, signal.color.g, signal.color.b});
            display.level = std::clamp(display.level, 0.0F, 1.0F);
            break;
    }
    return display;
}

/// Überträgt den Anzeigestreifen eines Nodes (Spektrum, Band-Bereich, Welle) aus der Signalliste.
void refreshMonitor(NodeItem* item, const std::unordered_map<std::string, const runtime::LiveSignal*>& byPort) {
    const auto found = byPort.find(keyOf(item->nodeId().value(), runtime::kMonitorKey));
    if (found == byPort.end()) {
        item->clearMonitor();
        return;
    }
    const runtime::LiveSignal& signal = *found->second;
    MonitorDisplay display;
    display.active = true;
    display.bipolar = signal.bipolar;
    display.first = signal.rangeFirst;
    display.last = signal.rangeLast;
    for (std::size_t i = 0; i < display.bars.size(); ++i) {
        const float value = signal.bars.bands[i];
        display.bars[i] = signal.bipolar ? std::clamp(value, -1.0F, 1.0F) : std::clamp(value, 0.0F, 1.0F);
    }
    item->setMonitor(display);
}

}  // namespace

bool NodeEditor::liveEnabled() const noexcept { return liveEnabled_; }

void NodeEditor::setLiveEnabled(bool on) {
    liveEnabled_ = on;
    QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}.setValue(QString::fromLatin1(kLiveKey), on);
    if (!on) {
        clearLive();
    }
}

void NodeEditor::loadLiveSetting() {
    liveEnabled_ = QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}
                       .value(QString::fromLatin1(kLiveKey), true)
                       .toBool();
}

void NodeEditor::clearLive() {
    if (!liveShown_) {
        return;
    }
    liveShown_ = false;
    liveRanges_.clear();
    for (const auto& entry : nodes_) {
        entry.second->clearLive();
    }
    for (WireItem* wire : wires_) {
        wire->syncLive();
    }
}

void NodeEditor::refreshLive() {
    if (!liveEnabled_) {
        return;
    }
    const std::vector<runtime::LiveSignal> signals = player_.liveSignals();
    if (signals.empty()) {
        clearLive();  // gestoppt oder noch kein Frame: keine Werte, die man zeigen könnte
        return;
    }
    std::unordered_map<std::string, const runtime::LiveSignal*> byPort;
    byPort.reserve(signals.size());
    for (const runtime::LiveSignal& signal : signals) {
        byPort.emplace(keyOf(signal.node.value(), signal.portKey), &signal);
    }
    for (const auto& entry : nodes_) {
        NodeItem* item = entry.second;
        if (item->hasMonitor()) {
            refreshMonitor(item, byPort);
        }
        for (const PortSlot& port : item->ports()) {
            if (!port.output) {
                continue;
            }
            // Eine Gruppen-Kachel steht für einen Port eines Mitglieds (realNode/realKey).
            const std::uint64_t node = port.realNode != 0 ? port.realNode : item->nodeId().value();
            const std::string& key = port.realNode != 0 ? port.realKey : port.key;
            const std::string id = keyOf(node, key);
            const auto found = byPort.find(id);
            if (found == byPort.end()) {
                if (port.live.kind != LiveDisplay::Kind::None) {
                    item->setLive(port.key, LiveDisplay{});
                }
                continue;
            }
            item->setLive(port.key, displayOf(*found->second, port.type, liveRanges_[id]));
        }
    }
    liveShown_ = true;
    for (WireItem* wire : wires_) {
        wire->syncLive();
    }
}

}  // namespace whitehole::studio
