// UdpSender: echter Versand über die Loopback-Schnittstelle (POSIX und Winsock).
#include <catch2/catch_test_macros.hpp>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <memory>
#include <string>
#include <vector>

#include "whitehole/output/artnet.hpp"
#include "whitehole/output/output_pipeline.hpp"
#include "whitehole/output/udp_sender.hpp"

using namespace whitehole;
using namespace whitehole::mapping;
using namespace whitehole::output;

namespace {

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

/// Empfänger auf 127.0.0.1 mit zufällig gewähltem Port und 2 s Timeout.
class Receiver {
public:
    Receiver() {
#if defined(_WIN32)
        WSADATA data{};
        REQUIRE(::WSAStartup(MAKEWORD(2, 2), &data) == 0);
#endif
        fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        REQUIRE(fd_ != kInvalidSocket);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        REQUIRE(::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
#if defined(_WIN32)
        int length = sizeof(addr);
#else
        socklen_t length = sizeof(addr);
#endif
        REQUIRE(::getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &length) == 0);
        port_ = ntohs(addr.sin_port);
#if defined(_WIN32)
        const DWORD timeoutMs = 2000;
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
#else
        timeval timeout{2, 0};
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
    }
    ~Receiver() {
#if defined(_WIN32)
        ::closesocket(fd_);
        ::WSACleanup();
#else
        ::close(fd_);
#endif
    }
    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    [[nodiscard]] std::uint16_t port() const { return port_; }

    [[nodiscard]] std::vector<std::uint8_t> receive() const {
        std::vector<std::uint8_t> buffer(2048);
#if defined(_WIN32)
        const int n = ::recv(fd_, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
        buffer.resize(n > 0 ? static_cast<std::size_t>(n) : 0);
#else
        const ssize_t n = ::recv(fd_, buffer.data(), buffer.size(), 0);
        buffer.resize(n > 0 ? static_cast<std::size_t>(n) : 0);
#endif
        return buffer;
    }

private:
    SocketHandle fd_{kInvalidSocket};
    std::uint16_t port_{0};
};

}  // namespace

TEST_CASE("UdpSender liefert die Bytes unverändert an", "[output][udp]") {
    const Receiver receiver;
    Result<std::unique_ptr<UdpSender>> sender = UdpSender::create();
    REQUIRE(sender.ok());

    const std::vector<std::uint8_t> packet{1, 2, 3, 4, 5, 250};
    REQUIRE(sender.value()->send({"127.0.0.1", receiver.port()}, packet).ok());
    CHECK(receiver.receive() == packet);
    // Zweites Paket an dasselbe Ziel (benutzt die gemerkte Adresse).
    REQUIRE(sender.value()->send({"127.0.0.1", receiver.port()}, packet).ok());
    CHECK(receiver.receive() == packet);
}

TEST_CASE("UdpSender lehnt Adressen ab, die keine IPv4-Zahl sind", "[output][udp]") {
    Result<std::unique_ptr<UdpSender>> sender = UdpSender::create();
    REQUIRE(sender.ok());
    const std::vector<std::uint8_t> packet{1};
    const Result<void> bad = sender.value()->send({"nicht-eine-adresse", 6454}, packet);
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.error().code == ErrorCode::InvalidArgument);
}

TEST_CASE("UdpSender meldet einen Fehler, wenn das System das Paket nicht annimmt", "[output][udp]") {
    Result<std::unique_ptr<UdpSender>> sender = UdpSender::create();
    REQUIRE(sender.ok());
    const std::vector<std::uint8_t> tooBig(70000, 0);  // über der UDP-Grenze von 65507 Bytes
    const Result<void> bad = sender.value()->send({"127.0.0.1", 9}, tooBig);
    REQUIRE_FALSE(bad.ok());
    CHECK(bad.error().code == ErrorCode::IoError);
    // Die Meldung nennt den Systemfehler, nicht den Teilversand-Fall.
    CHECK(bad.error().message.find("Senden fehlgeschlagen") != std::string::npos);
}

TEST_CASE("Pipeline und UdpSender: Art-Net kommt als echtes Paket an", "[output][udp]") {
    const Receiver receiver;
    Result<std::unique_ptr<UdpSender>> sender = UdpSender::create();
    REQUIRE(sender.ok());

    LedMap map;
    map.ranges.push_back({FixtureId{1}, 0, 2});
    map.world.assign(2, Vec3{});
    map.unit.assign(2, Vec3{});
    FixturePatch patch;
    patch.fixture = FixtureId{1};
    patch.universe = 7;
    patch.color.gamma = 1.0F;
    Result<OutputPlan> plan = OutputPlan::create(map, std::vector<FixturePatch>{patch});
    REQUIRE(plan.ok());
    OutputConfig config;
    config.destinations.push_back({"127.0.0.1", receiver.port()});
    Result<std::unique_ptr<OutputPipeline>> pipeline =
        OutputPipeline::create(std::move(plan.value()), std::move(config), *sender.value());
    REQUIRE(pipeline.ok());

    REQUIRE(pipeline.value()->process(std::vector<ColorRgba>{{1, 0, 0, 1}, {0, 1, 0, 1}}).ok());
    const std::vector<std::uint8_t> got = receiver.receive();

    ArtDmxParams params;
    params.portAddress = 7;
    params.sequence = 1;
    const std::vector<std::uint8_t> channels{255, 0, 0, 0, 255, 0};
    CHECK(got == encodeArtDmx(params, channels).value());
    CHECK(pipeline.value()->stats().sendErrors == 0);
}

TEST_CASE("UdpSender: Netzwerkkarte wählen (bindInterface)", "[output][udp]") {
    auto sender = UdpSender::create();
    REQUIRE(sender.ok());
    // Eine Adresse, die dem Rechner nicht gehört, oder Unsinn: Fehler, die Wahl bleibt bestehen.
    CHECK_FALSE(sender.value()->bindInterface("kein-ip").ok());
    CHECK_FALSE(sender.value()->bindInterface("203.0.113.77").ok());  // TEST-NET-3, nie lokal
    // Loopback ist auf jedem Rechner eine Karte; danach kommt das Paket weiter an.
    REQUIRE(sender.value()->bindInterface("127.0.0.1").ok());
    Receiver receiver;
    const std::vector<std::uint8_t> payload{1, 2, 3, 4};
    REQUIRE(sender.value()->send({"127.0.0.1", receiver.port()}, payload).ok());
    CHECK(receiver.receive() == payload);
    // Zurück auf "das System wählt".
    REQUIRE(sender.value()->bindInterface("").ok());
    REQUIRE(sender.value()->send({"127.0.0.1", receiver.port()}, payload).ok());
    CHECK(receiver.receive() == payload);
}
