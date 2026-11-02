// color_pick.hpp: Regeln für die Farbwahl der Oberfläche, ohne Qt, damit sie sich testen lassen.
//
// Warum es diese Datei gibt (Issue #20): Der Farbdialog startet mit der aktuellen Farbe samt Alpha. Viele
// Farbparameter beginnen durchsichtig (Alpha 0, z. B. der Hintergrund des Text-Nodes). Wer dort eine Farbe wählt,
// bekam wieder Alpha 0 zurück: "nach dem Wählen ist die Farbe transparent", und ein Text in einer so gewählten
// Farbe blieb unsichtbar. Eine Farbe zu wählen heißt, sie sehen zu wollen: Der Dialog startet deshalb immer
// mit sichtbarem Alpha, und ein Ergebnis mit Alpha 0 wird sichtbar. Durchsichtig macht man mit dem Schalter
// "Transparent" (er stellt Alpha auf 0 und zurück auf 1).
#pragma once

#include "whitehole/core/types.hpp"

namespace whitehole::runtime {

/// Farbe, mit der der Farbdialog startet: die aktuelle, aber nie mit Alpha 0.
[[nodiscard]] inline ColorRgba colorPickerStart(ColorRgba current) {
    if (!(current.a > 0.0F)) {  // 0, negativ und NaN
        current.a = 1.0F;
    }
    return current;
}

/// Farbe, die nach dem Dialog gilt: die gewählte, aber nie mit Alpha 0.
[[nodiscard]] inline ColorRgba colorFromPicker(ColorRgba picked) { return colorPickerStart(picked); }

}  // namespace whitehole::runtime
