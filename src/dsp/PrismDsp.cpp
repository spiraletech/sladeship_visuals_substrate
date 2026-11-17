#include "dsp/PrismDsp.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <stdexcept>

namespace sladeship::prism {
namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kMinDb = -120.0f;
float bound(float x) { return std::clamp(x, 0.0f, 1.0f); }
float toward(float a, float b, float rate) { return a + (b - a) * rate; }
float decibels(float amplitude) { return std::max(kMinDb, 20.0f * std::log10(std::max(amplitude, 1e-6f))); }
float safeSample(float x) { return std::isfinite(x) ? x : 0.0f; }

// 4,096-point in-place radix-2 Cooley-Tukey FFT adapted from PRISM LAB.
void fft(std::array<std::complex<float>, kFftSize>& bins) {
    for (std::size_t i = 1, j = 0; i < kFftSize; ++i) {
        std::size_t bit = kFftSize >> 1;
        while (j & bit) { j ^= bit; bit >>= 1; }
        j ^= bit;
        if (i < j) std::swap(bins[i], bins[j]);
    }
    for (std::size_t length = 2; length <= kFftSize; length <<= 1) {
        const float theta = -2.0f * kPi / static_cast<float>(length);
        const auto base = std::complex<float>(std::cos(theta), std::sin(theta));
        for (std::size_t offset = 0; offset < kFftSize; offset += length) {
            auto twiddle = std::complex<float>(1.0f, 0.0f);
            for (std::size_t j = 0; j < length / 2; ++j) {
                auto even = bins[offset + j];
                auto odd = bins[offset + j + length / 2] * twiddle;
                bins[offset + j] = even + odd;
                bins[offset + j + length / 2] = even - odd;
                twiddle *= base;
            }
        }
    }
}

float lowBandAmplitude(const std::array<std::complex<float>, kFftSize>& bins,
                       float fromHz, float toHz, float sampleRate) {
    const auto lo = std::clamp(static_cast<int>(fromHz * kFftSize / sampleRate), 1, static_cast<int>(kFftSize / 2 - 1));
    const auto hi = std::clamp(static_cast<int>(std::ceil(toHz * kFftSize / sampleRate)), lo, static_cast<int>(kFftSize / 2 - 1));
    float power = 0;
    for (int k = lo; k <= hi; ++k) power += std::norm(bins[static_cast<std::size_t>(k)]);
    // PRISM Hann-windowed low-band amplitude and perceptual compression.
    const float amplitude = 4.0f * std::sqrt(power) / static_cast<float>(kFftSize);
    return bound(amplitude / (0.095f + amplitude));
}

void updateSomatic(SpectrumFrame& o) {
    // PRISM LAB deterministic psychoacoustic feature mapping (not an AI model).
    auto& s = o.somatic;
    if (o.rms < 1e-5f) {
        s.heavy *= .86f; s.glassy *= .86f; s.warm *= .86f; s.sharp *= .86f;
        s.wide *= .86f; s.dense *= .86f; s.floaty *= .86f; s.turbulence *= .86f;
        s.pressure *= .86f; s.haze *= .86f; s.metallic *= .86f; s.foggy *= .86f;
        s.electric *= .86f; s.airy *= .86f;
        return;
    }
    const float cent = bound((o.centroidHz - 180.0f) / 4200.0f);
    const float loud = bound(std::min(1.0f, o.rms * 8.0f) * 1.15f);
    const float stable = 1.0f - bound(o.spectralFlux * 1.6f);
    s.pressure = toward(s.pressure, bound(0.58f * loud + 0.42f * o.bass), 0.14f);
    s.heavy = toward(s.heavy, bound(0.58f * o.bass + 0.24f * s.pressure + 0.18f * (1 - cent)), 0.13f);
    s.sharp = toward(s.sharp, bound(0.52f * o.high + 0.34f * o.spectralFlux + 0.14f * cent), 0.16f);
    s.glassy = toward(s.glassy, bound(0.38f * cent + 0.34f * o.tonality + 0.20f * o.high + 0.08f * stable), 0.13f);
    s.warm = toward(s.warm, bound(0.42f * o.bass + 0.38f * o.mid + 0.20f * (1 - cent)), 0.12f);
    s.wide = toward(s.wide, o.stereoWidth, 0.12f);
    const float active = bound((o.bass + o.mid + o.high) / 3.0f);
    s.dense = toward(s.dense, bound(0.60f * active + 0.40f * loud), 0.13f);
    s.turbulence = toward(s.turbulence, bound(0.42f * o.spectralFlux + 0.30f * o.beat + 0.28f * (1 - o.harmonicity)), 0.16f);
    s.haze = toward(s.haze, bound(0.46f * (1 - o.tonality) + 0.30f * s.dense + 0.24f * stable), 0.10f);
    s.floaty = toward(s.floaty, bound(0.30f * s.wide + 0.28f * (1 - s.heavy) + 0.24f * stable + 0.18f * o.high), 0.10f);
    s.metallic = toward(s.metallic, bound(0.46f * s.glassy + 0.34f * s.sharp + 0.20f * o.tonality), 0.12f);
    s.foggy = toward(s.foggy, bound(0.62f * s.haze + 0.22f * s.dense + 0.16f * (1 - s.sharp)), 0.11f);
    s.electric = toward(s.electric, bound(0.44f * s.sharp + 0.34f * o.spectralFlux + 0.22f * o.high), 0.15f);
    s.airy = toward(s.airy, bound(0.50f * s.floaty + 0.30f * o.high + 0.20f * (1 - s.dense)), 0.10f);
}
} // namespace

