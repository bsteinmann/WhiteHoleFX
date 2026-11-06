// project_stage.cpp: Gruppen, Regionen und Output-Ziele der Projektdatei (siehe project_stage.hpp).
#include "project_stage.hpp"

#include <cmath>

namespace whitehole::project::detail {

namespace {

float fraction(Reader& r, std::string_view what) {
    const float value = r.number(what);
    if (r.ok() && !(value >= 0.0F && value <= 1.0F)) {
        r.fail(std::string{what} + " liegt zwischen 0 und 1");
    }
    return value;
}

}  // namespace

FixtureGroup parseGroup(Reader& r) {
    FixtureGroup group;
    group.name = r.word("Gruppenname");
    if (r.ok() && group.name.empty()) {
        r.fail("Der Gruppenname darf nicht leer sein");
    }
    while (r.ok() && !r.atEnd()) {
        group.fixtures.push_back(r.word("Fixture-Name"));
    }
    if (r.ok() && group.fixtures.empty()) {
        r.fail("Eine Gruppe braucht mindestens ein Fixture");
    }
    return group;
}

StageRegion parseRegion(Reader& r) {
    StageRegion region;
    region.name = r.word("Regionsname");
    if (r.ok() && region.name.empty()) {
        r.fail("Der Regionsname darf nicht leer sein");
    }
    region.left = fraction(r, "links");
    region.top = fraction(r, "oben");
    region.width = fraction(r, "Breite");
    region.height = fraction(r, "Höhe");
    r.expectEnd();
    if (r.ok() && (region.width <= 0.0F || region.height <= 0.0F)) {
        r.fail("Breite und Höhe der Region müssen größer als 0 sein");
    }
    return region;
}

std::string parseTarget(Reader& r) {
    const std::string kind = r.word("Art (fixture, group oder region)");
    if (r.ok() && kind != "fixture" && kind != "group" && kind != "region") {
        r.fail("Unbekannte Art \"" + kind + "\" (fixture, group oder region)");
    }
    const std::string name = r.word("Name");
    if (r.ok() && name.empty()) {
        r.fail("Der Name des Ziels darf nicht leer sein");
    }
    r.expectEnd();
    return kind + ":" + name;
}

void writeGroup(std::ostream& out, const FixtureGroup& group) {
    out << "group " << quoteWord(group.name);
    for (const std::string& fixture : group.fixtures) {
        out << " " << quoteWord(fixture);
    }
    out << "\n";
}

void writeRegion(std::ostream& out, const StageRegion& region) {
    out << "region " << quoteWord(region.name) << " " << formatFloat(region.left) << " " << formatFloat(region.top)
        << " " << formatFloat(region.width) << " " << formatFloat(region.height) << "\n";
}

void writeTargets(std::ostream& out, const std::vector<std::string>& targets) {
    for (const std::string& target : targets) {
        const std::size_t colon = target.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        out << "  target " << target.substr(0, colon) << " " << quoteWord(target.substr(colon + 1)) << "\n";
    }
}

}  // namespace whitehole::project::detail
