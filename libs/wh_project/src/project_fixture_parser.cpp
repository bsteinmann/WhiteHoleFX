// project_fixture_parser.cpp: Matrix, Strip, Gitter, Punkte und Patches eines Fixtures.
#include "project_fixture_parser.hpp"

#include <algorithm>

#include "project_names.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::project::detail {

namespace {

constexpr std::uint32_t kMaxDimension = 1U << 22;

void setShape(Reader& r, FixtureDraft& d, mapping::FixtureShape shape) {
    if (d.shape || !d.points.empty()) {
        r.fail("Das Fixture hat schon eine Form");
        return;
    }
    d.shape = std::move(shape);
}

void matrix(Reader& r, FixtureDraft& d) {
    mapping::MatrixShape m;
    m.columns = r.whole("Spaltenzahl", kMaxDimension);
    m.rows = r.whole("Zeilenzahl", kMaxDimension);
    while (r.ok() && !r.atEnd()) {
        const std::string key = r.word("Option");
        if (key == "pitch") {
            m.pitch = r.vec2("Abstand");
        } else if (key == "start") {
            m.wiring.start = readName(r, kCorners, "Startecke");
        } else if (key == "first") {
            m.wiring.first = readName(r, kFirstAxes, "Laufrichtung");
        } else if (key == "serpentine") {
            m.wiring.serpentine = true;
        } else if (r.ok()) {
            r.fail("Unbekannte Option \"" + key + "\" bei matrix");
        }
    }
    if (r.ok()) {
        setShape(r, d, m);
    }
}

void strip(Reader& r, FixtureDraft& d) {
    mapping::StripShape s;
    s.count = r.whole("LED-Zahl", kMaxDimension);
    if (r.word("from") != "from" && r.ok()) {
        r.fail("Erwartet \"from\"");
    }
    s.start = r.vec3("Start");
    if (r.word("to") != "to" && r.ok()) {
        r.fail("Erwartet \"to\"");
    }
    s.end = r.vec3("Ende");
    r.expectEnd();
    if (r.ok()) {
        setShape(r, d, s);
    }
}

void grid(Reader& r, FixtureDraft& d) {
    mapping::GridShape g;
    g.nx = r.whole("nx", kMaxDimension);
    g.ny = r.whole("ny", kMaxDimension);
    g.nz = r.whole("nz", kMaxDimension);
    while (r.ok() && !r.atEnd()) {
        const std::string key = r.word("Option");
        if (key == "pitch") {
            g.pitch = r.vec3("Abstand");
        } else if (key == "order") {
            g.wiring.order = readName(r, kAxisOrders, "Achsenfolge");
        } else if (key == "serpentine") {
            g.wiring.serpentine = true;
        } else if (key == "flipx") {
            g.wiring.flipX = true;
        } else if (key == "flipy") {
            g.wiring.flipY = true;
        } else if (key == "flipz") {
            g.wiring.flipZ = true;
        } else if (r.ok()) {
            r.fail("Unbekannte Option \"" + key + "\" bei grid");
        }
    }
    if (r.ok()) {
        setShape(r, d, g);
    }
}

void ring(Reader& r, FixtureDraft& d) {
    mapping::RingShape s;
    s.count = r.whole("LED-Zahl", kMaxDimension);
    while (r.ok() && !r.atEnd()) {
        const std::string key = r.word("Option");
        if (key == "radius") {
            s.radius = r.number("Radius");
        } else if (key == "start") {
            s.startAngle = r.number("Startwinkel");
        } else if (key == "ccw") {
            s.clockwise = false;
        } else if (r.ok()) {
            r.fail("Unbekannte Option \"" + key + "\" bei ring");
        }
    }
    if (r.ok()) {
        setShape(r, d, s);
    }
}

void helix(Reader& r, FixtureDraft& d) {
    mapping::HelixShape s;
    s.count = r.whole("LED-Zahl", kMaxDimension);
    while (r.ok() && !r.atEnd()) {
        const std::string key = r.word("Option");
        if (key == "radius") {
            s.radius = r.number("Radius");
        } else if (key == "height") {
            s.height = r.number("Höhe");
        } else if (key == "turns") {
            s.turns = r.number("Windungen");
        } else if (key == "start") {
            s.startAngle = r.number("Startwinkel");
        } else if (key == "ccw") {
            s.clockwise = false;
        } else if (r.ok()) {
            r.fail("Unbekannte Option \"" + key + "\" bei helix");
        }
    }
    if (r.ok()) {
        setShape(r, d, s);
    }
}

void cylinder(Reader& r, FixtureDraft& d) {
    mapping::CylinderShape s;
    s.perRing = r.whole("LEDs je Ring", kMaxDimension);
    s.rings = r.whole("Ringe", kMaxDimension);
    while (r.ok() && !r.atEnd()) {
        const std::string key = r.word("Option");
        if (key == "radius") {
            s.radius = r.number("Radius");
        } else if (key == "height") {
            s.height = r.number("Höhe");
        } else if (key == "start") {
            s.startAngle = r.number("Startwinkel");
        } else if (key == "ccw") {
            s.clockwise = false;
        } else if (key == "straight") {
            s.serpentine = false;
        } else if (r.ok()) {
            r.fail("Unbekannte Option \"" + key + "\" bei cylinder");
        }
    }
    if (r.ok()) {
        setShape(r, d, s);
    }
}

void point(Reader& r, FixtureDraft& d) {
    const Vec3 p = r.vec3("Punkt");
    r.expectEnd();
    if (!r.ok()) {
        return;
    }
    if (d.shape) {
        r.fail("Das Fixture hat schon eine Form");
        return;
    }
    d.points.push_back(p);
}

void patch(Reader& r, FixtureDraft& d) {
    output::FixturePatch p;
    p.protocol = readName(r, kProtocols, "Protokoll");
    bool haveDestination = false;
    while (r.ok() && !r.atEnd()) {
        const std::string key = r.word("Option");
        if (key == "destination") {
            p.destination = r.whole("Ziel", 65535);
            haveDestination = true;
        } else if (key == "universe") {
            p.universe = static_cast<std::uint16_t>(r.whole("Universe", 65535));
        } else if (key == "channel") {
            p.channel = static_cast<std::uint16_t>(r.whole("Kanal", 65535));
        } else if (key == "offset") {
            p.ddpOffset = r.whole("Offset", 0xFFFFFFFFU);
        } else if (key == "order") {
            p.color.order = readName(r, kColorOrders, "Farbreihenfolge");
        } else if (key == "skip-first") {
            p.skipFirst = static_cast<std::uint16_t>(r.whole("Pixel vor der ersten LED", 0xFFFFU));
        } else if (key == "skip-every") {
            p.skipEvery = r.whole("LEDs zwischen übersprungenen Pixeln", 0xFFFFFFFFU);
        } else if (key == "skip-count") {
            p.skipCount = static_cast<std::uint16_t>(r.whole("übersprungene Pixel", 0xFFFFU));
        } else if (key == "gamma") {
            p.color.gamma = r.number("Gamma");
        } else if (key == "brightness") {
            p.color.brightness = r.number("Helligkeit");
        } else if (r.ok()) {
            r.fail("Unbekannte Option \"" + key + "\" bei patch");
        }
    }
    if (r.ok() && !haveDestination) {
        r.fail("patch braucht \"destination <Nummer>\"");
    }
    if (r.ok()) {
        d.patches.push_back(p);
    }
}

}  // namespace

