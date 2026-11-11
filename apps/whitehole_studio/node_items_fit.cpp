// node_items_fit.cpp: welche Ports ein gerade gezogenes Kabel treffen darf (Ausgrauen im Editor).
//
// Warum es diese Datei gibt: Ob zwei Ports zusammenpassen, entscheidet graph::isConnectable und lässt sich an Farbe
// und Form nicht ablesen (ein Boolean-Ausgang darf zum Beispiel an ein Zahlenfeld, ein Trigger an einen Schalter).
// Während man ein Kabel zieht, bekommt deshalb jeder Port einen Zustand (`PortSlot::fit`): passend, passend nach
// Umwandlung (etwas dezenter) oder nicht passend (abgedunkelt). Die Entscheidung kommt aus derselben Funktion wie beim
// Loslassen, Anzeige und Verhalten können also nicht auseinanderlaufen. Reine Anzeige.
#include "node_items.hpp"

#include "whitehole/graph/port_type.hpp"

namespace whitehole::studio {

void NodeItem::setWireFit(graph::PortType type, bool fromOutput, bool ownNode, const std::string& startKey) {
    for (PortSlot& port : ports_) {
        if (ownNode) {
            // Ein Kabel an denselben Node wird abgelehnt; der Startport selbst bleibt normal sichtbar.
            port.fit = port.key == startKey && port.output == fromOutput ? WireFit::Exact : WireFit::No;
        } else if (port.output == fromOutput) {
            port.fit = WireFit::No;  // Ausgang an Ausgang oder Eingang an Eingang
        } else {
            const graph::PortType from = fromOutput ? type : port.type;
            const graph::PortType to = fromOutput ? port.type : type;
            port.fit = from == to ? WireFit::Exact : (graph::isConnectable(from, to) ? WireFit::Convert : WireFit::No);
        }
    }
    hoverKey_.clear();
    update();
}

void NodeItem::clearWireFit() {
    bool changed = !hoverKey_.empty();
    for (PortSlot& port : ports_) {
        changed = changed || port.fit != WireFit::Off;
        port.fit = WireFit::Off;
    }
    hoverKey_.clear();
    if (changed) {
        update();
    }
}

void NodeItem::setWireHover(const std::string& portKey) {
    if (hoverKey_ != portKey) {
        hoverKey_ = portKey;
        update();
    }
}

}  // namespace whitehole::studio
