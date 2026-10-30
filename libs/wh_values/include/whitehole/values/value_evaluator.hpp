// value_evaluator.hpp: wertet die CPU-Nodes eines Graphen einmal pro Frame aus.
//
// Warum es diese Datei gibt: Jemand muss die Kernel in der richtigen Reihenfolge
// aufrufen, ihnen ihre Eingangswerte reichen (gespeichert oder von einem anderen
// Kernel moduliert), ihre Instanzen über Frames hinweg am Leben halten und die
// Ergebnisse prüfen. Das ist der Evaluator und nur das.
//
// Ablauf pro Frame:
//   1. Der ValueStore wird geleert.
//   2. Nodes in Abhängigkeitsreihenfolge (Kahn): Jeder Kernel sieht die Werte
//      seiner Vorgänger aus demselben Frame, nie die des vorigen.
//   3. Danach liest der Compiler den Store (packParameters) und lädt den
//      Parameterpuffer auf die GPU.
//
// Welche Nodes laufen? Nur solche, deren Ergebnis jemand braucht: Ein CPU-Node
// wird ausgewertet, wenn mindestens einer seiner Ausgänge zu einem Nicht-CPU-Node
// führt oder zu einem CPU-Node, der selbst gebraucht wird. CPU-Nodes ganz ohne
// Ausgänge (spätere Senken wie "OSC senden") laufen immer. So kostet ein
// vergessener, unverbundener Node nichts, auch wenn er teuer wäre.
//
// Bekannte Kosten: Pro Node und Frame entstehen kleine Maps für Ein- und Ausgänge.
// Bei zig Nodes ist das unerheblich; wird es messbar, kommt ein vorab berechneter
// Auswertungsplan, der nur bei Strukturänderungen neu entsteht.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>

#include "whitehole/core/ids.hpp"
#include "whitehole/core/result.hpp"
#include "whitehole/core/services.hpp"
#include "whitehole/graph/graph_model.hpp"
#include "whitehole/values/cpu_kernel.hpp"
#include "whitehole/values/value_store.hpp"

namespace whitehole::values {

class ValueEvaluator {
public:
    /// Die Bibliothek muss den Evaluator überleben.
    explicit ValueEvaluator(const ICpuKernelLibrary& kernels) : kernels_{kernels} {}

    /// Wertet den Graphen aus und füllt `store` neu.
    ///
    /// Fehler: CycleDetected (beschädigter Graph), NotFound (Typ ohne Kernel),
    /// InvalidArgument (Kernel liefert unbekannten Ausgang oder falschen Typ).
    /// Bei einem Fehler bleibt der Store mit den bis dahin berechneten Werten
    /// gefüllt, der fehlerhafte Node trägt nichts bei.
    Result<void> evaluate(const graph::Graph& graph, const FrameTime& time, ValueStore& store);

    /// Anzahl der lebenden Kernel-Instanzen (für Tests und Diagnose).
    [[nodiscard]] std::size_t instanceCount() const noexcept { return instances_.size(); }

private:
    struct Instance {
        std::string typeId;
        std::unique_ptr<ICpuKernel> kernel;
    };

    void dropStaleInstances(const graph::Graph& graph);
    Instance* instanceFor(const graph::NodeInstance& node);

    const ICpuKernelLibrary& kernels_;
    std::unordered_map<NodeId, Instance> instances_;
};

}  // namespace whitehole::values
