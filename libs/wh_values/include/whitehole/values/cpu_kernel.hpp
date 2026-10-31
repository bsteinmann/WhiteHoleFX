// cpu_kernel.hpp: die Rechenvorschrift eines CPU-Nodes und ihre Registrierung.
//
// Warum es diese Datei gibt: Field-Nodes bestehen aus Beschreibung plus
// Shader-Snippet (wh_compiler/node_definition.hpp). CPU-Nodes sind das Gegenstück:
// Beschreibung plus Kernel, also C++-Code, der pro Frame Eingangswerte in
// Ausgangswerte verwandelt. Beide Sorten laufen über dieselbe Registry und
// teilen sich die Idee "Descriptor getrennt vom Code": Die GUI kennt nur den
// Descriptor, der Evaluator nur den Kernel.
//
// Zustand: Pro Node-Instanz im Graphen gibt es genau eine Kernel-Instanz, die
// über Frames hinweg lebt. Kernel dürfen darin Zustand halten (Glättung,
// Zähler). Wird der Node gelöscht oder wechselt sein Typ, verwirft der
// Evaluator die Instanz.
//
// Grenze: Kernel liefern Einzelwerte (Scalar, Integer, Boolean, Trigger, Color, Text)
// und Spektren (AudioSpectrum, eigener Kanal: KernelOutputs::setSpectrum). Punktwolken und
// Bilder bekommen eigene Kanäle, wenn die Nodes dazu entstehen.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/core/services.hpp"
#include "whitehole/core/spectrum.hpp"
#include "whitehole/core/types.hpp"
#include "whitehole/graph/node_descriptor.hpp"
#include "whitehole/graph/node_registry.hpp"

namespace whitehole::values {

/// Eingangswerte eines Nodes in einem Frame, nach Schlüssel. Für jeden Parameter
/// steht dessen aktueller Wert bereit (gespeichert oder moduliert), dazu die
/// Werte verbundener Eingänge ohne gleichnamigen Parameter. Fehlt ein Wert oder
/// hat er einen anderen Typ, liefern die Zugriffe den Rückfallwert.
class KernelInputs {
public:
    void set(std::string key, graph::ParameterValue value);

    [[nodiscard]] bool has(std::string_view key) const;
    /// double, int64 und bool (als 0 oder 1).
    [[nodiscard]] double number(std::string_view key, double fallback = 0.0) const;
    [[nodiscard]] std::int64_t integer(std::string_view key, std::int64_t fallback = 0) const;
    [[nodiscard]] bool boolean(std::string_view key, bool fallback = false) const;
    [[nodiscard]] ColorRgba color(std::string_view key, ColorRgba fallback = {}) const;

private:
    std::map<std::string, graph::ParameterValue, std::less<>> values_;
};

/// Was der Kernel in diesem Frame an seinen Ausgängen liefert. Der Typ jedes
/// Werts muss zum Portyp passen (Scalar: double, Integer: int64, Boolean und
/// Trigger: bool, Color: ColorRgba, Text: string), sonst meldet der Evaluator einen Fehler.
class KernelOutputs {
public:
    void set(std::string_view portKey, graph::ParameterValue value);

    [[nodiscard]] const std::map<std::string, graph::ParameterValue, std::less<>>& values() const noexcept {
        return values_;
    }

    /// Spektrum für einen Ausgang vom Typ AudioSpectrum (eigener Kanal, siehe IValueLookup::spectrum).
    void setSpectrum(std::string_view portKey, const Spectrum& spectrum);
    [[nodiscard]] const std::map<std::string, Spectrum, std::less<>>& spectra() const noexcept { return spectra_; }

private:
    std::map<std::string, graph::ParameterValue, std::less<>> values_;
    std::map<std::string, Spectrum, std::less<>> spectra_;
};

struct KernelContext {
    NodeId node;
    const FrameTime& time;
    const KernelInputs& inputs;
};

class ICpuKernel {
public:
    virtual ~ICpuKernel() = default;

    /// Läuft einmal pro Frame, auf dem Engine-Thread, und darf nicht blockieren.
    virtual void evaluate(const KernelContext& context, KernelOutputs& outputs) = 0;
};

using KernelFactory = std::function<std::unique_ptr<ICpuKernel>()>;

struct CpuNodeDefinition {
    graph::NodeDescriptor descriptor;  // domain muss Cpu sein
    KernelFactory factory;
};

class ICpuKernelLibrary {
public:
    virtual ~ICpuKernelLibrary() = default;

    /// Neue Kernel-Instanz für den Typ, oder nullptr, wenn er unbekannt ist.
    [[nodiscard]] virtual std::unique_ptr<ICpuKernel> create(std::string_view typeId) const = 0;
};

class CpuKernelLibrary final : public ICpuKernelLibrary {
public:
    [[nodiscard]] std::unique_ptr<ICpuKernel> create(std::string_view typeId) const override;
    [[nodiscard]] bool contains(std::string_view typeId) const;

    /// Fehler: InvalidArgument bei leerer Fabrik, AlreadyExists bei doppelter Typ-ID.
    Result<void> add(std::string typeId, KernelFactory factory);

private:
    std::map<std::string, KernelFactory, std::less<>> factories_;
};

/// Prüft jede Definition (Domäne Cpu, Fabrik vorhanden, Ausgänge liefern
/// Einzelwerte) und trägt Beschreibung und Kernel ein. Bricht beim ersten
/// Fehler ab; bereits eingetragene Definitionen bleiben bestehen. Eine
/// ungültige Definition wird gar nicht erst in die Registry geschrieben.
Result<void> registerCpuDefinitions(std::span<const CpuNodeDefinition> definitions,
                                    graph::INodeRegistrar& registrar, CpuKernelLibrary& library);

}  // namespace whitehole::values