PrismDsp::PrismDsp(std::uint32_t sampleRate) : rate_(sampleRate) {
    if (sampleRate < 8000 || sampleRate > 192000) throw std::invalid_argument("PRISM DSP: sample rate must be 8000..192000 Hz");
    reset();
}

void PrismDsp::reset() {
    left_.fill(0); right_.fill(0); previousBands_.fill(0);
    pressureHistory_.fill(0); turbulenceHistory_.fill(0);
    write_ = buffered_ = historyWrite_ = historyCount_ = 0;
    previousBass_ = previousLowCore_ = bassSustain_ = 0;
    output_ = SpectrumFrame{};
    output_.sampleRate = rate_;
    output_.spectrumDb.fill(kMinDb);
}

void PrismDsp::pushInterleaved(const float* samples, std::size_t frames, unsigned channels) {
    if (channels == 0 || channels > 32) throw std::invalid_argument("PRISM DSP: invalid channel count");
    if (!samples && frames) throw std::invalid_argument("PRISM DSP: null sample input");
    for (std::size_t i = 0; i < frames; ++i) {
        const float l = safeSample(samples[i * channels]);
        const float r = channels > 1 ? safeSample(samples[i * channels + 1]) : l;
        const auto ix = write_ & (kRingSize - 1);
        left_[ix] = l; right_[ix] = r;
        ++write_;
        if (buffered_ < kRingSize) ++buffered_;
    }
}

