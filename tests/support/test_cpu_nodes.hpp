// test_cpu_nodes.hpp: kleine CPU-Nodes mit Kerneln für die Evaluator-Tests
// (unabhängig von den Built-ins in wh_nodes).
#pragma once

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "test_nodes.hpp"
#include "whitehole/graph/parameter_resolve.hpp"
#include "whitehole/values/cpu_kernel.hpp"

namespace whitehole::test {

/// Beobachtungspunkte, die Tests auslesen.
struct KernelProbes {
    int counterRuns{0};              // wie oft v.counter ausgewertet wurde
    int kernelsCreated{0};           // wie viele Kernel-Instanzen entstanden sind
    std::optional<double> sinkSeen;  // letzter Wert, den v.sink bekam
};

struct ValueFixture {
    std::shared_ptr<graph::NodeRegistry> registry = std::make_shared<graph::NodeRegistry>();
    values::CpuKernelLibrary kernels;
    std::shared_ptr<KernelProbes> probes = std::make_shared<KernelProbes>();
};

namespace detail {

template <typename Fn>
class LambdaKernel final : public values::ICpuKernel {
public:
    explicit LambdaKernel(Fn fn) : fn_{std::move(fn)} {}
    void evaluate(const values::KernelContext& context, values::KernelOutputs& outputs) override {
        fn_(context, outputs);
    }

private:
    Fn fn_;
};

/// Fabrik für einen zustandslosen Kernel aus einer Lambda-Funktion.
template <typename Fn>
values::KernelFactory stateless(std::shared_ptr<KernelProbes> probes, Fn fn) {
    return [probes = std::move(probes), fn = std::move(fn)]() -> std::unique_ptr<values::ICpuKernel> {
        ++probes->kernelsCreated;
        return std::make_unique<LambdaKernel<Fn>>(fn);
    };
}

class CounterKernel final : public values::ICpuKernel {
public:
    explicit CounterKernel(std::shared_ptr<KernelProbes> probes) : probes_{std::move(probes)} {}
    void evaluate(const values::KernelContext&, values::KernelOutputs& outputs) override {
        ++count_;
        ++probes_->counterRuns;
        outputs.set("out", static_cast<double>(count_));
    }

private:
    std::shared_ptr<KernelProbes> probes_;
    int count_{0};
};

inline values::CpuNodeDefinition definition(std::string typeId,
                                            std::vector<graph::PortDescriptor> ports,
                                            std::vector<graph::ParameterDescriptor> parameters,
                                            values::KernelFactory factory) {
    values::CpuNodeDefinition result;
    result.descriptor.typeId = std::move(typeId);
    result.descriptor.displayName = result.descriptor.typeId;
    result.descriptor.domain = graph::NodeDomain::Cpu;
    result.descriptor.ports = std::move(ports);
    result.descriptor.parameters = std::move(parameters);
    graph::addModulationPorts(result.descriptor);
    result.factory = std::move(factory);
    return result;
}

inline graph::ParameterDescriptor number(const std::string& key, double value) {
    return graph::ParameterDescriptor{key, key, value, std::nullopt, std::nullopt};
}

}  // namespace detail

/// Typen:
///  v.const     Ausgang out = Parameter value                     (modulierbar)
///  v.add       Ausgang out = a + b                               (a, b modulierbar)
///  v.counter   Ausgang out = Zähler 1, 2, 3, ... (hat Zustand)
///  v.pass      Eingang in (ohne Parameter), Ausgang out = in, oder -1 ohne Verbindung
///  v.clock     Ausgänge time, delta aus dem FrameTime
///  v.sink      keine Ausgänge; merkt sich den Wert von Eingang value in probes->sinkSeen
///  v.badtype   Ausgang out, liefert aber einen Text
///  v.ghost     Ausgang out, liefert aber den nicht vorhandenen Ausgang "nope"
///  v.nokernel  Cpu-Beschreibung ohne Kernel in der Bibliothek
///  v.field     Field-Domäne mit modulierbarem Parameter level (0..100); der "Verbraucher"
inline ValueFixture makeValueFixture() {
    using graph::PortType;
    ValueFixture fixture;
    const auto probes = fixture.probes;
    std::vector<values::CpuNodeDefinition> definitions;

    definitions.push_back(detail::definition(
        "v.const", {outPort("out", PortType::Scalar)}, {detail::number("value", 0.0)},
        detail::stateless(probes, [](const values::KernelContext& c, values::KernelOutputs& o) {
            o.set("out", c.inputs.number("value"));
        })));

    definitions.push_back(detail::definition(
        "v.add", {outPort("out", PortType::Scalar)},
        {detail::number("a", 0.0), detail::number("b", 0.0)},
        detail::stateless(probes, [](const values::KernelContext& c, values::KernelOutputs& o) {
            o.set("out", c.inputs.number("a") + c.inputs.number("b"));
        })));

    definitions.push_back(detail::definition(
        "v.counter", {outPort("out", PortType::Scalar)}, {},
        [probes]() -> std::unique_ptr<values::ICpuKernel> {
            ++probes->kernelsCreated;
            return std::make_unique<detail::CounterKernel>(probes);
        }));

    definitions.push_back(detail::definition(
        "v.pass", {inPort("in", PortType::Scalar), outPort("out", PortType::Scalar)}, {},
        detail::stateless(probes, [](const values::KernelContext& c, values::KernelOutputs& o) {
            o.set("out", c.inputs.number("in", -1.0));
        })));

    definitions.push_back(detail::definition(
        "v.clock", {outPort("time", PortType::Scalar), outPort("delta", PortType::Scalar)}, {},
        detail::stateless(probes, [](const values::KernelContext& c, values::KernelOutputs& o) {
            o.set("time", c.time.time.count());
            o.set("delta", c.time.delta.count());
        })));

    definitions.push_back(detail::definition(
        "v.sink", {}, {detail::number("value", 0.0)},
        detail::stateless(probes, [probes](const values::KernelContext& c, values::KernelOutputs&) {
            probes->sinkSeen = c.inputs.number("value");
        })));

    definitions.push_back(detail::definition(
        "v.badtype", {outPort("out", PortType::Scalar)}, {},
        detail::stateless(probes, [](const values::KernelContext&, values::KernelOutputs& o) {
            o.set("out", std::string{"kein Scalar"});
        })));

    definitions.push_back(detail::definition(
        "v.ghost", {outPort("out", PortType::Scalar)}, {},
        detail::stateless(probes, [](const values::KernelContext&, values::KernelOutputs& o) {
            o.set("nope", 1.0);
        })));

    // Die Beschreibung kommt ohne Kernel in die Registry (Fehler im Aufbau, den der Evaluator meldet).
    graph::NodeDescriptor noKernel;
    noKernel.typeId = "v.nokernel";
    noKernel.domain = graph::NodeDomain::Cpu;
    noKernel.ports = {outPort("out", PortType::Scalar)};
    mustRegister(*fixture.registry, noKernel);

    graph::NodeDescriptor consumer;
    consumer.typeId = "v.field";
    consumer.domain = graph::NodeDomain::Field;
    consumer.parameters = {graph::ParameterDescriptor{"level", "Level", 1.0, 0.0, 100.0}};
    consumer.ports = {outPort("out", PortType::Field)};
    graph::addModulationPorts(consumer);
    mustRegister(*fixture.registry, consumer);

    if (!values::registerCpuDefinitions(definitions, *fixture.registry, fixture.kernels).ok()) {
        std::abort();  // Fehler im Testaufbau
    }
    return fixture;
}

}  // namespace whitehole::test
