#pragma once
#include "dsp/PrismDsp.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sladeship::prism {

struct TimedFrame {
    double seconds = 0.0; // center of the source window, relative to file start
    SpectrumFrame signal{};
};

// Read-only, seekable analysis over the *whole* file; no playback/DSP mutation.
// Frames are recorded chronologically using a configurable hop (1024 default).
class PrismTimeline {
public:
    static PrismTimeline fromWaveFile(const std::string& path, std::size_t hopFrames = 1024);
    SpectrumFrame sampleAt(double seconds) const; // deterministic, bounded interpolation
    const std::vector<TimedFrame>& frames() const noexcept { return frames_; }
    double durationSeconds() const noexcept { return rate_ ? static_cast<double>(totalFrames_) / rate_ : 0.0; }
    std::uint32_t sampleRate() const noexcept { return rate_; }
    std::uint64_t totalFrames() const noexcept { return totalFrames_; }
    std::size_t hopFrames() const noexcept { return hop_; }

private:
    std::vector<TimedFrame> frames_;
    std::uint32_t rate_ = 0;
    std::uint64_t totalFrames_ = 0;
    std::size_t hop_ = 1024;
};

} // namespace sladeship::prism
