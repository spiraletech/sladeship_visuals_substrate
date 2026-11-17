#include "dsp/PrismWav.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace sladeship::prism {
namespace {
std::uint16_t u16(const unsigned char* b) {
    return static_cast<std::uint16_t>(b[0] | (static_cast<std::uint16_t>(b[1]) << 8));
}
std::uint32_t u32(const unsigned char* b) {
    return static_cast<std::uint32_t>(b[0]) | (static_cast<std::uint32_t>(b[1]) << 8) |
           (static_cast<std::uint32_t>(b[2]) << 16) | (static_cast<std::uint32_t>(b[3]) << 24);
}
void readExact(std::istream& in, char* dest, std::streamsize n) {
    if (!in.read(dest, n)) throw std::runtime_error("PRISM WAV: truncated file");
}
float unpack(const unsigned char* bytes, std::uint16_t format, std::uint16_t bits) {
    if (format == 3 && bits == 32) {
        const std::uint32_t val = u32(bytes);
        float result = 0;
        std::memcpy(&result, &val, sizeof result);
        return std::isfinite(result) ? result : 0.0f;
    }
    if (format == 1 && bits == 16) return static_cast<float>(static_cast<std::int16_t>(u16(bytes))) / 32768.0f;
    if (format == 1 && bits == 24) {
        std::int32_t value = static_cast<std::int32_t>(bytes[0]) |
            (static_cast<std::int32_t>(bytes[1]) << 8) | (static_cast<std::int32_t>(bytes[2]) << 16);
        if (value & 0x800000) value |= ~0xFFFFFF;
        return static_cast<float>(value) / 8388608.0f;
    }
    if (format == 1 && bits == 32) {
        const std::uint32_t raw = u32(bytes);
        std::int64_t signedValue = raw;
        if (raw & 0x80000000U) signedValue -= (1LL << 32);
        return static_cast<float>(signedValue / 2147483648.0);
    }
    throw std::runtime_error("PRISM WAV: unsupported encoding; use PCM 16/24/32 or float32 WAV");
}
} // namespace

SpectrumFrame analyzeWaveFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("PRISM WAV: cannot open file: " + path);
    std::array<unsigned char, 12> riff{};
    readExact(in, reinterpret_cast<char*>(riff.data()), static_cast<std::streamsize>(riff.size()));
    if (std::memcmp(riff.data(), "RIFF", 4) || std::memcmp(riff.data() + 8, "WAVE", 4))
        throw std::runtime_error("PRISM WAV: expected RIFF/WAVE file");

    bool haveFormat = false, haveData = false;
    std::uint16_t format = 0, channels = 0, bits = 0, alignment = 0;
    std::uint32_t sampleRate = 0, dataBytes = 0;
    std::streampos dataPos{};
    for (;;) {
        std::array<unsigned char, 8> chunk{};
        if (!in.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(chunk.size()))) break;
        const auto length = u32(chunk.data() + 4);
        const auto start = in.tellg();
        if (std::memcmp(chunk.data(), "fmt ", 4) == 0) {
            if (length < 16 || length > 4096) throw std::runtime_error("PRISM WAV: invalid fmt chunk");
            std::vector<unsigned char> fmt(length);
            readExact(in, reinterpret_cast<char*>(fmt.data()), static_cast<std::streamsize>(length));
            format = u16(fmt.data()); channels = u16(fmt.data()+2);
            sampleRate = u32(fmt.data()+4); alignment = u16(fmt.data()+12); bits = u16(fmt.data()+14);
            // WAVE_FORMAT_EXTENSIBLE: PCM/float subformat GUID starts at byte 24.
            if (format == 0xFFFE && length >= 40) format = u16(fmt.data()+24);
            haveFormat = true;
        } else if (std::memcmp(chunk.data(), "data", 4) == 0 && !haveData) {
            dataPos = start;
            dataBytes = length;
            haveData = true;
        }
        // Skip unrecognized chunks, including metadata and LIST; honor odd-byte padding.
        in.clear(); in.seekg(start + static_cast<std::streamoff>(length) + static_cast<std::streamoff>(length & 1U));
        if (!in) throw std::runtime_error("PRISM WAV: invalid chunk size");
    }
    if (!haveFormat || !haveData || channels == 0 || channels > 32 || sampleRate < 8000 || sampleRate > 192000 ||
        (format != 1 && format != 3) || (bits != 16 && bits != 24 && bits != 32) ||
        (format == 3 && bits != 32) || alignment != channels * (bits / 8))
        throw std::runtime_error("PRISM WAV: unsupported or malformed WAV format");
    if (dataBytes < alignment * kFftSize || dataBytes % alignment)
        throw std::runtime_error("PRISM WAV: audio shorter than 4096 frames or invalid block alignment");
    in.clear(); in.seekg(dataPos);
    if (!in) throw std::runtime_error("PRISM WAV: missing data payload");
    PrismDsp dsp(sampleRate);
    constexpr std::size_t blockFrames = kFftSize;
    std::vector<unsigned char> raw(blockFrames * alignment);
    std::vector<float> pcm(blockFrames * channels);
    std::uint32_t remaining = dataBytes;
    while (remaining > 0) {
        const auto frames = std::min<std::size_t>(blockFrames, remaining / alignment);
        const auto bytes = frames * alignment;
        readExact(in, reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(bytes));
        for (std::size_t i = 0; i < frames; ++i) {
            for (unsigned c = 0; c < channels; ++c) {
                pcm[i * channels + c] = unpack(raw.data() + i * alignment + c * (bits / 8), format, bits);
            }
        }
        dsp.pushInterleaved(pcm.data(), frames, channels);
        if (dsp.bufferedFrames() >= kFftSize) dsp.analyze();
        remaining -= static_cast<std::uint32_t>(bytes);
    }
    return dsp.frame();
}
} // namespace sladeship::prism
