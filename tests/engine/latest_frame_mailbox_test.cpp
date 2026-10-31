// Tests der Mailbox zwischen Engine- und Output-Thread: neuester Frame gewinnt, Warten mit Zeitlimit.
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

#include "whitehole/engine/latest_frame_mailbox.hpp"

using namespace whitehole;
using namespace whitehole::engine;

TEST_CASE("Mailbox: leer liefert nichts") {
    LatestFrameMailbox mailbox;
    CHECK_FALSE(mailbox.takeLatest().has_value());
    CHECK(mailbox.droppedFrames() == 0U);
}

TEST_CASE("Mailbox: ein neuer Frame überschreibt den nicht abgeholten und wird gezählt") {
    LatestFrameMailbox mailbox;
    mailbox.publish(PixelFrame{1, {}});
    mailbox.publish(PixelFrame{2, {}});

    CHECK(mailbox.droppedFrames() == 1U);

    const auto frame = mailbox.takeLatest();
    REQUIRE(frame.has_value());
    CHECK(frame->frame == 2U);
    CHECK_FALSE(mailbox.takeLatest().has_value());
}

TEST_CASE("Mailbox: waitForFrame kehrt nach dem Timeout ohne Frame zurück") {
    LatestFrameMailbox mailbox;
    CHECK_FALSE(mailbox.waitForFrame(std::chrono::milliseconds{10}).has_value());
}

TEST_CASE("Mailbox: waitForFrame wacht auf, wenn ein anderer Thread veröffentlicht") {
    LatestFrameMailbox mailbox;

    std::thread producer{[&mailbox] {
        std::this_thread::sleep_for(std::chrono::milliseconds{20});
        mailbox.publish(PixelFrame{9, {}});
    }};

    const auto frame = mailbox.waitForFrame(std::chrono::milliseconds{2000});
    producer.join();

    REQUIRE(frame.has_value());
    CHECK(frame->frame == 9U);
}
