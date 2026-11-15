// dmx_cell_colors.cpp: Umsetzung zu dmx_cell_colors.hpp.
#include "dmx_cell_colors.hpp"

namespace whitehole::studio {

QColor dmxCellColor(DmxCellCategory category) {
    switch (category) {
        case DmxCellCategory::Fixture:
            return QColor{173, 216, 230};  // helles Blau
        case DmxCellCategory::Pool:
            return QColor{197, 224, 165};  // helles Grün
        case DmxCellCategory::Master:
            return QColor{255, 213, 128};  // helles Orange
        case DmxCellCategory::Live:
            return QColor{206, 180, 240};  // helles Violett
        case DmxCellCategory::Node:
            return QColor{255, 236, 139};  // helles Gelb
        case DmxCellCategory::None:
        default:
            return QColor{};  // ungültig: Aufrufer setzt den normalen Hintergrund zurück
    }
}

DmxCellCategory dmxCellCategory(const runtime::DmxInputListener& listener) {
    switch (listener.kind) {
        case runtime::DmxInputListener::Kind::Master:
            return DmxCellCategory::Master;
        case runtime::DmxInputListener::Kind::LiveSelect:
        case runtime::DmxInputListener::Kind::LiveFade:
            return DmxCellCategory::Live;
        case runtime::DmxInputListener::Kind::Node:
        default:
            return DmxCellCategory::Node;
    }
}

DmxCellCategory dmxCellCategory(const runtime::DmxOutputOccupant& occupant) {
    return occupant.kind == runtime::DmxOutputOccupant::Kind::Fixture ? DmxCellCategory::Fixture : DmxCellCategory::Pool;
}

}  // namespace whitehole::studio
