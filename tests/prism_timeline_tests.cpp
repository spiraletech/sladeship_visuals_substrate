#include "dsp/PrismTimeline.h"
#include "dsp/PrismWav.h"
#include <algorithm>
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
void check(bool b, const char* why) { if (!b) throw std::runtime_error(why); }
void u16(std::ofstream& o, std::uint16_t v) { const char a[2]={char(v),char(v>>8)};o.write(a,2); }
void u32(std::ofstream& o, std::uint32_t v) { const char a[4]={char(v),char(v>>8),char(v>>16),char(v>>24)};o.write(a,4); }
void wave(const std::filesystem::path& p, std::size_t frames, bool phaseFlip=false, bool shortClip=false) {
    std::ofstream out(p, std::ios::binary);
    if (!out) throw std::runtime_error("cannot create fixture");
    out.write("RIFF",4);u32(out,static_cast<std::uint32_t>(36+frames*4));out.write("WAVEfmt ",8);
    u32(out,16);u16(out,1);u16(out,2);u32(out,48000);u32(out,48000*4);u16(out,4);u16(out,16);
    out.write("data",4);u32(out,static_cast<std::uint32_t>(frames*4));
    for(std::size_t i=0;i<frames;++i) {
        const float val=(phaseFlip || shortClip || i>=frames/2)
            ? .55f*std::sin(2.f*pi*440.f*static_cast<float>(i)/48000.f) : 0.f;
        const auto s=static_cast<std::int16_t>(val*32767.f);
        u16(out,static_cast<std::uint16_t>(s));
        u16(out,static_cast<std::uint16_t>(phaseFlip?-s:s));
    }
}
}
int main() {
    const auto p=std::filesystem::temp_directory_path()/"sladeship_feature_timeline_fixture.wav";
    try {
        wave(p,48000);
        std::size_t samples=0, callbacks=0;
        auto info=streamWaveFile(p.string(),[&](const float* s,std::size_t n,unsigned channels,std::uint32_t rate){
            check(s!=nullptr && channels==2 && rate==48000,"stream metadata");
            samples+=n;++callbacks;
        });
        check(info.frames==48000 && info.channels==2 && info.sampleRate==48000,"wave info");
        check(samples==48000 && callbacks>1,"complete streaming decode");
        const auto track=PrismTimeline::fromWaveFile(p.string(),1024);
        check(track.frames().size()>30 && track.frames().size()<60,"full-song windows");
        check(std::abs(track.durationSeconds()-1.0)<1e-9,"correct duration");
        check(track.frames().front().signal.valid && track.frames().back().signal.valid,"valid spectra");
        check(track.sampleAt(0).rms<0.001f,"silent introduction");
        check(track.sampleAt(0.8).rms>0.25f,"second-half music measured");
        check(track.sampleAt(-4).rms==track.sampleAt(0).rms,"negative seek clamps");
        check(track.sampleAt(999).rms==track.frames().back().signal.rms,"end seek clamps");
        const auto a=track.sampleAt(.55),b=track.sampleAt(.55);
        check(a.rms==b.rms && a.spectralFlux==b.spectralFlux,"repeatable seek interpolation");
        for(std::size_t i=1;i<track.frames().size();++i)
            check(track.frames()[i].seconds>track.frames()[i-1].seconds,"timestamps strictly increasing");
        bool badHop=false;try{PrismTimeline::fromWaveFile(p.string(),0);}catch(const std::invalid_argument&){badHop=true;}
        check(badHop,"invalid hop rejected");
        wave(p,8192,true);
        const auto antiphase=PrismTimeline::fromWaveFile(p.string());
        check(antiphase.sampleAt(0.06).stereoCorrelation<-.99f,"stereo anti-phase retained");
        wave(p,512,false,true);
        const auto tiny=PrismTimeline::fromWaveFile(p.string());
        check(tiny.frames().size()==1 && tiny.sampleAt(0).valid,"short WAV analysis padding");
        check(std::abs(tiny.durationSeconds()-512.0/48000.0)<1e-9,"padding does not change duration");
        std::filesystem::remove(p);
        std::cout<<"PASS: streamed WAV, timed features, silence/music, seeks, stereo, short source and hop bounds\n";
        return 0;
    }catch(const std::exception& ex){
        std::filesystem::remove(p);
        std::cerr<<"FAIL: "<<ex.what()<<"\n";return 1;
    }
}
