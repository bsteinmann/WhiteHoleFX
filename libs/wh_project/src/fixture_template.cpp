// fixture_template.cpp: Umsetzung zu fixture_template.hpp. Eine Vorlage ist ein Projekt mit einem Fixture.
#include "whitehole/project/fixture_template.hpp"

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

#include "whitehole/graph/node_registry.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::project {

namespace {

/// Vorlagen haben keinen Effektgraphen, also auch keine Node-Typen.
class EmptyRegistry final : public graph::INodeRegistry {
public:
    [[nodiscard]] const graph::NodeDescriptor* find(std::string_view) const override { return nullptr; }
    [[nodiscard]] std::vector<const graph::NodeDescriptor*> all() const override { return {}; }
};

Project emptyProject() { return Project{std::make_shared<const EmptyRegistry>()}; }

}  // namespace

Result<std::string> writeFixtureTemplate(const FixtureTemplate& tpl) {
    if (tpl.name.empty()) {
        return Error{ErrorCode::InvalidArgument, "Die Vorlage braucht einen Namen"};
    }
    Project project = emptyProject();
    const Result<mapping::FixtureId> id = project.layout.add(tpl.name, Vec3{0.0F, 0.0F, 0.0F}, tpl.shape);
    if (!id.ok()) {
        return id.error();
    }
    if (tpl.patch) {
        output::FixturePatch patch = *tpl.patch;
        patch.fixture = id.value();
        project.patches.push_back(patch);
    }
    return writeProject(project);
}

Result<FixtureTemplate> parseFixtureTemplate(std::string_view text) {
    Result<Project> parsed = parseProject(text, std::make_shared<const EmptyRegistry>());
    if (!parsed.ok()) {
        return parsed.error();
    }
    const Project& project = parsed.value();
    if (project.layout.fixtures().size() != 1) {
        return Error{ErrorCode::InvalidArgument, "Eine Fixture-Vorlage enthält genau ein Fixture"};
    }
    const mapping::Fixture& fixture = project.layout.fixtures().front();
    FixtureTemplate tpl;
    tpl.name = fixture.name;
    tpl.shape = fixture.shape;
    for (const output::FixturePatch& p : project.patches) {
        if (p.fixture == fixture.id) {
            tpl.patch = p;
            break;
        }
    }
    return tpl;
}

namespace {

/// Dateiname aus dem Namen: Zeichen, die Dateisysteme nicht mögen, werden zu "_". Die Namen im Ordner
/// sind damit eindeutig genug; der echte Name steht in der Datei.
std::filesystem::path fileFor(const std::filesystem::path& folder, const std::string& name) {
    std::string safe = name;
    for (char& c : safe) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ||
            static_cast<unsigned char>(c) < 32) {
            c = '_';
        }
    }
    safe += ".whfx";
    return folder / std::filesystem::path{std::u8string{safe.begin(), safe.end()}};
}

}  // namespace

std::vector<FixtureTemplate> FixtureLibrary::list() const {
    std::vector<FixtureTemplate> result;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator{folder_, ec}) {
        if (!entry.is_regular_file() || entry.path().extension() != ".whfx") {
            continue;
        }
        std::ifstream in{entry.path(), std::ios::binary};
        std::ostringstream text;
        text << in.rdbuf();
        if (Result<FixtureTemplate> tpl = parseFixtureTemplate(text.str()); tpl.ok()) {
            result.push_back(std::move(tpl.value()));
        }
    }
    std::sort(result.begin(), result.end(),
              [](const FixtureTemplate& a, const FixtureTemplate& b) { return a.name < b.name; });
    return result;
}

Result<void> FixtureLibrary::save(const FixtureTemplate& tpl) const {
    const Result<std::string> text = writeFixtureTemplate(tpl);
    if (!text.ok()) {
        return text.error();
    }
    std::error_code ec;
    std::filesystem::create_directories(folder_, ec);
    if (ec) {
        return Error{ErrorCode::IoError, "Der Ordner der Fixture-Bibliothek lässt sich nicht anlegen"};
    }
    std::ofstream out{fileFor(folder_, tpl.name), std::ios::binary | std::ios::trunc};
    out << text.value();
    if (!out) {
        return Error{ErrorCode::IoError, "Die Vorlage lässt sich nicht schreiben"};
    }
    return {};
}

Result<void> FixtureLibrary::remove(const std::string& name) const {
    std::error_code ec;
    if (!std::filesystem::remove(fileFor(folder_, name), ec) || ec) {
        return Error{ErrorCode::NotFound, "Diese Vorlage gibt es nicht"};
    }
    return {};
}

}  // namespace whitehole::project
