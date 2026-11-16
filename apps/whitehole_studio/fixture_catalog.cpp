// fixture_catalog.cpp: Umsetzung zu fixture_catalog.hpp.
#include "fixture_catalog.hpp"

#include <algorithm>

#include "form_helpers.hpp"
#include "user_library.hpp"

namespace whitehole::studio {

using form::fromStd;

std::vector<FixtureEntry> mergedFixtures(const runtime::Player& player) {
    std::vector<FixtureEntry> entries;
    for (const project::FixtureType& type : player.fixtureTypes()) {
        entries.push_back({fromStd(type.name), type.shape, true, false});
    }
    const auto stored = userlib::load(player.registry());
    if (stored.ok()) {
        for (const project::FixtureType& type : stored.value()) {
            const QString name = fromStd(type.name);
            const auto found = std::ranges::find(entries, name, &FixtureEntry::name);
            if (found != entries.end()) {
                found->inLibrary = true;
            } else {
                entries.push_back({name, type.shape, false, true});
            }
        }
    }
    std::ranges::sort(entries, {}, &FixtureEntry::name);
    return entries;
}

}  // namespace whitehole::studio
