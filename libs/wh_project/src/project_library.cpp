// project_library.cpp: Export/Import einzelner Fixtures als eigene Textdatei, plus die Controller-Bibliothek.
//
// Warum es das gibt: Ein Fixture (Matrix 8x8, Säule ...), das man in einem anderen Projekt wiederverwenden will,
// exportiert man im Reiter Fixtures als eigene Datei und importiert sie dort später wieder - der Nutzer wählt den
// Pfad selbst (Dateidialog), es gibt keine automatisch geladene Bibliothek mehr. Export/Import nutzt dieselbe
// Schreibweise wie die Projektdatei (`fixturetype`-Blöcke), damit Parser und Writer nicht doppelt gebaut werden
// müssen.
#include <string>

#include "whitehole/project/project.hpp"

namespace whitehole::project {

std::string writeFixtureExport(const std::vector<FixtureType>& types) {
    return "whitehole-project 1\n# Fixture-Export (WhiteHoleFX).\n\n" + writeFixtureTypes(types);
}

Result<std::vector<FixtureType>> parseFixtureExport(std::string_view text,
                                                    std::shared_ptr<const graph::INodeRegistry> registry) {
    Result<Project> parsed = parseProject(text, std::move(registry));
    if (!parsed.ok()) {
        return parsed.error();
    }
    return std::move(parsed.value().types);
}

std::string writeControllerLibrary(const std::vector<output::Destination>& controllers) {
    return "whitehole-project 1\n# Benutzerbibliothek der Controller (WhiteHoleFX). Wird vom Studio gepflegt.\n\n" +
           writeDestinations(controllers);
}

Result<std::vector<output::Destination>> parseControllerLibrary(std::string_view text,
                                                                 std::shared_ptr<const graph::INodeRegistry> registry) {
    Result<Project> parsed = parseProject(text, std::move(registry));
    if (!parsed.ok()) {
        return parsed.error();
    }
    std::vector<output::Destination> controllers = std::move(parsed.value().destinations);
    for (std::size_t i = 0; i < controllers.size(); ++i) {
        if (controllers[i].name.empty()) {
            return Error{ErrorCode::InvalidArgument, "Controller " + std::to_string(i) + " in der Bibliothek hat keinen Namen"};
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (controllers[j].name == controllers[i].name) {
                return Error{ErrorCode::AlreadyExists, "Der Name \"" + controllers[i].name + "\" steht in der Bibliothek zweimal"};
            }
        }
    }
    return controllers;
}

}  // namespace whitehole::project
