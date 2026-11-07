// network_interfaces.cpp: Umsetzung zu network_interfaces.hpp. Listet die Netzwerkkarten über QNetworkInterface auf.
#include "network_interfaces.hpp"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>
#include <QObject>

namespace whitehole::studio {

QString NetworkCard::label() const { return QStringLiteral("%1 – %2").arg(name, address); }

QList<NetworkCard> networkCards() {
    QList<NetworkCard> cards;
    const auto required = QNetworkInterface::IsUp | QNetworkInterface::IsRunning;
    for (const QNetworkInterface& card : QNetworkInterface::allInterfaces()) {
        if ((card.flags() & required) != required || card.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        for (const QNetworkAddressEntry& entry : card.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() == QAbstractSocket::IPv4Protocol) {
                cards.append(NetworkCard{card.humanReadableName(), ip.toString()});
            }
        }
    }
    // Localhost zuletzt und immer: zum Testen auf demselben Rechner (Issue #35), auch wenn das System die
    // Loopback-Karte nicht als laufend meldet.
    cards.append(NetworkCard{QObject::tr("Localhost"), QStringLiteral("127.0.0.1")});
    return cards;
}

}  // namespace whitehole::studio
