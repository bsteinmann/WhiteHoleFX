// app_settings.hpp: die Einstellungen des Programms (nicht des Projekts), gespeichert beim Nutzer.
//
// Warum es das gibt: Welche Netzwerkkarte sendet, ob DMX empfangen wird und welches Audiogerät läuft,
// hängt vom Rechner und vom Auftritt ab, nicht vom Projekt. Dieselben Werte lagen bisher verstreut in
// Menüs und QSettings-Schlüsseln. Hier steht alles an einer Stelle: ein einfacher Wertetyp, den das
// Dialogfenster bearbeitet und das Hauptfenster anwendet.
//
// Schlüssel in QSettings (Organisation und Anwendung "WhiteHoleFX"): dmx/artnet, dmx/sacn, audio/source
// wie bisher, neu dmx/artnetPort, dmx/sacnPort, network/outInterface, network/inInterface,
// nodeDefaults/<Typ-ID>/<Parameter> (Wert als Text, siehe encodeParameterValue).
#pragma once

#include <QString>

#include "whitehole/runtime/node_defaults.hpp"

namespace whitehole::studio {

struct AppSettings {
    bool artnetIn{false};   // Art-Net empfangen
    bool sacnIn{false};     // sACN empfangen
    int artnetPort{6454};
    int sacnPort{5568};
    /// IPv4-Adresse der Netzwerkkarte zum Senden bzw. Empfangen; leer = das System wählt.
    QString outputInterface;
    QString inputInterface;
    /// "off", "default", "device:<Name>" oder "file:<Pfad>".
    QString audioSource{QStringLiteral("off")};
    /// Eigene Standardwerte für neue Nodes (Seite "Node-Standardwerte").
    runtime::NodeDefaults nodeDefaults;

    bool operator==(const AppSettings&) const = default;
};

[[nodiscard]] AppSettings loadAppSettings();
void saveAppSettings(const AppSettings& settings);

}  // namespace whitehole::studio
