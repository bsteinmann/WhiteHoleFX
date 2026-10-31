// project_parser.cpp: liest die Projektdatei Zeile für Zeile.
//
// Aufbau: Eine Zustandsmaschine mit drei Zuständen (außerhalb, im fixture-Block, im node-Block).
// Jede Zeile wird von einer eigenen kleinen Funktion gelesen; Fehler tragen die Zeilennummer.
// Prüfungen, die nur mit einem Gerät möglich sind (Shader übersetzen, Texturgröße), gehören
// nicht hierher; sie meldet später der Runner.
#include <type_traits>
#include <unordered_map>
#include <variant>

#include "project_fixture_parser.hpp"
#include "project_names.hpp"
#include "whitehole/project/project.hpp"

namespace whitehole::project {

namespace {

using detail::FixtureDraft;
using detail::Line;
using detail::Reader;

Error withLine(std::size_t line, const Error& e) {
    return Error{e.code, "Zeile " + std::to_string(line) + ": " + e.message};
}

class Parser {
public:
    explicit Parser(std::shared_ptr<const graph::INodeRegistry> registry)
        : project_{std::move(registry)} {}

    std::optional<Error> handle(const Line& line);
    Result<Project> finish();

private:
    enum class State { Outside, Fixture, Node };

    std::optional<Error> handleOutside(const Line& line);
    std::optional<Error> handleNode(const Line& line);
    std::optional<Error> settings(Reader& r);
    void destination(Reader& r);
    std::optional<Error> beginNode(Reader& r);
    std::optional<Error> setParameter(Reader& r);
    std::optional<Error> connect(Reader& r);
    std::optional<Error> output(Reader& r);
    std::optional<graph::PortRef> portRef(Reader& r, const char* what);

