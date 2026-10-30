// project_fixture_parser.cpp: Matrix, Strip, Gitter, Punkte und Patches eines Fixtures.
#include "project_fixture_parser.hpp"

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
        } else if (r.ok()) {
            r.fail("Unbekannte Option \"" + key + "\" bei grid");
        }
    }
    if (r.ok()) {
        setShape(r, d, g);
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

FixtureDraft beginFixture(Reader& r) {
    FixtureDraft d;
    d.line = r.line();
    d.name = r.word("Name");
    if (r.ok() && !r.atEnd()) {
        if (r.word("at") != "at" && r.ok()) {
            r.fail("Erwartet \"at\"");
        }
        d.origin = r.vec3("Ursprung");
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
        return Error{e.code, "Zeile " + std::to_string(draft.line) + " (fixture \"" + draft.name +
                                 "\"): " + e.message};
    };
    for (const auto& f : project.layout.fixtures()) {
        if (f.name == draft.name) {
            return at(Error{ErrorCode::AlreadyExists, "Name ist schon vergeben"});
        }
    }
    if (!draft.shape && draft.points.empty()) {
        return at(Error{ErrorCode::InvalidArgument, "Es fehlt die Form (matrix, strip, grid oder point)"});
    }
    mapping::FixtureShape shape =
        draft.shape ? std::move(*draft.shape) : mapping::FixtureShape{mapping::PointsShape{draft.points}};
    const auto id = project.layout.add(draft.name, draft.origin, std::move(shape));
    if (!id.ok()) {
        return at(id.error());
    }
    for (output::FixturePatch p : draft.patches) {
        p.fixture = id.value();
        project.patches.push_back(p);
    }
    return std::nullopt;
}

}  // namespace whitehole::project::detail
