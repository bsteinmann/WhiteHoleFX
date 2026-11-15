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
#include <QSignalBlocker>
#include <QSlider>
#include <QLabel>
#include <QStatusBar>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <set>
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
        std::set<std::uint32_t> sacn = player_->dmxUsage().sacn;
        if (appSettings_.masterDmx && appSettings_.masterDmxProtocol == 1) {
            sacn.insert(static_cast<std::uint32_t>(std::max(0, appSettings_.masterDmxUniverse)));  // der Master-Kanal auch
        }
        if (appSettings_.liveDmx && appSettings_.liveDmxProtocol == 1) {
            sacn.insert(static_cast<std::uint32_t>(std::max(0, appSettings_.liveDmxUniverse)));  // der Live-Look-Kanal auch
        }
        dmxReceiver_->setSacnUniverses(sacn);  // tut nichts, solange sich die Menge nicht ändert
    }
}

void MainWindow::applyMasterDmx() {
    if (!appSettings_.masterDmx) {
        player_->setMasterDmx(std::nullopt);
        return;
    }
    runtime::MasterDmxSource source;
    source.protocol = appSettings_.masterDmxProtocol == 1 ? input::DmxProtocol::Sacn : input::DmxProtocol::ArtNet;
    source.universe = static_cast<std::uint32_t>(std::max(0, appSettings_.masterDmxUniverse));
    source.channel = static_cast<std::uint32_t>(std::clamp(appSettings_.masterDmxChannel, 1, 512));
    player_->setMasterDmx(source);
}

void MainWindow::applyLiveDmx() {
    if (!appSettings_.liveDmx) {
        player_->setLiveDmx(std::nullopt);
        return;
    }
    runtime::LiveDmxSource source;
    source.protocol = appSettings_.liveDmxProtocol == 1 ? input::DmxProtocol::Sacn : input::DmxProtocol::ArtNet;
    source.universe = static_cast<std::uint32_t>(std::max(0, appSettings_.liveDmxUniverse));
    source.selectChannel = static_cast<std::uint32_t>(std::clamp(appSettings_.liveDmxSelectChannel, 1, 512));
    source.fadeChannel = static_cast<std::uint32_t>(std::clamp(appSettings_.liveDmxFadeChannel, 0, 512));
    player_->setLiveDmx(source);
}

void MainWindow::assignMasterDmx(input::DmxProtocol protocol, std::uint32_t universe, std::uint32_t channel) {
    AppSettings wanted = appSettings_;
    wanted.masterDmx = true;
    wanted.masterDmxProtocol = protocol == input::DmxProtocol::Sacn ? 1 : 0;
    wanted.masterDmxUniverse = static_cast<int>(universe);
    wanted.masterDmxChannel = static_cast<int>(channel);
    applySettings(wanted, true);
    updateStatus();
}

void MainWindow::assignLiveDmxSelect(input::DmxProtocol protocol, std::uint32_t universe, std::uint32_t channel) {
    AppSettings wanted = appSettings_;
    wanted.liveDmx = true;
    wanted.liveDmxProtocol = protocol == input::DmxProtocol::Sacn ? 1 : 0;
    wanted.liveDmxUniverse = static_cast<int>(universe);
    wanted.liveDmxSelectChannel = static_cast<int>(channel);
    applySettings(wanted, true);
    updateStatus();
}

void MainWindow::assignLiveDmxFade(input::DmxProtocol protocol, std::uint32_t universe, std::uint32_t channel) {
    // Protokoll/Universe teilt sich Live-DMX mit dem Auswahl-Kanal (ein LiveDmxSource für beide, siehe
    // live_dmx.hpp) - ein Rechtsklick hier verschiebt also auch den Auswahl-Kanal auf dieses Universe.
    AppSettings wanted = appSettings_;
    wanted.liveDmx = true;
    wanted.liveDmxProtocol = protocol == input::DmxProtocol::Sacn ? 1 : 0;
    wanted.liveDmxUniverse = static_cast<int>(universe);
    wanted.liveDmxFadeChannel = static_cast<int>(channel);
    applySettings(wanted, true);
    updateStatus();
}

