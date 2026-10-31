// node_preset.cpp: Presets lesen, schreiben und im Ordner verwalten (siehe node_preset.hpp).
#include "whitehole/runtime/node_preset.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include "whitehole/project/project.hpp"

namespace whitehole::runtime {

namespace fs = std::filesystem;

Result<std::string> presetToText(const NodeClipboard& clipboard, std::shared_ptr<const graph::INodeRegistry> registry) {
    if (clipboard.empty()) {
        return Error{ErrorCode::InvalidArgument, "Es sind keine Nodes ausgewählt"};
    }
    project::Project fragment{std::move(registry)};
    const auto ids = applyClipboard(fragment.graph, clipboard, Vec2{});
    if (!ids.ok()) {
        return ids.error();
    }
    return project::writeProject(fragment);
}

Result<NodeClipboard> presetFromText(std::string_view text, std::shared_ptr<const graph::INodeRegistry> registry) {
    auto parsed = project::parseProject(text, std::move(registry));
    if (!parsed.ok()) {
        return parsed.error();
    }
    const graph::Graph& graph = parsed.value().graph;
    std::vector<NodeId> all;
    for (const graph::NodeInstance& node : graph.nodes()) {
        all.push_back(node.id);
    }
    NodeClipboard clip = copyNodes(graph, all);
    if (clip.empty()) {
        return Error{ErrorCode::InvalidArgument, "Das Preset enthält keinen Node"};
    }
    return clip;
}

bool isValidPresetName(std::string_view name) {
    if (name.empty() || name.size() > 64) {
        return false;
    }
    if (name.front() == '.' || name.front() == ' ' || name.back() == '.' || name.back() == ' ') {
        return false;
    }
    return std::ranges::none_of(name, [](char c) {
        return static_cast<unsigned char>(c) < 0x20 || std::string_view{"\\/:*?\"<>|"}.find(c) != std::string_view::npos;
    });
}

namespace {

/// UTF-8-Text <-> Pfad. Dateinamen sind UTF-8 (Umlaute sind erlaubt); auf Windows braucht das den
/// Umweg über char8_t, sonst würde die Ansi-Codepage angenommen.
fs::path pathFromUtf8(const std::string& text) {
    return fs::path{std::u8string{reinterpret_cast<const char8_t*>(text.data()), text.size()}};
}

std::string utf8From(const fs::path& path) {
    const std::u8string text = path.u8string();
    return std::string{reinterpret_cast<const char*>(text.data()), text.size()};
}

std::string lower(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

}  // namespace

fs::path PresetStore::fileOf(const std::string& name) const { return directory_ / pathFromUtf8(name + kPresetExtension); }

std::vector<std::string> PresetStore::list() const {
    std::vector<std::string> names;
    std::error_code ec;
    for (const fs::directory_entry& entry : fs::directory_iterator(directory_, ec)) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != kPresetExtension) {
            continue;
        }
        std::string name = utf8From(entry.path().stem());
        if (isValidPresetName(name)) {
            names.push_back(std::move(name));
        }
    }
    std::ranges::sort(names, [](const std::string& a, const std::string& b) {
        const std::string la = lower(a);
        const std::string lb = lower(b);
        return la != lb ? la < lb : a < b;
    });
    return names;
}

bool PresetStore::contains(const std::string& name) const {
    std::error_code ec;
    return isValidPresetName(name) && fs::is_regular_file(fileOf(name), ec);
}

Result<void> PresetStore::save(const std::string& name, std::string_view text) const {
    if (!isValidPresetName(name)) {
        return Error{ErrorCode::InvalidArgument, "Ungültiger Preset-Name (keine Sonderzeichen wie / \\ : * ? \" < > |)"};
    }
    std::error_code ec;
    fs::create_directories(directory_, ec);
    if (ec) {
        return Error{ErrorCode::IoError, "Preset-Ordner lässt sich nicht anlegen: " + ec.message()};
    }
    std::ofstream out{fileOf(name), std::ios::binary | std::ios::trunc};
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.close();
    if (!out) {
        return Error{ErrorCode::IoError, "Preset \"" + name + "\" lässt sich nicht schreiben"};
    }
    return {};
}

Result<std::string> PresetStore::load(const std::string& name) const {
    if (!isValidPresetName(name)) {
        return Error{ErrorCode::InvalidArgument, "Ungültiger Preset-Name"};
    }
    if (!contains(name)) {
        return Error{ErrorCode::NotFound, "Preset \"" + name + "\" gibt es nicht"};
    }
    std::ifstream in{fileOf(name), std::ios::binary};
    std::ostringstream text;
    text << in.rdbuf();
    if (!in && !in.eof()) {
        return Error{ErrorCode::IoError, "Preset \"" + name + "\" lässt sich nicht lesen"};
    }
    return text.str();
}

Result<void> PresetStore::remove(const std::string& name) const {
    if (!isValidPresetName(name)) {
        return Error{ErrorCode::InvalidArgument, "Ungültiger Preset-Name"};
    }
    if (!contains(name)) {
        return Error{ErrorCode::NotFound, "Preset \"" + name + "\" gibt es nicht"};
    }
    std::error_code ec;
    fs::remove(fileOf(name), ec);
    if (ec) {
        return Error{ErrorCode::IoError, "Preset lässt sich nicht löschen: " + ec.message()};
    }
    return {};
}

}  // namespace whitehole::runtime
