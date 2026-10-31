// udp_sender.cpp: Umsetzung zu udp_sender.hpp. Verschickt Pakete über einen UDP-Socket.
#include "whitehole/output/udp_sender.hpp"

#if defined(_WIN32)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstring>

namespace whitehole::output {

struct UdpSender::Impl {
    SOCKET fd{INVALID_SOCKET};
    bool started{false};
    std::map<std::pair<std::string, std::uint16_t>, sockaddr_in> cache;
};

UdpSender::UdpSender(std::unique_ptr<Impl> impl) : impl_{std::move(impl)} {}

UdpSender::~UdpSender() {
    if (impl_ && impl_->fd != INVALID_SOCKET) {
        ::closesocket(impl_->fd);
    }
    if (impl_ && impl_->started) {
        ::WSACleanup();
    }
}

Result<std::unique_ptr<UdpSender>> UdpSender::create() {
    auto impl = std::make_unique<Impl>();
    WSADATA data{};
    if (::WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return Error{ErrorCode::IoError, "Winsock ließ sich nicht starten"};
    }
    impl->started = true;
    impl->fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (impl->fd == INVALID_SOCKET) {
        const int code = ::WSAGetLastError();
        ::WSACleanup();
        impl->started = false;
        return Error{ErrorCode::IoError, "Der UDP-Socket ließ sich nicht anlegen (Winsock-Fehler " +
                                             std::to_string(code) + ")"};
    }
    u_long nonBlocking = 1;
    ::ioctlsocket(impl->fd, FIONBIO, &nonBlocking);
    const BOOL on = TRUE;
    ::setsockopt(impl->fd, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&on), sizeof(on));
    const int ttl = 1;
    ::setsockopt(impl->fd, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl), sizeof(ttl));
    return std::unique_ptr<UdpSender>(new UdpSender(std::move(impl)));
}

Result<void> UdpSender::send(const Endpoint& to, std::span<const std::uint8_t> packet) {
    const auto key = std::make_pair(to.address, to.port);
    auto it = impl_->cache.find(key);
    if (it == impl_->cache.end()) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(to.port);
        if (::InetPtonA(AF_INET, to.address.c_str(), &addr.sin_addr) != 1) {
            return Error{ErrorCode::InvalidArgument, "Keine gültige IPv4-Adresse: " + to.address};
        }
        it = impl_->cache.emplace(key, addr).first;
    }
    const int sent = ::sendto(impl_->fd, reinterpret_cast<const char*>(packet.data()),
                              static_cast<int>(packet.size()), 0,
                              reinterpret_cast<const sockaddr*>(&it->second), sizeof(sockaddr_in));
    if (sent == SOCKET_ERROR) {
        return Error{ErrorCode::IoError,
                     "Senden fehlgeschlagen (Winsock-Fehler " + std::to_string(::WSAGetLastError()) + ")"};
    }
    if (static_cast<std::size_t>(sent) != packet.size()) {
        return Error{ErrorCode::IoError, "Das Paket wurde nur teilweise gesendet"};
    }
    return {};
}

}  // namespace whitehole::output

#else

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace whitehole::output {

struct UdpSender::Impl {
    int fd{-1};
    std::map<std::pair<std::string, std::uint16_t>, sockaddr_in> cache;
};

UdpSender::UdpSender(std::unique_ptr<Impl> impl) : impl_{std::move(impl)} {}

UdpSender::~UdpSender() {
    if (impl_ && impl_->fd >= 0) {
        ::close(impl_->fd);
    }
}

Result<std::unique_ptr<UdpSender>> UdpSender::create() {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return Error{ErrorCode::IoError, std::string{"Der UDP-Socket ließ sich nicht anlegen: "} + std::strerror(errno)};
    }
    const int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on));
    const unsigned char ttl = 1;
    ::setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    auto impl = std::make_unique<Impl>();
    impl->fd = fd;
    return std::unique_ptr<UdpSender>(new UdpSender(std::move(impl)));
}

Result<void> UdpSender::send(const Endpoint& to, std::span<const std::uint8_t> packet) {
    const auto key = std::make_pair(to.address, to.port);
    auto it = impl_->cache.find(key);
    if (it == impl_->cache.end()) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(to.port);
        if (::inet_pton(AF_INET, to.address.c_str(), &addr.sin_addr) != 1) {
            return Error{ErrorCode::InvalidArgument, "Keine gültige IPv4-Adresse: " + to.address};
        }
        it = impl_->cache.emplace(key, addr).first;
    }
    const ssize_t sent = ::sendto(impl_->fd, packet.data(), packet.size(), MSG_DONTWAIT,
                                  reinterpret_cast<const sockaddr*>(&it->second), sizeof(sockaddr_in));
    if (sent < 0) {
        return Error{ErrorCode::IoError, std::string{"Senden fehlgeschlagen: "} + std::strerror(errno)};
    }
    if (static_cast<std::size_t>(sent) != packet.size()) {
        return Error{ErrorCode::IoError, "Das Paket wurde nur teilweise gesendet"};
    }
    return {};
}

}  // namespace whitehole::output

#endif
