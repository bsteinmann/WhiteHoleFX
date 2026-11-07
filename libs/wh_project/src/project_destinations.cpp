// project_destinations.cpp: Umsetzung zu project_destinations.hpp und Project-weite Angleichung der Patches.
//
// Warum es diese Datei gibt: Das Protokoll gehört seit 0.61 zum Controller. Der Parser und der Player brauchen dieselbe
// Regel, wie Patches und Controller zusammenpassen; sie steht hier statt in beiden.
#include "project_destinations.hpp"

#include <array>
#include <string>

#include "whitehole/output/patch_protocol.hpp"

namespace whitehole::project {

namespace detail {

Result<void> resolveDestinationProtocols(Project& project, const std::vector<bool>& explicitProtocol) {
    const std::size_t original = project.destinations.size();
    for (std::size_t i = 0; i < original; ++i) {
        std::array<bool, 3> used{};
        for (const output::FixturePatch& patch : project.patches) {
            if (patch.destination == i) {
                used[static_cast<std::size_t>(patch.protocol)] = true;
            }
        }
        const bool declared = i < explicitProtocol.size() && explicitProtocol[i];
        if (declared) {
            for (const output::FixturePatch& patch : project.patches) {
                if (patch.destination == i && patch.protocol != project.destinations[i].protocol) {
                    const mapping::Fixture* fixture = project.layout.find(patch.fixture);
                    return Error{ErrorCode::InvalidArgument,
                                 "Das Protokoll des Patches von \"" + (fixture != nullptr ? fixture->name : std::string{"?"}) +
                                     "\" passt nicht zu Ziel " + std::to_string(i) +
                                     "; das Protokoll steht beim Ziel (destination ... protocol ...)"};
                }
            }
            continue;
        }
        bool first = true;
        for (std::size_t p = 0; p < used.size(); ++p) {
            if (!used[p]) {
                continue;
            }
            const auto protocol = static_cast<output::Protocol>(p);
            if (first) {
                project.destinations[i].protocol = protocol;
                first = false;
                continue;
            }
            output::Destination copy = project.destinations[i];
            copy.protocol = protocol;
            const std::size_t index = project.destinations.size();
            project.destinations.push_back(std::move(copy));
            for (output::FixturePatch& patch : project.patches) {
                if (patch.destination == i && patch.protocol == protocol) {
                    patch.destination = static_cast<std::uint32_t>(index);
                }
            }
        }
        if (first) {  // kein Patch darauf: Multicast gibt es nur bei sACN, sonst Art-Net wie bisher
            project.destinations[i].protocol =
                project.destinations[i].address.empty() ? output::Protocol::Sacn : output::Protocol::ArtNet;
        }
    }
    return {};
}

}  // namespace detail

Result<int> alignPatches(Project& project) {
    int changed = 0;
    for (output::FixturePatch& patch : project.patches) {
        if (patch.destination >= project.destinations.size()) {
            continue;
        }
        const output::Protocol wanted = project.destinations[patch.destination].protocol;
        if (patch.protocol == wanted) {
            continue;
        }
        const Result<int> shifted = output::alignPatchProtocol(patch, wanted);
        if (!shifted.ok()) {
            const mapping::Fixture* fixture = project.layout.find(patch.fixture);
            return Error{shifted.error().code, "\"" + (fixture != nullptr ? fixture->name : std::string{"?"}) + "\": " +
                                                   shifted.error().message};
        }
        ++changed;
    }
    return changed;
}

}  // namespace whitehole::project
