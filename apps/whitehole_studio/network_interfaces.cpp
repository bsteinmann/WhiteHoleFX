// network_interfaces.cpp: Umsetzung zu network_interfaces.hpp. Listet die Netzwerkkarten über QNetworkInterface auf.
#include "network_interfaces.hpp"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>

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
    return cards;
}

}  // namespace whitehole::studio
