// fixture_shape_label.cpp: Umsetzung zu fixture_shape_label.hpp.
#include "fixture_shape_label.hpp"

#include <QObject>

namespace whitehole::studio {

QString shapeKindLabel(const mapping::FixtureShape& shape) {
    if (std::holds_alternative<mapping::MatrixShape>(shape)) {
        return QObject::tr("2D-Matrix");
    }
    if (std::holds_alternative<mapping::GridShape>(shape)) {
        return QObject::tr("3D-Matrix");
    }
    if (std::holds_alternative<mapping::StripShape>(shape)) {
        return QObject::tr("Strip");
    }
    if (std::holds_alternative<mapping::RingShape>(shape)) {
        return QObject::tr("Kreis");
    }
    if (std::holds_alternative<mapping::HelixShape>(shape)) {
        return QObject::tr("Spirale");
    }
    if (std::holds_alternative<mapping::CylinderShape>(shape)) {
        return QObject::tr("Säule");
    }
    return QObject::tr("Punktwolke");
}

}  // namespace whitehole::studio
