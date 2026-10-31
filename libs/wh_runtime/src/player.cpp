// player.cpp: Zustandsführung für Oberflächen.
#include "whitehole/runtime/player.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>

#include "whitehole/output/packet_sink.hpp"
#include "whitehole/runtime/graph_layout.hpp"

namespace whitehole::runtime {

namespace {

/// Wirft Pakete weg; dient nur der Prüfung des Ausgabeplans beim Laden.
class DiscardSink final : public output::IPacketSink {
public:
    Result<void> send(const output::Endpoint&, std::span<const std::uint8_t>) override { return {}; }
};

}  // namespace

Result<void> Player::load(const std::string& path) {
    const auto text = project::readTextFile(path);
    if (!text.ok()) {
        return text.error();
    }
    auto parsed = project::parseProject(text.value(), library_.registry);
    if (!parsed.ok()) {
        return parsed.error();
    }
    return adopt(std::move(parsed).value(), path);
}

Result<void> Player::loadText(std::string_view text) {
    auto parsed = project::parseProject(text, library_.registry);
    if (!parsed.ok()) {
        return parsed.error();
    }
    return adopt(std::move(parsed).value(), {});
}

Result<void> Player::adopt(project::Project project, std::string path) {
    // Den Ausgabeplan jetzt prüfen: Konfigurationsfehler sollen beim Öffnen auffallen, nicht erst
    // beim Start.
    DiscardSink discard;
    auto parts = buildOutput(project, discard);
    if (!parts.ok()) {
        return parts.error();
    }
    stop();
    live_.reset();
    liveLast_.reset();
    map_ = std::move(parts.value().map);
    applyLayoutIfMissing(project.graph);
    project_.emplace(std::move(project));
    path_ = std::move(path);
    stepError_.reset();
    graphDirty_ = false;
    valuesDirty_ = false;
    rebuildDirty_ = false;
    modified_ = false;
    undo_.clear();
    redo_.clear();
    lastEditKey_.clear();
    testPattern_ = TestPattern{};
    return {};
}

Result<void> Player::setTestPattern(TestPattern pattern) {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    if (!std::isfinite(pattern.color.r) || !std::isfinite(pattern.color.g) || !std::isfinite(pattern.color.b)) {
        return Error{ErrorCode::InvalidArgument, "Die Testfarbe ist keine Zahl"};
    }
    pattern.color.r = std::clamp(pattern.color.r, 0.0F, 1.0F);
    pattern.color.g = std::clamp(pattern.color.g, 0.0F, 1.0F);
    pattern.color.b = std::clamp(pattern.color.b, 0.0F, 1.0F);
    pattern.color.a = 1.0F;
    testPattern_ = std::move(pattern);
    if (session_) {
        session_->setTestPattern(testPattern_);
    }
    return {};
}

float Player::setMasterBrightness(float master) {
    master_ = std::isfinite(master) ? std::clamp(master, 0.0F, 1.0F) : 1.0F;
    if (session_) {
        session_->setOutputLevel(master_, blackout_);
    }
    return master_;
}

void Player::setBlackout(bool on) {
    blackout_ = on;
    if (session_) {
        session_->setOutputLevel(master_, blackout_);
    }
}

void Player::clearTestPattern() {
    testPattern_ = TestPattern{};
    if (session_) {
        session_->setTestPattern(TestPattern{});
    }
}

Result<void> Player::start() {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    if (device_ == nullptr) {
        return Error{ErrorCode::InvalidArgument,
                     "Kein GPU-Gerät: Das Programm wurde ohne GPU-Backend gebaut oder es wurde kein Adapter gefunden"};
    }
    if (session_) {
        return {};
    }
    auto clock = std::make_unique<engine::SteadyFrameClock>();
    SessionOptions options;
    options.keepLatestColors = true;
    // Im Live-Betrieb rechnet die Session den Graphen der Decks, nicht den des Editors.
    std::optional<project::Project> liveProject;
    if (live_) {
        liveProject.emplace(*project_);
        liveProject->graph = live_->graph();
        liveProject->target = live_->target();
    }
    auto session = Session::create(
        liveProject ? *liveProject : *project_, SessionDeps{*device_, library_.snippets, library_.kernels, sink_, *clock}, options);
    if (!session.ok()) {
        return session.error();
    }
    clock_ = std::move(clock);
    session_ = std::move(session).value();
    if (testPattern_.active()) {
        session_->setTestPattern(testPattern_);
    }
    session_->setOutputLevel(master_, blackout_);
    stepError_.reset();
    graphDirty_ = false;
    valuesDirty_ = false;
    rebuildDirty_ = false;
    return {};
}

void Player::stop() {
    // Die Session zuerst: Sie liest die Uhr noch beim Abbau.
    session_.reset();
    clock_.reset();
}

Result<void> Player::step() {
    if (!session_) {
        return {};
    }
    if (rebuildDirty_) {
        // Das Ziel steckt in der Stage; ein neues braucht eine neue Session.
        rebuildDirty_ = false;
        graphDirty_ = false;
        valuesDirty_ = false;
        stop();
        if (const Result<void> restarted = start(); !restarted.ok()) {
            stepError_ = restarted.error();
            return restarted;
        }
    }
    bool structural = false;
    if (live_) {
        // Die Überblendung läuft mit der Uhrzeit, nicht mit der Zahl der Frames
        const double now = liveNow();
        const double elapsed = liveLast_ ? now - *liveLast_ : 0.0;
        liveLast_ = now;
        if (live_->advance(elapsed)) {
            valuesDirty_ = true;
        }
        if (live_->takeStructureChanged()) {
            graphDirty_ = true;
            structural = true;
        }
    }
    if (graphDirty_) {
        graphDirty_ = false;
        valuesDirty_ = false;
        // Ein Fehler (z. B. Shader lässt sich nicht übersetzen) lässt den letzten guten Zustand
        // laufen und steht in status().lastError.
        (void)session_->applyGraph(activeGraph());
        if (structural) {
            liveLast_ = liveNow();  // das Übersetzen des Shaders soll nicht von der Überblendzeit abgehen
        }
    } else if (valuesDirty_) {
        valuesDirty_ = false;
        (void)session_->applyParameters(activeGraph());
    }
    const Result<void> r = session_->renderFrame();
    if (!r.ok()) {
        stepError_ = r.error();
        stop();
    }
    return r;
}

PlayerStatus Player::status() const {
    PlayerStatus s;
    s.loaded = project_.has_value();
    s.running = session_ != nullptr;
    s.path = path_;
    s.framesPerSecond = framesPerSecond();
    s.ledCount = static_cast<std::uint32_t>(map_.world.size());
    s.lastError = stepError_;
    if (session_) {
        const SessionStats stats = session_->stats();
        s.frames = stats.engine.framesRendered;
        s.framesDropped = stats.engine.framesDropped;
        s.packets = stats.output.packetsSent;
        s.sendErrors = stats.output.sendErrors;
        if (const auto error = session_->stageError()) {
            s.lastError = error;
        } else if (const auto outputError = session_->lastOutputError()) {
            s.lastError = outputError;
        }
    }
    return s;
}

std::vector<NodeInfo> Player::nodes() const {
    std::vector<NodeInfo> result;
    if (!project_) {
        return result;
    }
    const graph::Graph& g = project_->graph;
    std::unordered_map<std::string, std::size_t> total;
    for (const graph::NodeInstance& node : g.nodes()) {
        ++total[node.typeId];
    }
    std::unordered_map<std::string, std::size_t> seen;
    for (const graph::NodeInstance& node : g.nodes()) {
        const std::size_t index = ++seen[node.typeId];
        const graph::NodeDescriptor* descriptor = g.registry().find(node.typeId);
        if (descriptor == nullptr || descriptor->parameters.empty()) {
            continue;
        }
        NodeInfo info;
        info.id = node.id;
        info.typeId = node.typeId;
        info.title = descriptor->displayName;
        if (total[node.typeId] > 1) {
            info.title += " " + std::to_string(index);
        }
        for (const graph::ParameterDescriptor& p : descriptor->parameters) {
            ParameterInfo parameter;
            parameter.key = p.key;
            parameter.displayName = p.displayName;
            parameter.defaultValue = p.defaultValue;
            const auto it = node.parameters.find(p.key);
            parameter.value = it != node.parameters.end() ? it->second : p.defaultValue;
            parameter.minValue = p.minValue;
            parameter.maxValue = p.maxValue;
            parameter.choices = p.choices;
            parameter.driven = g.findInputConnection(node.id, p.key) != nullptr;
            info.parameters.push_back(std::move(parameter));
        }
        result.push_back(std::move(info));
    }
    return result;
}

Result<graph::ParameterValue> Player::setParameter(NodeId node, const std::string& key,
                                                   graph::ParameterValue value) {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    const std::string editKey = "param:" + std::to_string(node.value()) + ":" + key;
    auto before = beginEdit(editKey);
    const Result<void> set = project_->graph.setParameter(node, key, std::move(value));
    finishEdit(std::move(before), editKey, set.ok());
    if (!set.ok()) {
        return set.error();
    }
    // Ein Text steckt im Shader (@text); nur reine Zahlen, Farben und Schalter gehen den billigen Weg
    if (std::holds_alternative<std::string>(project_->graph.findNode(node)->parameters.at(key))) {
        graphDirty_ = true;
    } else {
        valuesDirty_ = true;
    }
    modified_ = true;
    return project_->graph.findNode(node)->parameters.at(key);
}

Result<void> Player::save(const std::string& path) {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    const auto text = project::writeProject(*project_);
    if (!text.ok()) {
        return text.error();
    }
    if (const Result<void> r = project::writeTextFile(path, text.value()); !r.ok()) {
        return r;
    }
    path_ = path;
    modified_ = false;
    return {};
}

std::optional<LedInfo> Player::ledInfoAt(const preview::ViewFrame& frame, float pixelX, float pixelY,
                                         float maxDistance) const {
    if (!project_) {
        return std::nullopt;
    }
    const std::optional<std::uint32_t> led = ledAtPixel(map_, frame, pixelX, pixelY, maxDistance);
    if (!led) {
        return std::nullopt;
    }
    const std::vector<ColorRgba> colors = session_ ? session_->latestColors() : std::vector<ColorRgba>{};
    return describeLed(*project_, map_, *led, colors);
}

Result<preview::Image> Player::image(const preview::PreviewOptions& given) const {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
    }
    // Die Vorschau zeigt, was die LEDs ausgeben: je Fixture das Gamma seines Patches.
    preview::PreviewOptions options = given;
    if (options.fixtureGamma.empty()) {
        for (const mapping::FixtureRange& range : map_.ranges) {
            float gamma = options.gamma;
            for (const output::FixturePatch& patch : project_->patches) {
                if (patch.fixture == range.fixture) {
                    gamma = patch.color.gamma;
                }
            }
            options.fixtureGamma.push_back(gamma);
        }
    }
    if (session_) {
        std::vector<ColorRgba> colors = session_->latestColors();
        if (colors.empty()) {
            colors.assign(map_.world.size(), ColorRgba{0.0F, 0.0F, 0.0F, 1.0F});
        }
        return preview::renderPreview(map_, colors, options);
    }
    return preview::renderPreview(map_, preview::layoutColors(map_), options);
}

}  // namespace whitehole::runtime
