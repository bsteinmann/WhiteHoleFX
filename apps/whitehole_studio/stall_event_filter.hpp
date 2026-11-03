// stall_event_filter.hpp: zählt alle Qt-Ereignisse für den Wächter (stall_watchdog.hpp).
//
// Warum es das gibt (Issue #26): Der erste Aufrufstapel eines Freezes zeigte den Hauptthread mitten im
// Ein- und Ausblenden von Widgets durch eine Queued-Slot-Meldung. Ein einzelner Stapel sagt nicht, welches
// Widget das ist und ob es sich um eine Schleife handelt. Dieser Filter meldet jedes Ereignis mit Klassenname
// und Typ an den Wächter; der zeigt bei einem Stillstand, was in der letzten Sekunde am häufigsten kam.
#pragma once

#include <QEvent>
#include <QObject>

#include "stall_watchdog.hpp"

namespace whitehole::studio {

class StallEventFilter final : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        stallCountEvent(watched->metaObject()->className(), static_cast<int>(event->type()));
        return false;
    }
};

}  // namespace whitehole::studio
