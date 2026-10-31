// main_window_dmx.cpp: DMX-Eingang im Hauptfenster (Menü "Eingang", Statusanzeige).
//
// Warum eine eigene Datei: Der Empfang ist eine Betriebsentscheidung wie die Netzwerkausgabe, kein
// Teil des Projekts. Die Fenster-Datei bleibt dadurch klein. Die Netzwerkarbeit selbst macht
// input::DmxReceiver (wh_input); hier stehen nur Schalter, das Merken der Wahl und die Anzeige.
//
// Sicherheit: Nach der Installation ist der Empfang AUS. Ein Schalter im Menü öffnet den UDP-Port
// (unter Windows fragt die Firewall dann nach). Die Wahl wird gemerkt, weil sie bewusst getroffen wurde.
#include <QAction>
#include <QCoreApplication>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QThread>

#include <cstdint>
#include <memory>
#include <vector>

#include "main_window.hpp"
#include "whitehole/output/artnet.hpp"
#include "whitehole/output/udp_sender.hpp"

namespace whitehole::studio {

namespace {

QSettings settings() { return QSettings{QStringLiteral("WhiteHoleFX"), QStringLiteral("WhiteHoleFX")}; }

}  // namespace

void MainWindow::buildDmxInput() {
    artnetInAction_ = new QAction{tr("&Art-Net empfangen (UDP %1)").arg(input::kArtNetListenPort), this};
    sacnInAction_ = new QAction{tr("&sACN empfangen (UDP %1)").arg(input::kSacnListenPort), this};
    for (QAction* action : {artnetInAction_, sacnInAction_}) {
        action->setCheckable(true);
        connect(action, &QAction::toggled, this, [this]() { applyDmxInput(); });
    }
    artnetInAction_->setToolTip(tr("DMX über Art-Net empfangen; die DMX-Nodes im Projekt lesen daraus"));
    sacnInAction_->setToolTip(tr("DMX über sACN (E1.31) empfangen, Unicast und Multicast"));

    QMenu* menu = menuBar()->addMenu(tr("&Eingang"));
    menu->addAction(artnetInAction_);
    menu->addAction(sacnInAction_);
    menu->addSeparator();
    buildAudioInput(menu);

    const QSettings s = settings();
    {
        const QSignalBlocker blockArt{artnetInAction_};
        const QSignalBlocker blockSacn{sacnInAction_};
        artnetInAction_->setChecked(s.value(QStringLiteral("dmx/artnet"), false).toBool());
        sacnInAction_->setChecked(s.value(QStringLiteral("dmx/sacn"), false).toBool());
    }
    applyDmxInput();
}

void MainWindow::applyDmxInput() {
    if (!dmxReceiver_) {
        dmxReceiver_ = std::make_unique<input::DmxReceiver>(library_->dmx);
    }
    dmxReceiver_->stop();
    if (artnetInAction_->isChecked() || sacnInAction_->isChecked()) {
        input::DmxReceiverConfig config = dmxConfig_;
        config.artnet = artnetInAction_->isChecked();
        config.sacn = sacnInAction_->isChecked();
        const Result<void> started = dmxReceiver_->start(config);
        if (!started.ok()) {
            // Port belegt oder verboten: beide Schalter zurück, damit die Anzeige nicht lügt.
            const QSignalBlocker blockArt{artnetInAction_};
            const QSignalBlocker blockSacn{sacnInAction_};
            artnetInAction_->setChecked(false);
            sacnInAction_->setChecked(false);
            statusBar()->showMessage(tr("DMX-Eingang: %1").arg(QString::fromStdString(started.error().message)), 10000);
        }
    }
    QSettings s = settings();
    s.setValue(QStringLiteral("dmx/artnet"), artnetInAction_->isChecked());
    s.setValue(QStringLiteral("dmx/sacn"), sacnInAction_->isChecked());
    syncDmxUniverses();
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
        return usage.any() ? tr("   |   DMX-Nodes im Projekt, Eingang aus (Menü Eingang)") : QString{};
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
    // Ende-zu-Ende über Loopback: Schalter an -> Paket an den gewählten Port -> Wert im Speicher.
    dmxConfig_.artnetPort = 0;  // freie Ports, damit der Test weder 6454 noch 5568 braucht
    dmxConfig_.sacnPort = 0;
    artnetInAction_->setChecked(true);
    if (!dmxReceiver_ || !dmxReceiver_->running() || dmxReceiver_->artnetPort() == 0) {
        return QStringLiteral("DMX-Eingang: der Schalter startet den Empfang nicht");
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
    artnetInAction_->setChecked(false);
    sacnInAction_->setChecked(false);
    dmxConfig_ = input::DmxReceiverConfig{};
    if (dmxReceiver_->running()) {
        return QStringLiteral("DMX-Eingang: Ausschalten gescheitert");
    }
    return arrived ? QString{} : QStringLiteral("DMX-Eingang: das Testpaket kam nicht an");
}

}  // namespace whitehole::studio