void MainWindow::patchDmxValueNode(NodeId node, std::optional<std::size_t> lookIndex, input::DmxProtocol protocol,
                                    std::uint32_t universe, std::uint32_t channel) {
    // Dieselben drei Parameter, die runtime::findDmxListenerInGraph liest (dmx_listener.cpp) und die der
    // Node-Editor beim Verstellen von Hand setzt - kein neuer Mechanismus, nur von hier aus statt dort.
    // lookIndex (Bugfix: "DMX Out" fand einen Node nicht, der nur in einem gespeicherten Look steckt, siehe
    // CLAUDE.md) entscheidet, welcher Graph den Node tatsächlich enthält: setParameter() setzt immer den
    // gerade im Editor offenen Graphen voraus und würde einen Look-Node entweder gar nicht finden (Fehler,
    // kein Crash) oder - schlimmer, falls die NodeId zufällig im offenen Graphen existiert - den falschen
    // Node verändern. setLookNodeParameter() schreibt dagegen gezielt in den Graphen des betroffenen Looks.
    const auto report = [this](const Result<graph::ParameterValue>& result) {
        if (!result.ok()) {
            statusBar()->showMessage(QString::fromStdString(result.error().message), 8000);
        }
    };
    const auto setOne = [this, &lookIndex](NodeId target, const std::string& key, graph::ParameterValue value) {
        if (lookIndex.has_value()) {
            return player_->setLookNodeParameter(*lookIndex, target, key, std::move(value));
        }
        return player_->setParameter(target, key, std::move(value));
    };
    report(setOne(node, "source", graph::ParameterValue{std::int64_t{protocol == input::DmxProtocol::Sacn ? 1 : 0}}));
    report(setOne(node, "universe", graph::ParameterValue{static_cast<std::int64_t>(universe)}));
    report(setOne(node, "channel", graph::ParameterValue{static_cast<std::int64_t>(channel)}));
}

void MainWindow::patchDmxOutputValueNode(std::uint16_t poolChannel, std::uint16_t count, output::Protocol protocol,
                                          std::uint32_t universe, std::uint32_t channel) {
    // Gegenstück zu patchDmxValueNode, für die Schreib-Richtung: der Node selbst hat keine eigene Adresse
    // (nur seinen internen Pool-Kanal), die echte Adresse steckt in einem project::DmxChannelPatch. Kein
    // lookIndex nötig: dmxPatches ist projektweit, nicht Teil des Graphen, also unabhängig davon, ob der
    // Node gerade im offenen Graphen oder in einem Look steckt (siehe Player::patchDmxOutputValueNode).
    const Result<void> result = player_->patchDmxOutputValueNode(poolChannel, count, protocol, universe, channel);
    if (!result.ok()) {
        statusBar()->showMessage(QString::fromStdString(result.error().message), 8000);
    }
}

void MainWindow::followMasterDmx() {
    if (masterSlider_ == nullptr || masterLabel_ == nullptr) {
        return;
    }
    const bool fromDmx = player_->masterFromDmx();
    if (fromDmx) {
        const int percent = static_cast<int>(std::lround(player_->masterBrightness() * 100.0F));
        if (masterSlider_->value() != percent) {
            const QSignalBlocker block{masterSlider_};
            masterSlider_->setValue(percent);
            masterLabel_->setText(tr("%1 %").arg(percent));
        }
    }
    if (masterSlider_->isEnabled() == fromDmx) {
        masterSlider_->setEnabled(!fromDmx);
        masterSlider_->setToolTip(fromDmx ? tr("Der Master kommt gerade vom DMX-Kanal (Einstellungen → DMX-Eingang)")
                                          : tr("Gesamthelligkeit (wirkt auf Vorschau und Ausgabe, nicht im Projekt gespeichert)"));
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
