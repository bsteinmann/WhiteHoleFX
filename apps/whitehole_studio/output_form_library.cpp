// output_form_library.cpp: Teil von OutputForm: die Bibliothek der Controller (Speichern, Holen, Löschen).
//
// Warum eine eigene Datei: output_form.cpp bleibt bei Tabelle und Einstellungen; hier steht alles, was die Datei im
// Benutzerverzeichnis berührt (user_library.hpp), samt Rauchtest.
#include <QAction>
#include <QByteArray>
#include <QDir>
#include <QInputDialog>
#include <QMenu>
#include <QTemporaryDir>

#include "output_form.hpp"
#include "user_library.hpp"

namespace whitehole::studio {

using form::fromStd;

namespace {

QString protocolText(output::Protocol protocol) {
    switch (protocol) {
        case output::Protocol::ArtNet: return QStringLiteral("Art-Net");
        case output::Protocol::Sacn: return QStringLiteral("sACN");
        case output::Protocol::Ddp: return QStringLiteral("DDP");
    }
    return {};
}

/// Lenkt die Controller-Bibliothek für die Dauer des Rauchtests in ein Wegwerfverzeichnis.
class ScopedControllerLibrary {
public:
    ScopedControllerLibrary()
        : had_{qEnvironmentVariableIsSet("WHITEHOLEFX_CONTROLLER_LIBRARY")}, old_{qgetenv("WHITEHOLEFX_CONTROLLER_LIBRARY")} {
        qputenv("WHITEHOLEFX_CONTROLLER_LIBRARY", dir_.filePath(QStringLiteral("controller-library.whfx")).toUtf8());
    }
    ~ScopedControllerLibrary() {
        if (had_) {
            qputenv("WHITEHOLEFX_CONTROLLER_LIBRARY", old_);
        } else {
            qunsetenv("WHITEHOLEFX_CONTROLLER_LIBRARY");
        }
    }
    ScopedControllerLibrary(const ScopedControllerLibrary&) = delete;
    ScopedControllerLibrary& operator=(const ScopedControllerLibrary&) = delete;

private:
    QTemporaryDir dir_;
    bool had_;
    QByteArray old_;
};

}  // namespace

void OutputForm::saveSelectedToLibrary() {
    const project::Project* project = player_.project();
    if (project == nullptr) {
        return;
    }
    const int row = table_->currentRow();
    if (row < 0 || static_cast<std::size_t>(row) >= project->destinations.size()) {
        Q_EMIT failed(tr("Wähle zuerst einen Controller in der Tabelle."));
        return;
    }
    if (project->destinations[static_cast<std::size_t>(row)].name.empty()) {
        // Ohne Namen lässt sich ein Controller nicht wiederfinden: nachfragen und gleich im Projekt eintragen.
        bool accepted = false;
        const QString name = QInputDialog::getText(this, tr("In Bibliothek speichern"), tr("Name des Controllers:"),
                                                   QLineEdit::Normal, QString{}, &accepted)
                                 .trimmed();
        if (!accepted || name.isEmpty()) {
            return;
        }
        table_->item(row, Name)->setText(name);  // übernimmt den Namen ins Projekt (itemChanged)
    }
    const output::Destination controller = player_.project()->destinations[static_cast<std::size_t>(row)];
    const Result<void> saved = userlib::upsertController(player_.project()->graph.registryPtr(), controller);
    if (!saved.ok()) {
        Q_EMIT failed(fromStd(saved.error().message));
        return;
    }
    Q_EMIT notice(tr("Controller „%1“ in der Bibliothek gespeichert.").arg(fromStd(controller.name)));
}

void OutputForm::addFromLibrary(const output::Destination& controller) {
    if (player_.project() == nullptr) {
        return;
    }
    std::vector<output::Destination> destinations = player_.project()->destinations;
    destinations.push_back(controller);
    const Result<void> r = player_.setDestinations(std::move(destinations));
    if (r.ok()) {
        rebuild();
    }
    report(r);
}

void OutputForm::applyFromLibrary(const output::Destination& controller) {
    if (player_.project() == nullptr) {
        return;
    }
    const int row = table_->currentRow();
    if (row < 0 || static_cast<std::size_t>(row) >= player_.project()->destinations.size()) {
        Q_EMIT failed(tr("Wähle zuerst einen Controller in der Tabelle."));
        return;
    }
    // Gleiche Nummer, neue Einstellungen: die Patches bleiben, setDestinations schiebt bei Protokollwechsel das Universe.
    std::vector<output::Destination> destinations = player_.project()->destinations;
    destinations[static_cast<std::size_t>(row)] = controller;
    const Result<void> r = player_.setDestinations(std::move(destinations));
    if (r.ok()) {
        rebuild();
        table_->setCurrentCell(row, Address);
    }
    report(r);
}

void OutputForm::fillLibraryMenu(QMenu* menu, LibraryAction mode) {
    menu->clear();
    if (player_.project() == nullptr) {
        return;
    }
    const auto controllers = userlib::loadControllers(player_.project()->graph.registryPtr());
    if (!controllers.ok()) {
        menu->addAction(fromStd(controllers.error().message))->setEnabled(false);
        return;
    }
    if (controllers.value().empty()) {
        menu->addAction(tr("(Bibliothek ist leer)"))->setEnabled(false);
        return;
    }
    for (const output::Destination& controller : controllers.value()) {
        const QString where = controller.address.empty() ? tr("Multicast") : fromStd(controller.address);
        const QString text = QStringLiteral("%1 – %2 %3").arg(fromStd(controller.name), protocolText(controller.protocol), where);
        QAction* entry = menu->addAction(text);
        if (mode == LibraryAction::Add) {
            connect(entry, &QAction::triggered, this, [this, controller]() { addFromLibrary(controller); });
        } else if (mode == LibraryAction::Apply) {
            connect(entry, &QAction::triggered, this, [this, controller]() { applyFromLibrary(controller); });
        } else {
            connect(entry, &QAction::triggered, this, [this, controller]() {
                const Result<void> removed = userlib::removeController(player_.project()->graph.registryPtr(), controller.name);
                if (!removed.ok()) {
                    Q_EMIT failed(fromStd(removed.error().message));
                }
            });
        }
    }
}

QString OutputForm::exerciseLibrary() {
    const ScopedControllerLibrary scoped;
    const std::size_t before = player_.project()->destinations.size();
    const auto registry = player_.project()->graph.registryPtr();

    // Controller anlegen, benennen, in die Bibliothek speichern (zweimal: das zweite Mal ersetzt)
    addDestination();
    const int row = static_cast<int>(before);
    table_->item(row, Name)->setText(QStringLiteral("Bibliothek-Test"));
    table_->setCurrentCell(row, Address);
    saveSelectedToLibrary();
    table_->item(row, Address)->setText(QStringLiteral("192.168.0.77"));
    saveSelectedToLibrary();
    const auto stored = userlib::loadControllers(registry);
    if (!stored.ok() || stored.value().size() != 1 || stored.value()[0].name != "Bibliothek-Test" ||
        stored.value()[0].address != "192.168.0.77") {
        return QStringLiteral("Ausgabe: Controller steht nicht (einmal, mit neuer Adresse) in der Bibliothek");
    }

    // Aus der Bibliothek holen: ein weiterer Controller mit denselben Werten
    fillLibraryMenu(addMenu_, LibraryAction::Add);
    if (addMenu_->actions().size() != 1 || !addMenu_->actions()[0]->isEnabled()) {
        return QStringLiteral("Ausgabe: Bibliotheksmenü zeigt den gespeicherten Controller nicht");
    }
    addMenu_->actions()[0]->trigger();
    if (player_.project()->destinations.size() != before + 2 || player_.project()->destinations.back().name != "Bibliothek-Test" ||
        player_.project()->destinations.back().address != "192.168.0.77") {
        return QStringLiteral("Ausgabe: Controller aus der Bibliothek wurde nicht übernommen");
    }

    // Auf den gewählten anwenden: der erste Controller bekommt die Werte, die Zeilenzahl bleibt
    table_->item(row, Address)->setText(QStringLiteral("192.168.0.99"));
    table_->setCurrentCell(row, Address);
    fillLibraryMenu(applyMenu_, LibraryAction::Apply);
    if (applyMenu_->actions().size() != 1) {
        return QStringLiteral("Ausgabe: Anwenden-Menü der Bibliothek ist leer");
    }
    applyMenu_->actions()[0]->trigger();
    if (player_.project()->destinations.size() != before + 2 ||
        player_.project()->destinations[static_cast<std::size_t>(row)].address != "192.168.0.77") {
        return QStringLiteral("Ausgabe: Anwenden aus der Bibliothek hat den Controller nicht überschrieben");
    }

    // Aus der Bibliothek löschen
    fillLibraryMenu(deleteMenu_, LibraryAction::Remove);
    if (deleteMenu_->actions().size() != 1) {
        return QStringLiteral("Ausgabe: Lösch-Menü der Bibliothek ist leer");
    }
    deleteMenu_->actions()[0]->trigger();
    const auto left = userlib::loadControllers(registry);
    if (!left.ok() || !left.value().empty()) {
        return QStringLiteral("Ausgabe: Controller wurde nicht aus der Bibliothek gelöscht");
    }

    // Aufräumen: die zwei Controller dieses Tests wieder entfernen
    for (int i = 0; i < 2; ++i) {
        table_->clearSelection();
        table_->setCurrentCell(-1, -1);
        removeSelectedDestination();
    }
    if (player_.project()->destinations.size() != before) {
        return QStringLiteral("Ausgabe: Aufräumen nach dem Bibliothekstest schlug fehl");
    }
    return {};
}

}  // namespace whitehole::studio
