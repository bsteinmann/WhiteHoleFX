#include "udp_listener.hpp"

#include <cstring>
#include <string>

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
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#endif

namespace whitehole::input {

#if defined(_WIN32)
using Handle = SOCKET;
constexpr Handle kInvalid = INVALID_SOCKET;
using OptPtr = const char*;
#else
using Handle = int;
constexpr Handle kInvalid = -1;
using OptPtr = const void*;
#endif

struct UdpListener::Impl {
    Handle fd{kInvalid};
    std::uint16_t port{0};
#if defined(_WIN32)
    bool wsaStarted{false};
#endif
};

namespace {

std::string lastError() {
#if defined(_WIN32)
    return "Winsock-Fehler " + std::to_string(::WSAGetLastError());
#else
    return std::strerror(errno);
#endif
}

in_addr sacnGroup(std::uint32_t universe) {
    in_addr address{};
    address.s_addr = htonl((239U << 24) | (255U << 16) | ((universe >> 8) & 0xFFU) << 8 | (universe & 0xFFU));
    return address;
}

bool membership(Handle fd, std::uint32_t universe, bool join) {
    ip_mreq request{};
    request.imr_multiaddr = sacnGroup(universe);
    request.imr_interface.s_addr = htonl(INADDR_ANY);
    return ::setsockopt(fd, IPPROTO_IP, join ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP,
                        reinterpret_cast<OptPtr>(&request), sizeof(request)) == 0;
}

}  // namespace

UdpListener::UdpListener(std::unique_ptr<Impl> impl) : impl_{std::move(impl)} {}

UdpListener::~UdpListener() {
    if (!impl_) {
        return;
    }
#if defined(_WIN32)
    if (impl_->fd != kInvalid) {
        ::closesocket(impl_->fd);
    }
    if (impl_->wsaStarted) {
        ::WSACleanup();
    }
#else
    if (impl_->fd != kInvalid) {
        ::close(impl_->fd);
    }
#endif
}

Result<std::unique_ptr<UdpListener>> UdpListener::open(std::uint16_t port) {
    auto impl = std::make_unique<Impl>();
#if defined(_WIN32)
    WSADATA data{};
    if (::WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return Error{ErrorCode::IoError, "Winsock ließ sich nicht starten"};
    }
    impl->wsaStarted = true;
#endif
    // Ab hier räumt der Destruktor von UdpListener auf, auch bei einem Fehler.
    std::unique_ptr<UdpListener> listener{new UdpListener(std::move(impl))};
    Impl& state = *listener->impl_;

    state.fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (state.fd == kInvalid) {
        return Error{ErrorCode::IoError, "Socket nicht anlegbar (" + lastError() + ")"};
    }
    const int on = 1;
    ::setsockopt(state.fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<OptPtr>(&on), sizeof(on));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (::bind(state.fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        return Error{ErrorCode::IoError, "Port " + std::to_string(port) + " nicht bindbar (" + lastError() + ")"};
    }
    sockaddr_in bound{};
#if defined(_WIN32)
    int length = sizeof(bound);
#else
    socklen_t length = sizeof(bound);
#endif
    if (::getsockname(state.fd, reinterpret_cast<sockaddr*>(&bound), &length) != 0) {
        return Error{ErrorCode::IoError, "Port nicht ermittelbar (" + lastError() + ")"};
    }
    state.port = ntohs(bound.sin_port);
    return listener;
}

int UdpListener::receive(std::span<std::uint8_t> buffer, int timeoutMs) {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(impl_->fd, &readSet);
    timeval timeout{};
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    const int ready = ::select(static_cast<int>(impl_->fd) + 1, &readSet, nullptr, nullptr, &timeout);
    if (ready <= 0) {
        return ready == 0 ? 0 : -1;
    }
#if defined(_WIN32)
    const int n = ::recv(impl_->fd, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0);
    return n == SOCKET_ERROR ? -1 : n;
#else
    const ssize_t n = ::recv(impl_->fd, buffer.data(), buffer.size(), 0);
    return n < 0 ? -1 : static_cast<int>(n);
#endif
}

std::uint16_t UdpListener::port() const { return impl_->port; }

bool UdpListener::joinSacnGroup(std::uint32_t universe) { return membership(impl_->fd, universe, true); }

bool UdpListener::leaveSacnGroup(std::uint32_t universe) { return membership(impl_->fd, universe, false); }

}  // namespace whitehole::input
