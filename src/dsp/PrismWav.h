#pragma once
#include "dsp/PrismDsp.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace sladeship::prism {
// First import adapter: RIFF/WAVE PCM 16/24/32-bit or IEEE float32.
// This is offline analysis only; it never touches the OS playback device.
struct WaveInfo {
    std::uint32_t sampleRate = 0;
    unsigned channels = 0;
    std::uint64_t frames = 0;
};
using PcmConsumer = std::function<void(const float* interleaved, std::size_t frames, unsigned channels, std::uint32_t sampleRate)>;
// Decodes WAV in bounded streaming blocks. No audio device involved; preserves stereo float PCM.
WaveInfo streamWaveFile(const std::string& path, const PcmConsumer& consume);
SpectrumFrame analyzeWaveFile(const std::string& path);
}
