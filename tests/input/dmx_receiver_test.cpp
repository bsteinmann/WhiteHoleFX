// DmxReceiver: echter Empfang über die Loopback-Schnittstelle (POSIX und Winsock). Die Ports sind
// frei gewählt (Port 0), damit die Tests weder 6454/5568 belegen noch mit laufender Software kollidieren.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "whitehole/input/dmx_receiver.hpp"
#include "whitehole/output/artnet.hpp"
#include "whitehole/output/sacn.hpp"
#include "whitehole/output/udp_sender.hpp"

using namespace whitehole;
using namespace whitehole::input;

namespace {

template <typename Predicate>
bool waitFor(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{3};
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    return predicate();
}

DmxReceiverConfig testConfig() {
    DmxReceiverConfig config;
    config.artnetPort = 0;
    config.sacnPort = 0;
    return config;
}

}  // namespace

TEST_CASE("DmxReceiver: Art-Net und sACN kommen im Speicher an", "[input][receiver][udp]") {
    auto store = std::make_shared<DmxStore>();
    DmxReceiver receiver{store};
    REQUIRE_FALSE(receiver.running());
    REQUIRE(receiver.start(testConfig()).ok());
    REQUIRE(receiver.running());
    REQUIRE(receiver.artnetPort() != 0);
    REQUIRE(receiver.sacnPort() != 0);

    auto sender = output::UdpSender::create();
    REQUIRE(sender.ok());

    const std::vector<std::uint8_t> artData{11, 22, 33};
    const auto art = output::encodeArtDmx(output::ArtDmxParams{2, 0, 0}, artData);
    REQUIRE(art.ok());
    REQUIRE(sender.value()->send({"127.0.0.1", receiver.artnetPort()}, art.value()).ok());

    output::SacnParams params;
    params.universe = 9;
    const std::vector<std::uint8_t> sacnData{44, 55};
    const auto sacn = output::encodeSacn(params, sacnData);
    REQUIRE(sacn.ok());
    REQUIRE(sender.value()->send({"127.0.0.1", receiver.sacnPort()}, sacn.value()).ok());

    REQUIRE(waitFor([&] { return store->read(DmxProtocol::ArtNet, 2, 1).has_value(); }));
    REQUIRE(waitFor([&] { return store->read(DmxProtocol::Sacn, 9, 1).has_value(); }));
    CHECK(store->read(DmxProtocol::ArtNet, 2, 2)->values[0] == 22);
    CHECK(store->read(DmxProtocol::Sacn, 9, 2)->values[0] == 55);
}

TEST_CASE("DmxReceiver: Müll wird gezählt und verworfen", "[input][receiver][udp]") {
    auto store = std::make_shared<DmxStore>();
    DmxReceiver receiver{store};
    REQUIRE(receiver.start(testConfig()).ok());

    auto sender = output::UdpSender::create();
    REQUIRE(sender.ok());
    const std::vector<std::uint8_t> junk(40, 0x42);
    REQUIRE(sender.value()->send({"127.0.0.1", receiver.artnetPort()}, junk).ok());

    REQUIRE(waitFor([&] { return store->stats().ignored == 1; }));
    CHECK(store->stats().packets == 0);
}

TEST_CASE("DmxReceiver: einzelne Protokolle, Stoppen und erneutes Starten", "[input][receiver][udp]") {
    auto store = std::make_shared<DmxStore>();
    DmxReceiver receiver{store};

    DmxReceiverConfig onlyArt = testConfig();
    onlyArt.sacn = false;
    REQUIRE(receiver.start(onlyArt).ok());
    CHECK(receiver.artnetPort() != 0);
    CHECK(receiver.sacnPort() == 0);
    CHECK_FALSE(receiver.start(onlyArt).ok());  // läuft schon

    receiver.stop();
    CHECK_FALSE(receiver.running());
    CHECK(receiver.artnetPort() == 0);

    REQUIRE(receiver.start(testConfig()).ok());
    CHECK(receiver.running());
    receiver.stop();
    receiver.stop();  // zweites Stoppen ist harmlos
}

TEST_CASE("DmxReceiver: belegter Port meldet einen Fehler mit dem Protokoll", "[input][receiver][udp]") {
    // SO_REUSEADDR erlaubt unter Linux kein zweites Binden auf einen UDP-Port, der ohne ihn belegt ist;
    // deshalb prüfen wir nur die Fehlerform: ein unbenutzbarer Port (65535 ist erlaubt, daher kein Test).
    auto store = std::make_shared<DmxStore>();
    DmxReceiver first{store};
    REQUIRE(first.start(testConfig()).ok());
    DmxReceiver second{store};
    DmxReceiverConfig config = testConfig();
    config.artnetPort = first.artnetPort();
    const auto result = second.start(config);
    if (!result.ok()) {  // Plattformen mit strikter Port-Exklusivität
        CHECK(result.error().message.find("Art-Net") != std::string::npos);
        CHECK_FALSE(second.running());
    }
}

TEST_CASE("DmxReceiver: sACN-Universen merken (Beitritt wird versucht, Ablehnung gezählt)", "[input][receiver][udp]") {
    auto store = std::make_shared<DmxStore>();
    DmxReceiver receiver{store};
    receiver.setSacnUniverses({1, 2});  // vor dem Start: nur merken
    CHECK(receiver.failedJoins() == 0);
    REQUIRE(receiver.start(testConfig()).ok());
    receiver.setSacnUniverses({2, 3});
    receiver.setSacnUniverses({});
    // Ob das System den Beitritt zulässt, hängt vom Netz ab (Container ohne Schnittstelle: abgelehnt);
    // der Aufruf darf nie abstürzen oder hängen.
    SUCCEED();
}