const SpectrumFrame& PrismDsp::analyze() {
    output_.valid = buffered_ >= kFftSize;
    if (!output_.valid) return output_;
    std::array<std::complex<float>, kFftSize> spectrum{};
    float sumSq = 0, sumL = 0, sumR = 0, sumLR = 0, sumMid = 0, sumSide = 0;
    float peak = 0, lrDiff = 0, lrSum = 0;
    for (std::size_t i = 0; i < kFftSize; ++i) {
        const auto pos = (write_ - kFftSize + i) & (kRingSize - 1);
        const float l = left_[pos], r = right_[pos], m = (l + r) * 0.5f, side = (l - r) * 0.5f;
        sumL += l * l; sumR += r * r; sumLR += l * r;
        sumMid += m * m; sumSide += side * side;
        sumSq += m * m;
        lrDiff += std::abs(l - r); lrSum += std::abs(l) + std::abs(r);
        peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
        const float window = 0.5f - 0.5f * std::cos(2.0f * kPi * static_cast<float>(i) / static_cast<float>(kFftSize - 1));
        spectrum[i] = {m * window, 0.0f};
    }
    fft(spectrum);
    auto& o = output_;
    const float invN = 1.0f / static_cast<float>(kFftSize);
    o.rms = std::sqrt(sumSq * invN);
    o.peak = peak;
    o.rmsDb = decibels(o.rms);
    o.peakDb = decibels(o.peak);
    o.midRms = std::sqrt(sumMid * invN);
    o.sideRms = std::sqrt(sumSide * invN);
    o.stereoCorrelation = sumL * sumR > 1e-12f ? std::clamp(sumLR / std::sqrt(sumL * sumR), -1.0f, 1.0f) : 0.0f;
    o.stereoWidth = toward(o.stereoWidth, bound(lrDiff / (lrSum + 1e-5f) * 1.75f), 0.12f);
    // Bounded spectral field: 0-20 Hz, then 255 logarithmically distributed bands to 20 kHz.
    std::array<float, kSpectrumBands> visualAmplitudes{};
    const float nyquist = static_cast<float>(rate_) * 0.5f;
    for (std::size_t k = 1; k < kFftSize / 2; ++k) {
        const float hz = static_cast<float>(k) * static_cast<float>(rate_) * invN;
        if (hz > 20000.0f) break;
        std::size_t band = 0;
        if (hz > 20.0f) {
            const float t = std::log(hz / 20.0f) / std::log(20000.0f / 20.0f);
            band = 1 + std::min<std::size_t>(254, static_cast<std::size_t>(t * 255.0f));
        }
        const float amp = 4.0f * std::abs(spectrum[k]) * invN;
        visualAmplitudes[band] = std::max(visualAmplitudes[band], amp);
    }
    for (std::size_t b = 0; b < kSpectrumBands; ++b) o.spectrumDb[b] = decibels(visualAmplitudes[b]);
    o.sub = toward(o.sub, lowBandAmplitude(spectrum, 20, 60, rate_), 0.34f);
    o.bassBody = toward(o.bassBody, lowBandAmplitude(spectrum, 60, 120, rate_), 0.36f);
    o.lowPunch = toward(o.lowPunch, lowBandAmplitude(spectrum, 120, 250, rate_), 0.42f);
    const float lowCore = bound(o.sub * .50f + o.bassBody * .36f + o.lowPunch * .14f);
    const float kick = bound(std::max(0.0f, lowCore - previousLowCore_) * 5.8f + std::max(0.0f, o.lowPunch - bassSustain_) * 1.7f);
    o.beat = std::max(o.beat * 0.88f, kick);
    previousLowCore_ = toward(previousLowCore_, lowCore, 0.22f);
    bassSustain_ = toward(bassSustain_, lowCore, .16f);
    const auto hiBin = std::min<std::size_t>(kFftSize / 2 - 2, static_cast<std::size_t>(6000.0f * kFftSize / static_cast<float>(rate_)));
    float total = 0, weighted = 0, strongest = 0;
    std::size_t strongestIndex = 0;
    for (std::size_t k = 2; k <= hiBin; ++k) {
        const float m = std::abs(spectrum[k]);
        total += m; weighted += m * static_cast<float>(k) * rate_ * invN;
        if (m > strongest) { strongest = m; strongestIndex = k; }
    }
    o.centroidHz = total > 1e-5f ? weighted / total : 0.0f;
    o.dominantHz = strongest > 1e-4f ? static_cast<float>(strongestIndex) * rate_ * invN : 0.0f;
    o.tonality = total > 1e-5f ? bound((strongest / (total / std::max<std::size_t>(1, hiBin - 1)) - 1.0f) / 28.0f) : 0.0f;
    float harmonic = 0;
    if (o.dominantHz > 35.0f) {
        for (unsigned h = 1; h <= 8; ++h) {
            const float hz = o.dominantHz * static_cast<float>(h);
            if (hz > 6000 || hz > nyquist) break;
            const auto bin = static_cast<std::size_t>(std::round(hz * kFftSize / static_cast<float>(rate_)));
            if (bin < kFftSize / 2) harmonic += std::abs(spectrum[bin]) / h;
        }
    }
    o.harmonicity = strongest > 0.0f ? bound(harmonic / (strongest * 1.9f + 1e-5f)) : 0.0f;
    std::array<float, kAnalysisBands> currentBands{};
    float flux = 0;
    for (std::size_t b = 0; b < kAnalysisBands; ++b) {
        const std::size_t start = 1 + b * b * (kFftSize / 2 - 2) / (kAnalysisBands * kAnalysisBands);
        const std::size_t stop = std::max(start + 1, 1 + (b+1) * (b+1) * (kFftSize / 2 - 2) / (kAnalysisBands * kAnalysisBands));
        float mag = 0;
        for (std::size_t k = start; k < stop; ++k) mag += std::abs(spectrum[k]);
        const float scaled = (mag / (stop - start)) * 0.014f;
        currentBands[b] = bound((scaled / (1.0f + scaled)) * 1.6f);
        flux += std::max(0.0f, currentBands[b] - previousBands_[b]);
        previousBands_[b] = currentBands[b];
    }
    o.spectralFlux = toward(o.spectralFlux, bound(flux * .72f), .24f);
    float low = 0, mid = 0, high = 0;
    for (std::size_t i = 0; i < 10; ++i) low += currentBands[i];
    for (std::size_t i = 10; i < 38; ++i) mid += currentBands[i];
    for (std::size_t i = 38; i < 64; ++i) high += currentBands[i];
    o.bass = low / 10; o.mid = mid / 28; o.high = high / 26;
    o.beat = std::max(o.beat, bound(std::max(0.0f, o.bass - previousBass_ - .025f) * 9.0f));
    previousBass_ = toward(previousBass_, o.bass, .08f);
    std::array<float, 12> chromaEnergy{};
    for (int note = 0; note < 88; ++note) {
        const float noteHz = 27.5f * std::pow(2.0f, static_cast<float>(note) / 12.0f);
        const auto k = static_cast<std::size_t>(std::round(noteHz * kFftSize / static_cast<float>(rate_)));
        if (k >= 2 && k <= hiBin) {
            auto& energy = chromaEnergy[static_cast<std::size_t>((note + 9) % 12)];
            energy += std::abs(spectrum[k]);
        }
    }
    for (std::size_t c = 0; c < 12; ++c)
        o.chroma[c] = toward(o.chroma[c], bound(chromaEnergy[c] / (strongest + 1e-5f)), .18f);
    updateSomatic(o);
    pressureHistory_[historyWrite_] = o.somatic.pressure;
    turbulenceHistory_[historyWrite_] = o.somatic.turbulence;
    historyWrite_ = (historyWrite_ + 1) % pressureHistory_.size();
    historyCount_ = std::min(historyCount_ + 1, pressureHistory_.size());
    float p = 0, t = 0;
    for (std::size_t i = 0; i < historyCount_; ++i) { p += pressureHistory_[i]; t += turbulenceHistory_[i]; }
    o.memoryPressure = toward(o.memoryPressure, p / historyCount_, .10f);
    o.memoryTurbulence = toward(o.memoryTurbulence, t / historyCount_, .10f);
    return o;
}

} // namespace sladeship::prism
