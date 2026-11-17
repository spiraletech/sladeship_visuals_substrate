#pragma once
#include "dsp/PrismDsp.h"
#include <string>

namespace sladeship::prism {
// First import adapter: RIFF/WAVE PCM 16/24/32-bit or IEEE float32.
// This is offline analysis only; it never touches the OS playback device.
SpectrumFrame analyzeWaveFile(const std::string& path);
}
