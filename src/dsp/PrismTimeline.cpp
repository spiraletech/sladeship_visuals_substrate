#include "dsp/PrismTimeline.h"
#include "dsp/PrismWav.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

namespace sladeship::prism {
namespace {
float interpolate(float a, float b, float t) { return a + (b - a) * t; }
SpectrumFrame mix(const SpectrumFrame& a, const SpectrumFrame& b, float t) {
    SpectrumFrame out = a;
#define MIX(name) out.name = interpolate(a.name, b.name, t)
    MIX(rms); MIX(peak); MIX(rmsDb); MIX(peakDb);
    MIX(stereoCorrelation); MIX(stereoWidth); MIX(midRms); MIX(sideRms);
    MIX(bass); MIX(mid); MIX(high); MIX(beat);
    MIX(sub); MIX(bassBody); MIX(lowPunch);
    MIX(dominantHz); MIX(centroidHz); MIX(spectralFlux);
    MIX(harmonicity); MIX(tonality); MIX(memoryPressure); MIX(memoryTurbulence);
#undef MIX
    for (std::size_t i=0; i<out.spectrumDb.size(); ++i)
        out.spectrumDb[i] = interpolate(a.spectrumDb[i], b.spectrumDb[i], t);
    for (std::size_t i=0; i<out.chroma.size(); ++i)
        out.chroma[i] = interpolate(a.chroma[i], b.chroma[i], t);
#define SOM(name) out.somatic.name = interpolate(a.somatic.name, b.somatic.name, t)
    SOM(heavy); SOM(glassy); SOM(warm); SOM(sharp);
    SOM(wide); SOM(dense); SOM(floaty); SOM(turbulence);
    SOM(pressure); SOM(haze); SOM(metallic); SOM(foggy);
    SOM(electric); SOM(airy);
#undef SOM
    return out;
}
} // namespace

PrismTimeline PrismTimeline::fromWaveFile(const std::string& path, std::size_t hopFrames) {
    if (hopFrames < 64 || hopFrames > kFftSize)
        throw std::invalid_argument("PRISM timeline: hop must be 64..4096 frames");
    PrismTimeline timeline;
    timeline.hop_ = hopFrames;
    std::unique_ptr<PrismDsp> dsp;
    std::uint64_t ingested = 0;
    std::uint64_t nextFrame = kFftSize;
    unsigned inputChannels = 0;
    auto info = streamWaveFile(path, [&](const float* pcm, std::size_t frames, unsigned channels, std::uint32_t rate) {
        if (!dsp) { dsp = std::make_unique<PrismDsp>(rate); inputChannels = channels; }
        std::size_t pos = 0;
        while (pos < frames) {
            const auto need = static_cast<std::size_t>(nextFrame - ingested);
            const auto n = std::min(frames - pos, need);
            dsp->pushInterleaved(pcm + pos * channels, n, channels);
            ingested += n;
            pos += n;
            if (ingested == nextFrame) {
                const auto& result = dsp->analyze();
                const double center = static_cast<double>(ingested - kFftSize / 2) / rate;
                timeline.frames_.push_back({center, result});
                nextFrame += hopFrames;
            }
        }
    });
    timeline.rate_ = info.sampleRate;
    timeline.totalFrames_ = info.frames;

    if (!dsp) throw std::runtime_error("PRISM timeline: empty audio");
    if (ingested < kFftSize) {
        // Short files receive analysis-only zero padding, never changes to playback audio.
        const auto missing = static_cast<std::size_t>(kFftSize - ingested);
        std::vector<float> silence(missing * inputChannels, 0.0f);
        dsp->pushInterleaved(silence.data(), missing, inputChannels);
        timeline.frames_.push_back({timeline.durationSeconds() * 0.5, dsp->analyze()});
    } else if (ingested + hopFrames > nextFrame && (ingested != nextFrame - hopFrames)) {
        // Include the actual tail; do not extrapolate by analyzing fabricated silence.
        const double center = static_cast<double>(ingested - kFftSize / 2) / info.sampleRate;
        timeline.frames_.push_back({center, dsp->analyze()});
    }
    if (timeline.frames_.empty()) throw std::runtime_error("PRISM timeline: no analysis frames");
    return timeline;
}

SpectrumFrame PrismTimeline::sampleAt(double seconds) const {
    if (frames_.empty()) return {};
    if (!std::isfinite(seconds) || seconds <= frames_.front().seconds) return frames_.front().signal;
    if (seconds >= frames_.back().seconds) return frames_.back().signal;
    const auto high = std::lower_bound(frames_.begin(), frames_.end(), seconds,
        [](const TimedFrame& f, double t) { return f.seconds < t; });
    if (high == frames_.begin()) return high->signal;
    const auto& left = *(high - 1);
    const double span = high->seconds - left.seconds;
    const float blend = static_cast<float>(std::clamp((seconds - left.seconds) / span, 0.0, 1.0));
    return mix(left.signal, high->signal, blend);
}

} // namespace sladeship::prism
