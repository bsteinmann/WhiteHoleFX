// player.cpp: Zustandsführung für Oberflächen.
#include "whitehole/runtime/player.hpp"

#include "whitehole/output/packet_sink.hpp"

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
    map_ = std::move(parts.value().map);
    project_.emplace(std::move(project));
    path_ = std::move(path);
    stepError_.reset();
    return {};
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
    auto session = Session::create(
        *project_, SessionDeps{*device_, library_.snippets, library_.kernels, sink_, *clock}, options);
    if (!session.ok()) {
        return session.error();
    }
    clock_ = std::move(clock);
    session_ = std::move(session).value();
    stepError_.reset();
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

Result<preview::Image> Player::image(const preview::PreviewOptions& options) const {
    if (!project_) {
        return Error{ErrorCode::InvalidArgument, "Es ist kein Projekt geladen"};
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
