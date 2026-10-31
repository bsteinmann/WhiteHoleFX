// Tests des Readback-Rings: Slots vergeben und freigeben, Frames erst nach dem GPU-Callback ausliefern.
#include <catch2/catch_test_macros.hpp>

#include <cstddef>

#include "fake_gpu_device.hpp"
#include "whitehole/engine/latest_frame_mailbox.hpp"
#include "whitehole/engine/readback_ring.hpp"

using namespace whitehole;
using namespace whitehole::engine;
using whitehole::test::FakeGpuDevice;

TEST_CASE("ReadbackRing: ungültige Argumente werden abgelehnt") {
    FakeGpuDevice device;
    LatestFrameMailbox mailbox;

    const auto noSlots = ReadbackRing::create(device, mailbox, 0, 16);
    REQUIRE_FALSE(noSlots.ok());
    CHECK(noSlots.error().code == ErrorCode::InvalidArgument);

    const auto noBytes = ReadbackRing::create(device, mailbox, 2, 0);
    CHECK_FALSE(noBytes.ok());
}

TEST_CASE("ReadbackRing: Slots werden vergeben und wieder frei") {
    FakeGpuDevice device;
    LatestFrameMailbox mailbox;
    auto created = ReadbackRing::create(device, mailbox, 2, 16);
    REQUIRE(created.ok());
    ReadbackRing& ring = *created.value();

    CHECK(ring.freeSlots() == 2U);

    const auto first = ring.acquire();
    const auto second = ring.acquire();
    const auto third = ring.acquire();
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK_FALSE(third.has_value());
    CHECK(first->index != second->index);

    ring.release(*first);
    CHECK(ring.freeSlots() == 1U);
    CHECK(ring.acquire().has_value());
}

TEST_CASE("ReadbackRing: commit liefert den Frame erst nach dem GPU-Callback") {
    FakeGpuDevice device;
    LatestFrameMailbox mailbox;
    auto created = ReadbackRing::create(device, mailbox, 2, 16);
    REQUIRE(created.ok());
    ReadbackRing& ring = *created.value();

    const auto slot = ring.acquire();
    REQUIRE(slot.has_value());
    device.fill(slot->buffer, std::byte{0xAB});
    ring.commit(*slot, 7);

    CHECK(ring.freeSlots() == 1U);  // belegt, bis der Callback lief
    CHECK_FALSE(mailbox.takeLatest().has_value());

    device.poll();

    CHECK(ring.freeSlots() == 2U);
    const auto frame = mailbox.takeLatest();
    REQUIRE(frame.has_value());
    CHECK(frame->frame == 7U);
    REQUIRE(frame->bytes.size() == 16U);
    CHECK(std::to_integer<unsigned>(frame->bytes[0]) == 0xABU);
    CHECK(std::to_integer<unsigned>(frame->bytes[15]) == 0xABU);
}

TEST_CASE("ReadbackRing: Zerstören vor dem Callback ist harmlos") {
    FakeGpuDevice device;
    LatestFrameMailbox mailbox;
    device.holdReadbacks(true);

    {
        auto created = ReadbackRing::create(device, mailbox, 1, 16);
        REQUIRE(created.ok());
        ReadbackRing& ring = *created.value();
        const auto slot = ring.acquire();
        REQUIRE(slot.has_value());
        ring.commit(*slot, 1);
    }  // Ring weg, Readback noch offen, Puffer zerstört

    CHECK(device.liveBufferCount() == 0U);

    device.holdReadbacks(false);
    device.poll();  // der späte Callback darf nichts ausliefern und nicht abstürzen

    CHECK_FALSE(mailbox.takeLatest().has_value());
}
