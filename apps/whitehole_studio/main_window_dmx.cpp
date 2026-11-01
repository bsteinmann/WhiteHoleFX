// main_window_dmx.cpp: DMX-Eingang im Hauptfenster (Empfänger starten und stoppen, Statusanzeige).
//
// Warum eine eigene Datei: Der Empfang ist eine Betriebsentscheidung wie die Netzwerkausgabe, kein
// Teil des Projekts. Die Fenster-Datei bleibt dadurch klein. Die Netzwerkarbeit selbst macht
// input::DmxReceiver (wh_input); hier stehen nur Schalter, das Merken der Wahl und die Anzeige.
//
// Sicherheit: Nach der Installation ist der Empfang AUS. Erst die Einstellung (Bearbeiten > Einstellungen,
// Seite DMX-Eingang) öffnet den UDP-Port (unter Windows fragt die Firewall dann nach). Die Wahl wird
// gemerkt, weil sie bewusst getroffen wurde.
#include <QCoreApplication>
#include <QStatusBar>
#include <QThread>

#include <cstdint>
#include <memory>
#include <vector>

#include "main_window.hpp"
#include "whitehole/output/artnet.hpp"
#include "whitehole/output/udp_sender.hpp"

namespace whitehole::studio {

QString MainWindow::applyDmxInput() {
    if (!dmxReceiver_) {
        dmxReceiver_ = std::make_unique<input::DmxReceiver>(library_->dmx);
    }
    dmxReceiver_->stop();
    if (!appSettings_.artnetIn && !appSettings_.sacnIn) {
        syncDmxUniverses();
        return {};
    }
    input::DmxReceiverConfig config;
    config.artnet = appSettings_.artnetIn;
    config.sacn = appSettings_.sacnIn;
    config.artnetPort = static_cast<std::uint16_t>(appSettings_.artnetPort);
    config.sacnPort = static_cast<std::uint16_t>(appSettings_.sacnPort);
    config.interfaceAddress = appSettings_.inputInterface.toStdString();
    const Result<void> started = dmxReceiver_->start(config);
    QString problem;
    if (!started.ok()) {
        // Port belegt oder verboten: beide Protokolle aus, damit die Anzeige nicht lügt.
        appSettings_.artnetIn = false;
        appSettings_.sacnIn = false;
        problem = tr("DMX-Eingang: %1").arg(QString::fromStdString(started.error().message));
    }
    syncDmxUniverses();
    return problem;
}

void MainWindow::syncDmxUniverses() {
    if (dmxReceiver_) {
        dmxReceiver_->setSacnUniverses(player_->dmxUsage().sacn);  // tut nichts, solange sich die Menge nicht ändert
    }
}

QString MainWindow::dmxStatusText() const {
    const bool listening = dmxReceiver_ && dmxReceiver_->running();
    const runtime::DmxUsage usage = player_->dmxUsage();
    if (!listening) {
        return usage.any() ? tr("   |   DMX-Nodes im Projekt, Eingang aus (Einstellungen)") : QString{};
    }
    const input::DmxStats stats = library_->dmx->stats();
    QString protocols;
    if (dmxReceiver_->artnetPort() != 0) {
        protocols += tr("Art-Net");
    }
    if (dmxReceiver_->sacnPort() != 0) {
        protocols += protocols.isEmpty() ? tr("sACN") : tr(" + sACN");
    }
    return tr("   |   DMX-Eingang %1: %2 Pakete").arg(protocols).arg(stats.packets);
}

QString MainWindow::exerciseDmx() {
    // Ende-zu-Ende über Loopback: Einstellung an -> Paket an den gewählten Port -> Wert im Speicher.
    const AppSettings before = appSettings_;
    AppSettings on = before;
    on.artnetIn = true;
    on.artnetPort = 0;  // freie Ports, damit der Test weder 6454 noch 5568 braucht
    on.sacnPort = 0;
    on.inputInterface = QStringLiteral("127.0.0.1");  // die Wahl der Karte darf den Empfang nicht stören
    if (const QString error = applySettings(on, false); !error.isEmpty()) {
        return QStringLiteral("DMX-Eingang: ") + error;
    }
    if (!dmxReceiver_ || !dmxReceiver_->running() || dmxReceiver_->artnetPort() == 0) {
        return QStringLiteral("DMX-Eingang: die Einstellung startet den Empfang nicht");
    }
    auto sender = output::UdpSender::create();
    if (!sender.ok()) {
        return QStringLiteral("DMX-Eingang: kein Sender für den Test");
    }
    const std::vector<std::uint8_t> channels{7, 77, 177};
    const auto packet = output::encodeArtDmx(output::ArtDmxParams{4, 0, 0}, channels);
    if (!packet.ok() || !sender.value()->send({"127.0.0.1", dmxReceiver_->artnetPort()}, packet.value()).ok()) {
        return QStringLiteral("DMX-Eingang: Testpaket nicht gesendet");
    }
    bool arrived = false;
    for (int i = 0; i < 300 && !arrived; ++i) {
        const auto sample = library_->dmx->read(input::DmxProtocol::ArtNet, 4, 3);
        arrived = sample && sample->values[0] == 177;
        if (!arrived) {
            QThread::msleep(10);
            QCoreApplication::processEvents();
        }
    }
    applySettings(before, false);
    if (dmxReceiver_->running() != (before.artnetIn || before.sacnIn)) {
        return QStringLiteral("DMX-Eingang: Ausschalten gescheitert");
    }
    return arrived ? QString{} : QStringLiteral("DMX-Eingang: das Testpaket kam nicht an");
}

}  // namespace whitehole::studio
