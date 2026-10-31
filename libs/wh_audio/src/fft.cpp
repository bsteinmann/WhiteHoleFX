// fft.cpp: Umsetzung zu fft.hpp (iterative Radix-2-FFT, Decimation in Time).
#include "whitehole/audio/fft.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace whitehole::audio {

Fft::Fft(std::size_t size) {
    size_ = 2;
    while (size_ < size) {
        size_ *= 2;
    }
    twiddle_.resize(size_ / 2);
    for (std::size_t k = 0; k < twiddle_.size(); ++k) {
        const double angle = -2.0 * std::numbers::pi * static_cast<double>(k) / static_cast<double>(size_);
        twiddle_[k] = {static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle))};
    }
    reverse_.resize(size_);
    std::size_t bits = 0;
    while ((std::size_t{1} << bits) < size_) {
        ++bits;
    }
    for (std::size_t i = 0; i < size_; ++i) {
        std::size_t r = 0;
        for (std::size_t b = 0; b < bits; ++b) {
            if ((i >> b) & 1U) {
                r |= std::size_t{1} << (bits - 1 - b);
            }
        }
        reverse_[i] = static_cast<std::uint32_t>(r);
    }
}

void Fft::forward(std::span<std::complex<float>> data) const {
    if (data.size() != size_) {
        return;
    }
    for (std::size_t i = 0; i < size_; ++i) {
        if (i < reverse_[i]) {
            std::swap(data[i], data[reverse_[i]]);
        }
    }
    for (std::size_t half = 1; half < size_; half *= 2) {
        const std::size_t step = size_ / (half * 2);
        for (std::size_t start = 0; start < size_; start += half * 2) {
            for (std::size_t k = 0; k < half; ++k) {
                const std::complex<float> t = twiddle_[k * step] * data[start + k + half];
                data[start + k + half] = data[start + k] - t;
                data[start + k] += t;
            }
        }
    }
}

}  // namespace whitehole::audio