    Project project_;
    State state_{State::Outside};
    bool headerSeen_{false};
    std::size_t blockStart_{0};
    FixtureDraft fixture_;
    NodeId node_;
    std::unordered_map<std::string, NodeId> nodes_;
};

std::optional<Error> Parser::handle(const Line& line) {
    Reader r{line, 0};
    if (!headerSeen_) {
        if (line.words[0] != "whitehole-project") {
            return withLine(line.number, Error{ErrorCode::InvalidArgument,
                                                 "Die Datei muss mit \"whitehole-project 1\" beginnen"});
        }
        (void)r.word("Schlüsselwort");
        const auto version = r.whole("Versionsnummer", 1000000);
        r.expectEnd();
        if (!r.ok()) {
            return r.error();
        }
        if (version != kFormatVersion) {
            return withLine(line.number,
                            Error{ErrorCode::Unsupported,
                                  "Formatversion " + std::to_string(version) + " wird nicht unterstützt"});
        }
        headerSeen_ = true;
        return std::nullopt;
    }

    if (state_ == State::Fixture) {
        (void)r.word("Schlüsselwort");
        if (line.words[0] == "end") {
            r.expectEnd();
            if (!r.ok()) {
                return r.error();
            }
            state_ = State::Outside;
            return detail::finishFixture(fixture_, project_);
        }
        detail::fixtureStatement(line.words[0], r, fixture_);
        return r.ok() ? std::nullopt : std::optional<Error>{r.error()};
    }
    if (state_ == State::Node) {
        return handleNode(line);
    }
    return handleOutside(line);
}

std::optional<Error> Parser::handleOutside(const Line& line) {
    Reader r{line, 0};
    const std::string key = r.word("Schlüsselwort");
    std::optional<Error> error;
    if (key == "settings") {
        error = settings(r);
    } else if (key == "destination") {
        destination(r);
    } else if (key == "fixture") {
        fixture_ = detail::beginFixture(r);
        blockStart_ = line.number;
        if (r.ok()) {
            state_ = State::Fixture;
        }
    } else if (key == "node") {
        error = beginNode(r);
        blockStart_ = line.number;
    } else if (key == "connect") {
        error = connect(r);
    } else if (key == "output") {
        error = output(r);
    } else {
        r.fail("Unbekannte Anweisung \"" + key + "\"");
    }
    if (error) {
        return error;
    }
    return r.ok() ? std::nullopt : std::optional<Error>{r.error()};
}

std::optional<Error> Parser::settings(Reader& r) {
    while (r.ok() && !r.atEnd()) {
        const std::string key = r.word("Einstellung");
        if (key == "fps") {
            const double fps = r.numberD("Bildrate");
            if (r.ok() && !(fps > 0.0 && fps <= 1000.0)) {
                r.fail("fps muss größer 0 und höchstens 1000 sein");
            }
            project_.framesPerSecond = fps;
        } else if (key == "normalize") {
            project_.normalize = detail::readName(r, detail::kNormalizeModes, "Normalisierung");
        } else if (key == "sacn-priority") {
            project_.sacnPriority = static_cast<std::uint8_t>(r.whole("Priorität", 200));
        } else if (key == "source-name") {
            project_.sourceName = r.word("Quellname");
        } else if (r.ok()) {
            r.fail("Unbekannte Einstellung \"" + key + "\"");
        }
    }
    return std::nullopt;
}

void Parser::destination(Reader& r) {
    const std::uint32_t number = r.whole("Zielnummer", 65535);
    if (r.ok() && number != project_.destinations.size()) {
        r.fail("Ziele müssen der Reihe nach von 0 an stehen; erwartet " +
               std::to_string(project_.destinations.size()));
    }
    const std::string address = r.word("Adresse");
    output::Destination d;
    if (r.ok() && address != "multicast") {
        if (!output::isIpv4(address)) {
            r.fail("\"" + address + "\" ist keine IPv4-Adresse (oder \"multicast\")");
        }
        d.address = address;
    }
    if (r.ok() && !r.atEnd()) {
        d.port = static_cast<std::uint16_t>(r.whole("Port", 65535));
    }
    r.expectEnd();
    if (r.ok()) {
        project_.destinations.push_back(std::move(d));
    }
}

std::optional<Error> Parser::beginNode(Reader& r) {
    const std::string name = r.word("Nodename");
    const std::string type = r.word("Node-Typ");
    r.expectEnd();
    if (!r.ok()) {
        return r.error();
    }
    if (name.find('.') != std::string::npos) {
        return withLine(r.line(), Error{ErrorCode::InvalidArgument, "Nodename darf keinen Punkt enthalten"});
    }
    if (nodes_.contains(name)) {
        return withLine(r.line(), Error{ErrorCode::AlreadyExists, "Nodename \"" + name + "\" ist schon vergeben"});
    }
    const auto id = project_.graph.addNode(type, Vec2{});
    if (!id.ok()) {
        return withLine(r.line(), id.error());
    }
    nodes_.emplace(name, id.value());
    node_ = id.value();
    state_ = State::Node;
    return std::nullopt;
}

std::optional<Error> Parser::handleNode(const Line& line) {
    Reader r{line, 0};
    const std::string key = r.word("Schlüsselwort");
    std::optional<Error> error;
    if (key == "end") {
        state_ = State::Outside;
    } else if (key == "set") {
        error = setParameter(r);
    } else if (key == "at") {
        const Vec2 position = r.vec2("Position");
        r.expectEnd();
        if (!r.ok()) {
            return r.error();
        }
        const auto result = project_.graph.setPosition(node_, position);
        if (!result.ok()) {
            return withLine(r.line(), result.error());
        }
    } else {
        r.fail("Unbekannte Anweisung \"" + key + "\" im node-Block");
    }
    if (error) {
        return error;
    }
    r.expectEnd();
    return r.ok() ? std::nullopt : std::optional<Error>{r.error()};
}

std::optional<Error> Parser::setParameter(Reader& r) {
    const std::string key = r.word("Parametername");
    if (!r.ok()) {
        return r.error();
    }
    const graph::NodeInstance* instance = project_.graph.findNode(node_);
    const graph::NodeDescriptor* descriptor = project_.graph.registry().find(instance->typeId);
    const graph::ParameterDescriptor* parameter = descriptor ? descriptor->findParameter(key) : nullptr;
    if (parameter == nullptr) {
        return withLine(r.line(), Error{ErrorCode::NotFound, "Node-Typ \"" + instance->typeId +
                                                                 "\" hat keinen Parameter \"" + key + "\""});
    }
    const graph::ParameterValue value = std::visit(
        [&](const auto& shape) -> graph::ParameterValue {
            using T = std::decay_t<decltype(shape)>;
            if constexpr (std::is_same_v<T, bool>) {
                return r.boolean(key);
            } else if constexpr (std::is_same_v<T, std::int64_t>) {
                return r.integer(key);
            } else if constexpr (std::is_same_v<T, double>) {
                return r.numberD(key);
            } else if constexpr (std::is_same_v<T, Vec2>) {
                return r.vec2(key);
            } else if constexpr (std::is_same_v<T, Vec3>) {
                return r.vec3(key);
            } else if constexpr (std::is_same_v<T, ColorRgba>) {
                return r.color(key);
            } else {
                return r.word(key);
            }
        },
        parameter->defaultValue);
    r.expectEnd();
    if (!r.ok()) {
        return r.error();
    }
    const auto result = project_.graph.setParameter(node_, key, value);
    return result.ok() ? std::nullopt : std::optional<Error>{withLine(r.line(), result.error())};
}

std::optional<graph::PortRef> Parser::portRef(Reader& r, const char* what) {
    const std::string text = r.word(what);
    if (!r.ok()) {
        return std::nullopt;
    }
    const std::size_t dot = text.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 == text.size()) {
        r.fail(std::string{what} + ": erwartet <node>.<port>, gefunden \"" + text + "\"");
        return std::nullopt;
    }
    const auto it = nodes_.find(text.substr(0, dot));
    if (it == nodes_.end()) {
        r.fail("Unbekannter Node \"" + text.substr(0, dot) + "\"", ErrorCode::NotFound);
        return std::nullopt;
    }
    return graph::PortRef{it->second, text.substr(dot + 1)};
}

std::optional<Error> Parser::connect(Reader& r) {
    const auto from = portRef(r, "Ausgang");
    if (r.ok() && r.word("->") != "->" && r.ok()) {
        r.fail("Erwartet \"->\"");
    }
    const auto to = portRef(r, "Eingang");
    r.expectEnd();
    if (!r.ok()) {
        return r.error();
    }
    const auto result = project_.graph.connect(*from, *to);
    return result.ok() ? std::nullopt : std::optional<Error>{withLine(r.line(), result.error())};
}

std::optional<Error> Parser::output(Reader& r) {
    const auto port = portRef(r, "Ausgang");
    r.expectEnd();
    if (!r.ok()) {
        return r.error();
    }
    if (project_.target) {
        return withLine(r.line(), Error{ErrorCode::AlreadyExists, "output ist schon gesetzt"});
    }
    const graph::NodeInstance* instance = project_.graph.findNode(port->node);
    const graph::NodeDescriptor* descriptor = project_.graph.registry().find(instance->typeId);
    const graph::PortDescriptor* found = descriptor ? descriptor->findPort(port->portKey) : nullptr;
    if (found == nullptr || found->direction != graph::PortDirection::Output) {
        return withLine(r.line(), Error{ErrorCode::NotFound, "\"" + port->portKey + "\" ist kein Ausgang von " +
                                                                 instance->typeId});
    }
    project_.target = *port;
    return std::nullopt;
}

Result<Project> Parser::finish() {
    if (!headerSeen_) {
        return Error{ErrorCode::InvalidArgument, "Die Datei ist leer"};
    }
    if (state_ != State::Outside) {
        return Error{ErrorCode::InvalidArgument,
                     "Zeile " + std::to_string(blockStart_) + ": Block ist nicht mit \"end\" beendet"};
    }
    return std::move(project_);
}

}  // namespace

Result<Project> parseProject(std::string_view text, std::shared_ptr<const graph::INodeRegistry> registry) {
    auto lines = detail::splitLines(text);
    if (!lines.ok()) {
        return lines.error();
    }
    Parser parser{std::move(registry)};
    for (const Line& line : lines.value()) {
        if (const auto error = parser.handle(line)) {
            return *error;
        }
    }
    return parser.finish();
}

}  // namespace whitehole::project
