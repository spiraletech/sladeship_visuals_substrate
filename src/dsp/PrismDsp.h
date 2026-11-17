#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sladeship::prism {

constexpr std::size_t kFftSize = 4096;
constexpr std::size_t kSpectrumBands = 256;
constexpr std::size_t kAnalysisBands = 64;
constexpr std::size_t kRingSize = 32768;

struct SomaticState {
    float heavy = 0, glassy = 0, warm = 0, sharp = 0;
    float wide = 0, dense = 0, floaty = 0, turbulence = 0;
    float pressure = 0, haze = 0, metallic = 0, foggy = 0;
    float electric = 0, airy = 0;
};

struct SpectrumFrame {
    bool valid = false;
    std::uint32_t sampleRate = 0;
    float rms = 0, peak = 0, rmsDb = -120, peakDb = -120;
    float stereoCorrelation = 0, stereoWidth = 0, midRms = 0, sideRms = 0;
    float bass = 0, mid = 0, high = 0, beat = 0;
    float sub = 0, bassBody = 0, lowPunch = 0;
    float dominantHz = 0, centroidHz = 0, spectralFlux = 0;
    float harmonicity = 0, tonality = 0;
    float memoryPressure = 0, memoryTurbulence = 0;
    SomaticState somatic;
    std::array<float, kSpectrumBands> spectrumDb{};
    std::array<float, 12> chroma{};
};

// Portable, read-only signal analysis. Does not capture, process, or modify playback audio.
// One owning thread per instance; ingest interleaved PCM floats then call analyze().
class PrismDsp {
public:
    explicit PrismDsp(std::uint32_t sampleRate = 48000);
    void reset();
    void pushInterleaved(const float* samples, std::size_t frames, unsigned channels);
    const SpectrumFrame& analyze();
    const SpectrumFrame& frame() const noexcept { return output_; }
    std::size_t bufferedFrames() const noexcept { return buffered_; }

private:
    std::uint32_t rate_;
    std::array<float, kRingSize> left_{}, right_{};
    std::size_t write_ = 0, buffered_ = 0;
    std::array<float, kAnalysisBands> previousBands_{};
    std::array<float, 100> pressureHistory_{}, turbulenceHistory_{};
    std::size_t historyWrite_ = 0, historyCount_ = 0;
    float previousBass_ = 0, previousLowCore_ = 0, bassSustain_ = 0;
    SpectrumFrame output_{};
};

} // namespace sladeship::prism
