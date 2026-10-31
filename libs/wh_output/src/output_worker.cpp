// output_worker.cpp: Umsetzung zu output_worker.hpp. Der Ausgabe-Thread, der Frames aus der Engine entgegennimmt.
#include "whitehole/output/output_worker.hpp"

#include <chrono>
#include <utility>

namespace whitehole::output {

OutputWorker::OutputWorker(std::unique_ptr<OutputPipeline> pipeline)
    : pipeline_{std::move(pipeline)}, thread_{[this] { run(stop_); }} {}

OutputWorker::~OutputWorker() {
    stop_.store(true);
    thread_.join();
}

void OutputWorker::run(const std::atomic<bool>& stop) {
    using namespace std::chrono_literals;
    while (!stop.load()) {
        // Kurzes Timeout, damit der Thread das Stop-Flag regelmäßig sieht.
        std::optional<engine::PixelFrame> frame = mailbox_.waitForFrame(20ms);
        if (!frame) {
            continue;
        }
        const Result<void> ok = pipeline_->processBytes(frame->bytes);
        if (!ok.ok()) {
            frameErrors_.fetch_add(1);
            const std::lock_guard<std::mutex> lock{errorMutex_};
            lastError_ = ok.error();
        }
    }
}

std::optional<Error> OutputWorker::lastFrameError() const {
    const std::lock_guard<std::mutex> lock{errorMutex_};
    return lastError_;
}

}  // namespace whitehole::output
