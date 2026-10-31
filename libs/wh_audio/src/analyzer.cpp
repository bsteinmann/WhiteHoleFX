// analyzer.cpp: Umsetzung zu analyzer.hpp.
#include "whitehole/audio/analyzer.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace whitehole::audio {

const std::array<double, kSpectrumBands + 1>& bandEdgesHz() {
    static const std::array<double, kSpectrumBands + 1> edges = [] {
        std::array<double, kSpectrumBands + 1> e{};
        const double ratio = std::pow(kBandHighHz / kBandLowHz, 1.0 / static_cast<double>(kSpectrumBands));
        double f = kBandLowHz;
        for (double& v : e) {
            v = f;
            f *= ratio;
        }
        return e;
    }();
    return edges;
}

AudioAnalyzer::AudioAnalyzer(AnalyzerConfig config)
    : config_{config},
      fft_{config.fftSize},
      beat_{static_cast<double>(config.hop) / (config.sampleRate > 0.0 ? config.sampleRate : 48000.0),
            config.minBeatInterval} {
    if (config_.sampleRate <= 0.0) {
        config_.sampleRate = 48000.0;
    }
    config_.fftSize = fft_.size();
    config_.hop = std::clamp<std::size_t>(config_.hop, 1, config_.fftSize);

    const std::size_t n = config_.fftSize;
    window_.resize(n);
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(i) / static_cast<double>(n));
        window_[i] = static_cast<float>(w);
        sum += w;
    }
    windowGain_ = sum / 2.0;
    ring_.assign(n, 0.0F);
    recentRing_.assign(kRecentSamples, 0.0F);
    scratch_.resize(n);

    // Welche FFT-Bins gehören zu welchem Band? Schmale Bänder (tiefe Töne) bekommen mindestens den
    // nächstliegenden Bin, damit sie nicht leer bleiben.
    const double binHz = config_.sampleRate / static_cast<double>(n);
    const std::size_t maxBin = n / 2 - 1;
    const auto& edges = bandEdgesHz();
    for (std::size_t b = 0; b < kSpectrumBands; ++b) {
        const auto lo = static_cast<std::size_t>(std::ceil(edges[b] / binHz));
        const auto hi = static_cast<std::size_t>(std::floor(edges[b + 1] / binHz));
        if (lo <= hi) {
            firstBin_[b] = std::min(lo, maxBin);
            lastBin_[b] = std::min(hi, maxBin);
        } else {
            const auto center = static_cast<std::size_t>(std::lround(std::sqrt(edges[b] * edges[b + 1]) / binHz));
            firstBin_[b] = lastBin_[b] = std::min(center, maxBin);
        }
    }
    reset();
}

void AudioAnalyzer::reset() {
    std::fill(ring_.begin(), ring_.end(), 0.0F);
    std::fill(recentRing_.begin(), recentRing_.end(), 0.0F);
    recentPos_ = 0;
    writePos_ = 0;
    filled_ = 0;
    sinceStep_ = 0;
    totalSamples_ = 0;
    hopPeak_ = 0.0F;
    beat_.reset();
    frame_ = {};
}

std::size_t AudioAnalyzer::push(std::span<const float> mono) {
    std::size_t steps = 0;
    for (const float sample : mono) {
        ring_[writePos_] = sample;
        writePos_ = (writePos_ + 1) % ring_.size();
        recentRing_[recentPos_] = sample;
        recentPos_ = (recentPos_ + 1) % recentRing_.size();
        filled_ = std::min(filled_ + 1, ring_.size());
        ++totalSamples_;
        hopPeak_ = std::max(hopPeak_, std::fabs(sample));
        if (++sinceStep_ >= config_.hop) {
            sinceStep_ = 0;
            if (filled_ == ring_.size()) {
                step();
                ++steps;
            }
            hopPeak_ = 0.0F;
        }
    }
    return steps;
}

void AudioAnalyzer::step() {
    const std::size_t n = ring_.size();
    double energy = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        const float s = ring_[(writePos_ + i) % n];  // ältestes zuerst
        energy += static_cast<double>(s) * s;
        scratch_[i] = {s * window_[i], 0.0F};
    }
    fft_.forward(scratch_);

    Spectrum spectrum;
    for (std::size_t b = 0; b < kSpectrumBands; ++b) {
        float strongest = 0.0F;
        for (std::size_t k = firstBin_[b]; k <= lastBin_[b]; ++k) {
            strongest = std::max(strongest, std::abs(scratch_[k]));
        }
        const double amplitude = static_cast<double>(strongest) / windowGain_;
        const double db = amplitude > 1e-9 ? 20.0 * std::log10(amplitude) : kFloorDb;
        spectrum.bands[b] = static_cast<float>(std::clamp((db - kFloorDb) / -kFloorDb, 0.0, 1.0));
    }

    frame_.rms = static_cast<float>(std::min(1.0, std::sqrt(energy / static_cast<double>(n))));
    frame_.peak = std::min(1.0F, hopPeak_);
    frame_.spectrum = spectrum;
    frame_.sampleRate = config_.sampleRate;
    frame_.recent.resize(recentRing_.size());
    for (std::size_t i = 0; i < recentRing_.size(); ++i) {
        frame_.recent[i] = recentRing_[(recentPos_ + i) % recentRing_.size()];  // älteste zuerst
    }
    frame_.audioTime = static_cast<double>(totalSamples_) / config_.sampleRate;
    ++frame_.frameCount;
    beat_.push(spectrum, frame_.audioTime);
    frame_.beat = beat_.info();
}

}  // namespace whitehole::audio