FixtureDraft beginFixture(Reader& r, bool isType) {
    FixtureDraft d;
    d.line = r.line();
    d.isType = isType;
    d.name = r.word("Name");
    while (r.ok() && !r.atEnd() && !isType) {
        const std::string key = r.word("Option");
        if (key == "at") {
            d.origin = r.vec3("Ursprung");
        } else if (key == "rotate") {
            d.rotation = r.vec3("Drehung");
        } else if (key == "type") {
            d.type = r.word("Typname");
        } else if (r.ok()) {
            r.fail("Erwartet \"at\", \"rotate\" oder \"type\", nicht \"" + key + "\"");
        }
    }
    r.expectEnd();
    return d;
}

void fixtureStatement(const std::string& keyword, Reader& r, FixtureDraft& draft) {
    if (keyword == "matrix") {
        matrix(r, draft);
    } else if (keyword == "strip") {
        strip(r, draft);
    } else if (keyword == "grid") {
        grid(r, draft);
    } else if (keyword == "ring") {
        ring(r, draft);
    } else if (keyword == "helix") {
        helix(r, draft);
    } else if (keyword == "cylinder") {
        cylinder(r, draft);
    } else if (keyword == "point") {
        point(r, draft);
    } else if (keyword == "patch") {
        patch(r, draft);
    } else {
        r.fail("Unbekannte Anweisung \"" + keyword + "\" im fixture-Block");
    }
    r.expectEnd();
}

