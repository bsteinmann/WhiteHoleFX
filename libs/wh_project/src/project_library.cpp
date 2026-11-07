// project_library.cpp: die Benutzerbibliothek der Bauformen als eigene Textdatei.
//
// Warum es das gibt: Bauformen (Matrix 8x8, Säule ...) soll man nicht in jedem Projekt neu anlegen und nicht verlieren,
// wenn man eine neue Programmversion installiert. Das Studio hält deshalb zusätzlich zu den Bauformen im Projekt eine
// Datei im Benutzerverzeichnis. Sie nutzt dieselbe Schreibweise wie die Projektdatei (`fixturetype`-Blöcke), damit
// Parser und Writer nicht doppelt gebaut werden müssen.
#include <string>

#include "whitehole/project/project.hpp"

namespace whitehole::project {

std::string writeFixtureLibrary(const std::vector<FixtureType>& types) {
    return "whitehole-project 1\n# Benutzerbibliothek der Bauformen (WhiteHoleFX). Wird vom Studio gepflegt.\n\n" +
           writeFixtureTypes(types);
}

Result<std::vector<FixtureType>> parseFixtureLibrary(std::string_view text,
                                                     std::shared_ptr<const graph::INodeRegistry> registry) {
    Result<Project> parsed = parseProject(text, std::move(registry));
    if (!parsed.ok()) {
        return parsed.error();
    }
    return std::move(parsed.value().types);
}

}  // namespace whitehole::project
