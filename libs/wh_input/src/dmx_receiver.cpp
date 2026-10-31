// dmx_receiver.cpp: Umsetzung zu dmx_receiver.hpp. Empfängt Art-Net und sACN über UDP und schreibt die Kanalwerte in einen DmxStore.
#include "whitehole/input/dmx_receiver.hpp"

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

#include "whitehole/input/dmx_packets.hpp"
#include "udp_listener.hpp"

namespace whitehole::input {

namespace {

constexpr int kPollMs = 100;

/// Ein Socket mit Thread. `parse` wandelt Bytes in ein Frame.
class Channel {
public:
    using Parser = std::optional<DmxFrame> (*)(std::span<const std::uint8_t>);

    Channel(std::unique_ptr<UdpListener> socket, Parser parse, DmxStore& store)
        : socket_{std::move(socket)}, parse_{parse}, store_{store}, thread_{[this] { run(); }} {}

    ~Channel() {
        stop_ = true;
        thread_.join();
    }

    UdpListener& socket() { return *socket_; }

private:
    void run() {
        std::array<std::uint8_t, 1500> buffer{};
        while (!stop_) {
            const int n = socket_->receive(buffer, kPollMs);
            if (n <= 0) {
                continue;
            }
            const auto frame = parse_(std::span<const std::uint8_t>{buffer.data(), static_cast<std::size_t>(n)});
            if (frame) {
                store_.write(frame->protocol, frame->universe, frame->channels);
            } else {
                store_.countIgnored();
            }
        }
    }

    std::unique_ptr<UdpListener> socket_;
    Parser parse_;
    DmxStore& store_;
    std::atomic<bool> stop_{false};
    std::thread thread_;  // zuletzt: startet erst, wenn alles andere steht
};

}  // namespace

struct DmxReceiver::Impl {
    std::mutex mutex;  // schützt channels und universes gegen gleichzeitige Aufrufe vom Hauptthread
    std::unique_ptr<Channel> artnet;
    std::unique_ptr<Channel> sacn;
    std::set<std::uint32_t> wanted;
    std::set<std::uint32_t> joined;
    std::atomic<std::size_t> failedJoins{0};

    void applyUniverses() {
        if (!sacn) {
            return;
        }
        for (auto it = joined.begin(); it != joined.end();) {
            if (!wanted.contains(*it)) {
                sacn->socket().leaveSacnGroup(*it);
                it = joined.erase(it);
            } else {
                ++it;
            }
        }
        for (const std::uint32_t universe : wanted) {
            if (!joined.contains(universe)) {
                if (sacn->socket().joinSacnGroup(universe)) {
                    joined.insert(universe);
                } else {
                    ++failedJoins;
                }
            }
        }
    }
};

DmxReceiver::DmxReceiver(std::shared_ptr<DmxStore> store)
    : store_{std::move(store)}, impl_{std::make_unique<Impl>()} {}

DmxReceiver::~DmxReceiver() { stop(); }

Result<void> DmxReceiver::start(const DmxReceiverConfig& config) {
    const std::lock_guard<std::mutex> lock{impl_->mutex};
    if (impl_->artnet || impl_->sacn) {
        return Error{ErrorCode::InvalidArgument, "Der DMX-Empfang läuft schon"};
    }
    std::unique_ptr<Channel> artnet;
    std::unique_ptr<Channel> sacn;
    if (config.artnet) {
        auto socket = UdpListener::open(config.artnetPort);
        if (!socket) {
            return Error{ErrorCode::IoError, "Art-Net-Empfang: " + socket.error().message};
        }
        artnet = std::make_unique<Channel>(std::move(socket.value()), &parseArtNet, *store_);
    }
    if (config.sacn) {
        auto socket = UdpListener::open(config.sacnPort);
        if (!socket) {
            return Error{ErrorCode::IoError, "sACN-Empfang: " + socket.error().message};
        }
        sacn = std::make_unique<Channel>(std::move(socket.value()), &parseSacn, *store_);
    }
    impl_->artnet = std::move(artnet);
    impl_->sacn = std::move(sacn);
    impl_->joined.clear();
    impl_->applyUniverses();
    return {};
}

void DmxReceiver::stop() {
    const std::lock_guard<std::mutex> lock{impl_->mutex};
    impl_->artnet.reset();
    impl_->sacn.reset();
    impl_->joined.clear();
}

bool DmxReceiver::running() const {
    const std::lock_guard<std::mutex> lock{impl_->mutex};
    return impl_->artnet || impl_->sacn;
}

std::uint16_t DmxReceiver::artnetPort() const {
    const std::lock_guard<std::mutex> lock{impl_->mutex};
    return impl_->artnet ? impl_->artnet->socket().port() : 0;
}

std::uint16_t DmxReceiver::sacnPort() const {
    const std::lock_guard<std::mutex> lock{impl_->mutex};
    return impl_->sacn ? impl_->sacn->socket().port() : 0;
}

void DmxReceiver::setSacnUniverses(const std::set<std::uint32_t>& universes) {
    const std::lock_guard<std::mutex> lock{impl_->mutex};
    if (impl_->wanted == universes) {
        return;
    }
    impl_->wanted = universes;
    impl_->applyUniverses();
}

std::size_t DmxReceiver::failedJoins() const { return impl_->failedJoins; }

}  // namespace whitehole::input
