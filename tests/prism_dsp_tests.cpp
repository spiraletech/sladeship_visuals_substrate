#include "dsp/PrismDsp.h"
#include "dsp/PrismWav.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace sladeship::prism;
namespace {
constexpr float pi = 3.14159265358979323846f;
void require(bool yes, const char* msg) { if (!yes) throw std::runtime_error(msg); }
std::vector<float> signal(float hz, float amplitude, float sr, int polarity = 1) {
    std::vector<float> samples(kFftSize * 2);
    for (std::size_t i=0; i < kFftSize; ++i) {
        const float v=amplitude * std::sin(2.0f*pi*hz*static_cast<float>(i)/sr);
        samples[i*2]=v; samples[i*2+1]=polarity*v;
    }
    return samples;
}
void write16(std::ofstream& o, std::uint16_t x) {
    const char b[]={static_cast<char>(x&255),static_cast<char>(x>>8)};o.write(b,2);
}
void write32(std::ofstream& o, std::uint32_t x) {
    const char b[]={static_cast<char>(x&255),static_cast<char>((x>>8)&255),static_cast<char>((x>>16)&255),static_cast<char>(x>>24)};o.write(b,4);
}
}
int main(){
    try {
        PrismDsp dsp(48000);
        require(!dsp.analyze().valid,"unprimed frame is not ready");
        const auto tone = signal(440, .5f, 48000);
        dsp.pushInterleaved(tone.data(), kFftSize, 2);
        const auto& frame = dsp.analyze();
        require(frame.valid,"FFT must be ready");
        require(std::abs(frame.dominantHz-440) < 15,"440 Hz dominant frequency");
        require(frame.rms > .30f && frame.rms < .40f,"440 Hz RMS");
        require(frame.stereoCorrelation > .99f,"in-phase correlation");
        require(frame.sideRms < .001f,"mono mid/side separation");
        require(frame.spectrumDb[100] <= 0,"spectrum dB upper bound");
        for (float v : frame.spectrumDb) require(std::isfinite(v) && v >= -120.001f, "spectrum is finite and bounded");
        const auto& soma=frame.somatic;
        const float parameters[]={soma.heavy,soma.glassy,soma.warm,soma.sharp,soma.wide,soma.dense,soma.floaty,soma.turbulence,soma.pressure,soma.haze,soma.metallic,soma.foggy,soma.electric,soma.airy};
        for(float v:parameters) require(v>=0 && v<=1 && std::isfinite(v),"somatic range");
        const auto outOfPhase = signal(440,.5f,48000,-1);
        dsp.reset();dsp.pushInterleaved(outOfPhase.data(),kFftSize,2);
        require(dsp.analyze().stereoCorrelation < -.99f,"out-of-phase correlation");
        require(dsp.frame().midRms < .001f && dsp.frame().sideRms>.3f,"out-of-phase mid/side");
        dsp.reset();std::vector<float> silence(kFftSize * 2);
        dsp.pushInterleaved(silence.data(),kFftSize,2);
        const auto& quiet=dsp.analyze();
        require(quiet.valid && quiet.rms == 0 && quiet.dominantHz == 0 && quiet.rmsDb==-120,"silence response");
        dsp.reset();dsp.pushInterleaved(tone.data(),kFftSize,2);
        require(dsp.analyze().valid,"reset re-prime");
        const auto temp=std::filesystem::temp_directory_path()/"sladeship_prism_dsp_test.wav";
        { std::ofstream w(temp,std::ios::binary);
            w.write("RIFF",4);write32(w,36+static_cast<std::uint32_t>(kFftSize*4));w.write("WAVEfmt ",8);
            write32(w,16);write16(w,1);write16(w,2);write32(w,48000);write32(w,192000);write16(w,4);write16(w,16);
            w.write("data",4);write32(w,static_cast<std::uint32_t>(kFftSize*4));
            for(float sample:tone){const auto n=static_cast<std::int16_t>(sample*32767);write16(w,static_cast<std::uint16_t>(n));}
        }
        const auto imported=analyzeWaveFile(temp.string());
        std::filesystem::remove(temp);
        require(imported.valid && std::abs(imported.dominantHz-440)<15,"WAV PCM16 import + analysis");
        std::cout<<"PASS: PRISM DSP FFT, RMS, correlation, somatic, silence, reset, PCM16 WAV import\n";
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1;}
}