std::optional<Error> finishFixture(FixtureDraft& draft, Project& project) {
    const auto at = [&](const Error& e) {
        return Error{e.code, "Zeile " + std::to_string(draft.line) + " (" + (draft.isType ? "fixturetype" : "fixture") +
                                 " \"" + draft.name + "\"): " + e.message};
    };
    const bool hasShape = draft.shape.has_value() || !draft.points.empty();
    if (draft.isType) {
        for (const FixtureType& other : project.types) {
            if (other.name == draft.name) {
                return at(Error{ErrorCode::AlreadyExists, "Name ist schon vergeben"});
            }
        }
        if (!hasShape) {
            return at(Error{ErrorCode::InvalidArgument, "Es fehlt die Form (matrix, strip, grid, ring, helix, cylinder oder point)"});
        }
        if (!draft.patches.empty()) {
            return at(Error{ErrorCode::InvalidArgument, "Ein fixturetype hat keinen Patch"});
        }
        mapping::FixtureShape shape =
            draft.shape ? std::move(*draft.shape) : mapping::FixtureShape{mapping::PointsShape{draft.points}};
        if (const auto valid = mapping::validateShape(shape); !valid.ok()) {
            return at(valid.error());
        }
        project.types.push_back(FixtureType{draft.name, std::move(shape)});
        return std::nullopt;
    }
    for (const auto& f : project.layout.fixtures()) {
        if (f.name == draft.name) {
            return at(Error{ErrorCode::AlreadyExists, "Name ist schon vergeben"});
        }
    }
    mapping::FixtureShape shape;
    if (!draft.type.empty()) {
        if (hasShape) {
            return at(Error{ErrorCode::InvalidArgument, "Mit \"type\" hat das Fixture keine eigene Form"});
        }
        const auto found = std::ranges::find(project.types, draft.type, &FixtureType::name);
        if (found == project.types.end()) {
            return at(Error{ErrorCode::NotFound, "Der Typ \"" + draft.type + "\" ist nicht definiert (fixturetype davor)"});
        }
        shape = found->shape;
    } else if (!hasShape) {
        return at(Error{ErrorCode::InvalidArgument, "Es fehlt die Form (matrix, strip, grid, ring, helix, cylinder oder point)"});
    } else {
        shape = draft.shape ? std::move(*draft.shape) : mapping::FixtureShape{mapping::PointsShape{draft.points}};
    }
    const auto id = project.layout.add(draft.name, draft.origin, std::move(shape));
    if (!id.ok()) {
        return at(id.error());
    }
    if (const auto rotated = project.layout.rotate(id.value(), draft.rotation); !rotated.ok()) {
        return at(rotated.error());
    }
    (void)project.layout.setType(id.value(), draft.type);
    for (output::FixturePatch p : draft.patches) {
        p.fixture = id.value();
        project.patches.push_back(p);
    }
    return std::nullopt;
}

}  // namespace whitehole::project::detail
