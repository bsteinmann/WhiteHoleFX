// fixture_library_user.cpp: Teil von FixtureLibraryPage: Abgleich mit der Benutzerbibliothek auf der Festplatte.
//
// Warum eine eigene Datei: fixture_library_page.cpp bleibt so unter der Größenmarke, und alles, was die Festplatte
// berührt, steht an einer Stelle (Spiegeln beim Bearbeiten, Ergänzen auf Knopfdruck).
#include <QString>

#include <algorithm>

#include "fixture_library_page.hpp"
#include "form_helpers.hpp"
#include "user_library.hpp"

namespace whitehole::studio {

using form::fromStd;

void FixtureLibraryPage::mirrorToUserLibrary(const QString& name) {
    if (player_.project() == nullptr) {
        return;
    }
    const std::string key = name.toUtf8().toStdString();
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        if (type.name == key) {
            const Result<void> saved = userlib::upsert(player_.project()->graph.registryPtr(), type);
            if (!saved.ok()) {
                Q_EMIT failed(fromStd(saved.error().message));
            }
            return;
        }
    }
}

void FixtureLibraryPage::mirrorRemoval(const QString& name) {
    if (player_.project() == nullptr) {
        return;
    }
    const Result<void> saved = userlib::remove(player_.project()->graph.registryPtr(), name.toUtf8().toStdString());
    if (!saved.ok()) {
        Q_EMIT failed(fromStd(saved.error().message));
    }
}

void FixtureLibraryPage::mirrorRename(const QString& from, const QString& to) {
    if (player_.project() == nullptr) {
        return;
    }
    const Result<void> saved =
        userlib::rename(player_.project()->graph.registryPtr(), from.toUtf8().toStdString(), to.toUtf8().toStdString());
    if (!saved.ok()) {
        Q_EMIT failed(fromStd(saved.error().message));
    }
}

int FixtureLibraryPage::importFromUserLibrary() {
    if (player_.project() == nullptr) {
        Q_EMIT projectNeeded();
        if (player_.project() == nullptr) {
            return 0;
        }
    }
    const auto stored = userlib::load(player_.project()->graph.registryPtr());
    if (!stored.ok()) {
        Q_EMIT failed(fromStd(stored.error().message));
        return 0;
    }
    int added = 0;
    QString last;
    for (const project::FixtureType& type : stored.value()) {
        const auto& have = player_.fixtureTypes();
        if (std::ranges::any_of(have, [&](const project::FixtureType& t) { return t.name == type.name; })) {
            continue;
        }
        const Result<void> result = player_.addFixtureType(type.name, type.shape);
        if (!result.ok()) {
            Q_EMIT failed(fromStd(result.error().message));
            break;
        }
        ++added;
        last = fromStd(type.name);
    }
    if (added > 0) {
        editing_ = true;
        fillList(last);
        editing_ = false;
        Q_EMIT edited();
    }
    return added;
}

}  // namespace whitehole::studio
