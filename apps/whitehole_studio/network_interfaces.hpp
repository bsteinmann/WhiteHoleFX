// network_interfaces.hpp: die Netzwerkkarten des Rechners mit ihren IPv4-Adressen.
//
// Warum es das gibt: In den Einstellungen wählt man die Karte für Art-Net/sACN aus einer Liste, statt eine
// Adresse zu tippen. Die Auflistung ist Sache von Qt (QNetworkInterface), der Kern bekommt nur die gewählte
// IPv4-Adresse (UdpSender::bindInterface, DmxReceiverConfig::interfaceAddress).
#pragma once

#include <QList>
#include <QString>

namespace whitehole::studio {

struct NetworkCard {
    QString name;     // z. B. "eth0" oder "Ethernet"
    QString address;  // IPv4, z. B. "192.168.1.20"

    /// Text für die Auswahlliste: "eth0 – 192.168.1.20".
    [[nodiscard]] QString label() const;
};

/// Aktive Karten mit mindestens einer IPv4-Adresse (ohne Loopback), je Adresse ein Eintrag.
[[nodiscard]] QList<NetworkCard> networkCards();

}  // namespace whitehole::studio
