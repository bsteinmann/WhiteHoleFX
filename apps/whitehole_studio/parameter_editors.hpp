// parameter_editors.hpp: das passende Bedienelement zu einem Parameter.
//
// Warum es diese Datei gibt: Das Parameterfenster (parameter_panel.cpp) baut Gruppen und Zeilen; wie
// ein einzelner Wert bearbeitet wird (Regler, Zahlenfeld, Kästchen, Farbfläche, Textfeld), hängt nur
// vom Typ des Wertes ab und lebt hier. Trennung auch wegen der Dateigröße.
//
// Aufgabe: `makeEditor` liefert das Widget und Hilfsfunktionen, um einen gespeicherten Wert
// anzuzeigen, ohne Änderungssignale auszulösen. Änderungen meldet das Widget über `commit`.
// Abhängigkeiten: Qt Widgets, wh_runtime (ParameterInfo).
#pragma once

#include <QWidget>

#include <array>
#include <functional>
#include <optional>

#include "whitehole/runtime/player.hpp"

namespace whitehole::studio {

using Commit = std::function<void(graph::ParameterValue)>;
using Show = std::function<void(const graph::ParameterValue&)>;

struct Editor {
    QWidget* widget{nullptr};
    Show show;  // zeigt einen (gespeicherten) Wert an, ohne Änderungssignale auszulösen
    /// Zahlenregler: neuer Bereich des Schiebereglers (aus den Parametern "<name>Min"/"<name>Max").
    std::function<void(double, double)> setSliderRange;
    /// Wird mit dem gespeicherten Wert aufgerufen, nachdem der Parameter geändert wurde.
    std::function<void(const graph::ParameterValue&)> onStored;
};

/// Das Bedienelement für den Typ des Wertes in `info`. `sliderRange`: Bereich des Schiebereglers, falls
/// der Parameter einen hat.
Editor makeEditor(const runtime::ParameterInfo& info, const Commit& commit, QWidget* parent,
                  std::optional<std::array<double, 2>> sliderRange);

}  // namespace whitehole::studio
