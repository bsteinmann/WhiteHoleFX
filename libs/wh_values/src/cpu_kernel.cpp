// cpu_kernel.cpp: Umsetzung zu cpu_kernel.hpp. Die Rechenvorschrift eines CPU-Nodes und ihre Registrierung.
#include "whitehole/values/cpu_kernel.hpp"

#include <utility>
#include <variant>

namespace whitehole::values {

namespace {

template <typename T>
const T* findAs(const std::map<std::string, graph::ParameterValue, std::less<>>& values,
                std::string_view key) {
    const auto it = values.find(key);
    return it == values.end() ? nullptr : std::get_if<T>(&it->second);
}

bool isValuePortType(graph::PortType type) {
    switch (type) {
        case graph::PortType::Scalar:
        case graph::PortType::Integer:
        case graph::PortType::Boolean:
        case graph::PortType::Trigger:
        case graph::PortType::Color:
        case graph::PortType::Text:
        case graph::PortType::AudioSpectrum:
            return true;
        default:
            return false;
    }
}

/// Eine Fehlermeldung oder ein leerer Text, wenn die Definition passt.
std::string validate(const CpuNodeDefinition& definition) {
    const graph::NodeDescriptor& descriptor = definition.descriptor;
    if (descriptor.domain != graph::NodeDomain::Cpu) {
        return "die Domäne muss Cpu sein";
    }
    if (!definition.factory) {
        return "es fehlt die Kernel-Fabrik";
    }
    for (const graph::PortDescriptor& port : descriptor.ports) {
        if (port.direction == graph::PortDirection::Output && !isValuePortType(port.type)) {
            return "der Ausgang '" + port.key +
                   "' liefert weder Einzelwert noch Spektrum; Punktwolken und Bilder bekommen "
                   "eigene Kanäle";
        }
    }
    return {};
}

}  // namespace

void KernelInputs::set(std::string key, graph::ParameterValue value) {
    values_.insert_or_assign(std::move(key), std::move(value));
}

bool KernelInputs::has(std::string_view key) const { return values_.find(key) != values_.end(); }

double KernelInputs::number(std::string_view key, double fallback) const {
    if (const auto* real = findAs<double>(values_, key)) {
        return *real;
    }
    if (const auto* integer = findAs<std::int64_t>(values_, key)) {
        return static_cast<double>(*integer);
    }
    if (const auto* flag = findAs<bool>(values_, key)) {
        return *flag ? 1.0 : 0.0;
    }
    return fallback;
}

std::int64_t KernelInputs::integer(std::string_view key, std::int64_t fallback) const {
    const auto* integer = findAs<std::int64_t>(values_, key);
    return integer == nullptr ? fallback : *integer;
}

bool KernelInputs::boolean(std::string_view key, bool fallback) const {
    const auto* flag = findAs<bool>(values_, key);
    return flag == nullptr ? fallback : *flag;
}

ColorRgba KernelInputs::color(std::string_view key, ColorRgba fallback) const {
    const auto* color = findAs<ColorRgba>(values_, key);
    return color == nullptr ? fallback : *color;
}

void KernelOutputs::set(std::string_view portKey, graph::ParameterValue value) {
    values_.insert_or_assign(std::string{portKey}, std::move(value));
}

void KernelOutputs::setSpectrum(std::string_view portKey, const Spectrum& spectrum) {
    spectra_.insert_or_assign(std::string{portKey}, spectrum);
}

std::unique_ptr<ICpuKernel> CpuKernelLibrary::create(std::string_view typeId) const {
    const auto it = factories_.find(typeId);
    return it == factories_.end() ? nullptr : it->second();
}

bool CpuKernelLibrary::contains(std::string_view typeId) const {
    return factories_.find(typeId) != factories_.end();
}

Result<void> CpuKernelLibrary::add(std::string typeId, KernelFactory factory) {
    if (!factory) {
        return Error{ErrorCode::InvalidArgument, "Für '" + typeId + "' fehlt die Kernel-Fabrik"};
    }
    if (contains(typeId)) {
        return Error{ErrorCode::AlreadyExists, "Für '" + typeId + "' gibt es bereits einen Kernel"};
    }
    factories_.emplace(std::move(typeId), std::move(factory));
    return {};
}

Result<void> registerCpuDefinitions(std::span<const CpuNodeDefinition> definitions,
                                    graph::INodeRegistrar& registrar, CpuKernelLibrary& library) {
    for (const CpuNodeDefinition& definition : definitions) {
        if (const std::string problem = validate(definition); !problem.empty()) {
            return Error{ErrorCode::InvalidArgument,
                         "CPU-Node '" + definition.descriptor.typeId + "': " + problem};
        }
        if (library.contains(definition.descriptor.typeId)) {
            return Error{ErrorCode::AlreadyExists, "Für '" + definition.descriptor.typeId +
                                                       "' gibt es bereits einen Kernel"};
        }
        if (Result<void> registered = registrar.registerNode(definition.descriptor); !registered.ok()) {
            return registered;
        }
        if (Result<void> added = library.add(definition.descriptor.typeId, definition.factory);
            !added.ok()) {
            return added;
        }
    }
    return {};
}

}  // namespace whitehole::values
