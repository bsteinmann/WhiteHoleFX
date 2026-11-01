// node_defaults.cpp: Umsetzung zu node_defaults.hpp. Eigene Standardwerte für neue Nodes und ihre Text-Kodierung.
#include "whitehole/runtime/node_defaults.hpp"

#include <charconv>
#include <cmath>
#include <system_error>
#include <vector>

namespace whitehole::runtime {

void NodeDefaults::set(const std::string& typeId, const std::string& key, graph::ParameterValue value) {
    values_[typeId][key] = std::move(value);
}

bool NodeDefaults::clear(const std::string& typeId, const std::string& key) {
    const auto type = values_.find(typeId);
    if (type == values_.end() || type->second.erase(key) == 0) {
        return false;
    }
    if (type->second.empty()) {
        values_.erase(type);
    }
    return true;
}

bool NodeDefaults::clearType(const std::string& typeId) { return values_.erase(typeId) > 0; }

const graph::ParameterValue* NodeDefaults::find(const std::string& typeId, const std::string& key) const {
    const auto type = values_.find(typeId);
    if (type == values_.end()) {
        return nullptr;
    }
    const auto value = type->second.find(key);
    return value == type->second.end() ? nullptr : &value->second;
}

const NodeDefaults::Values* NodeDefaults::valuesOf(const std::string& typeId) const {
    const auto type = values_.find(typeId);
    return type == values_.end() ? nullptr : &type->second;
}

namespace {

template <typename T>
std::string shortest(T value) {
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof buffer, value);
    return std::string{buffer, result.ptr};
}

template <typename T>
std::optional<T> number(std::string_view text) {
    T value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    if constexpr (std::is_floating_point_v<T>) {
        if (!std::isfinite(value)) {
            return std::nullopt;
        }
    }
    return value;
}

std::vector<std::string_view> split(std::string_view text) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t comma = text.find(',', start);
        parts.push_back(text.substr(start, comma == std::string_view::npos ? std::string_view::npos : comma - start));
        if (comma == std::string_view::npos) {
            return parts;
        }
        start = comma + 1;
    }
}

/// Genau `count` endliche Zahlen, durch Komma getrennt.
std::optional<std::vector<float>> floats(std::string_view text, std::size_t count) {
    const std::vector<std::string_view> parts = split(text);
    if (parts.size() != count) {
        return std::nullopt;
    }
    std::vector<float> values;
    for (const std::string_view part : parts) {
        const auto value = number<float>(part);
        if (!value) {
            return std::nullopt;
        }
        values.push_back(*value);
    }
    return values;
}

}  // namespace

std::string encodeParameterValue(const graph::ParameterValue& value) {
    struct Visitor {
        std::string operator()(bool v) const { return v ? "b:1" : "b:0"; }
        std::string operator()(std::int64_t v) const { return "i:" + shortest(v); }
        std::string operator()(double v) const { return "d:" + shortest(v); }
        std::string operator()(const Vec2& v) const { return "v2:" + shortest(v.x) + "," + shortest(v.y); }
        std::string operator()(const Vec3& v) const {
            return "v3:" + shortest(v.x) + "," + shortest(v.y) + "," + shortest(v.z);
        }
        std::string operator()(const ColorRgba& v) const {
            return "c:" + shortest(v.r) + "," + shortest(v.g) + "," + shortest(v.b) + "," + shortest(v.a);
        }
        std::string operator()(const std::string& v) const { return "s:" + v; }
    };
    return std::visit(Visitor{}, value);
}

std::optional<graph::ParameterValue> decodeParameterValue(std::string_view text) {
    const std::size_t colon = text.find(':');
    if (colon == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view kind = text.substr(0, colon);
    const std::string_view body = text.substr(colon + 1);
    if (kind == "b") {
        if (body == "1" || body == "0") {
            return graph::ParameterValue{body == "1"};
        }
        return std::nullopt;
    }
    if (kind == "i") {
        const auto v = number<std::int64_t>(body);
        return v ? std::optional<graph::ParameterValue>{*v} : std::nullopt;
    }
    if (kind == "d") {
        const auto v = number<double>(body);
        return v ? std::optional<graph::ParameterValue>{*v} : std::nullopt;
    }
    if (kind == "v2") {
        const auto v = floats(body, 2);
        return v ? std::optional<graph::ParameterValue>{Vec2{(*v)[0], (*v)[1]}} : std::nullopt;
    }
    if (kind == "v3") {
        const auto v = floats(body, 3);
        return v ? std::optional<graph::ParameterValue>{Vec3{(*v)[0], (*v)[1], (*v)[2]}} : std::nullopt;
    }
    if (kind == "c") {
        const auto v = floats(body, 4);
        return v ? std::optional<graph::ParameterValue>{ColorRgba{(*v)[0], (*v)[1], (*v)[2], (*v)[3]}} : std::nullopt;
    }
    if (kind == "s") {
        return graph::ParameterValue{std::string{body}};
    }
    return std::nullopt;
}

}  // namespace whitehole::runtime
