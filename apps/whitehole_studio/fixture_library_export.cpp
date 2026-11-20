// fixture_library_export.cpp: Export/Import einzelner Fixtures als eigene Datei (Reiter Fixtures).
//
// Warum eine eigene Datei: fixture_library_page.cpp liegt mit Liste, Formular und den übrigen Knöpfen schon nahe
// an der 400-Zeilen-Grenze (siehe CLAUDE.md); Export und Import sind von den anderen Methoden der Seite unabhängig
// und passen daher gut in eine eigene Datei. Es gibt dafür keine automatisch geladene Bibliothek mehr - der Pfad
// wird jedes Mal per Dateidialog frei gewählt (project::writeFixtureExport/parseFixtureExport übernehmen das
// eigentliche Format, dieselbe Schreibweise wie die Projektdatei).
#include "fixture_library_page.hpp"

#include <QByteArray>
#include <QFile>
#include <QSaveFile>

#include <string_view>

#include "form_helpers.hpp"

namespace whitehole::studio {

using form::fromStd;

bool FixtureLibraryPage::exportSelectedTo(const QString& path) {
    const QString name = selectedName();
    if (name.isEmpty()) {
        return false;
    }
    for (const project::FixtureType& type : player_.fixtureTypes()) {
        if (fromStd(type.name) != name) {
            continue;
        }
        QSaveFile out{path};
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            Q_EMIT failed(tr("Datei lässt sich nicht schreiben: %1").arg(path));
            return false;
        }
        out.write(QByteArray::fromStdString(project::writeFixtureExport({type})));
        if (!out.commit()) {
            Q_EMIT failed(tr("Datei lässt sich nicht speichern: %1").arg(path));
            return false;
        }
        Q_EMIT notice(tr("Fixture „%1“ exportiert nach: %2").arg(name, path));
        return true;
    }
    return false;
}

int FixtureLibraryPage::importFrom(const QString& path) {
    if (player_.project() == nullptr) {
        Q_EMIT projectNeeded();
        if (player_.project() == nullptr) {
            return -1;
        }
        (void)player_.dropSoleFixtureType();  // die Bauform der Vorlage soll nicht neben den importierten stehen
    }
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly)) {
        Q_EMIT failed(tr("Datei lässt sich nicht lesen: %1").arg(path));
        return -1;
    }
    const QByteArray bytes = file.readAll();
    const auto parsed = project::parseFixtureExport(
        std::string_view{bytes.constData(), static_cast<std::size_t>(bytes.size())}, player_.registry());
    if (!parsed.ok()) {
        Q_EMIT failed(fromStd(parsed.error().message));
        return -1;
    }
    int added = 0;
    QString last;
    for (const project::FixtureType& type : parsed.value()) {
        const QString name = uniqueName(fromStd(type.name));
        const Result<void> result = player_.addFixtureType(name.toUtf8().toStdString(), type.shape);
        if (!result.ok()) {
            Q_EMIT failed(fromStd(result.error().message));
            break;
        }
        ++added;
        last = name;
    }
    if (added > 0) {
        editing_ = true;
        fillList(last);
        editing_ = false;
        Q_EMIT notice(added == 1 ? tr("1 Fixture importiert.") : tr("%1 Fixtures importiert.").arg(added));
        Q_EMIT edited();
    }
    return added;
}

}  // namespace whitehole::studio
