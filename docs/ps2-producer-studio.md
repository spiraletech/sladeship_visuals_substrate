# SLADESHIP Producer Studio — Experimental M1/M2
Branch: feature/ps2-producer-dsp-studio (based on integration/prism-lab-dsp-core).
Status: implemented prototype awaiting Windows runtime/visual approval; no canon promotion.

## Implemented
- Read-only whole-song WAV timeline using PRISM (4,096-point FFT, 1,024-frame default hop).
- Bounded stream decoding of PCM16/24/32 and IEEE float32 RIFF/WAVE; original single-snapshot API preserved.
- Timestamped spectrum, bass, beat, Mid/Side, chroma, RMS and heuristic somatic data; deterministic interpolation and clamped seeks.
- Separate Windows SLADESHIP_STUDIO.exe: Win32, Direct3D11 shaders, Media Foundation playback, asynchronous file analysis, audio-clock-driven visuals.
- Original prototype scenes: Orbital Studio (bass/beat/stereo), Frequency Cathedral (stereo/bass/beat), Liquid Memory (flux/bass/pressure).
- Existing eight-layer/onion launcher is not changed. Other EtherTech repositories remain untouched.

## Build (Windows SDK, MSVC)
    cmake -S . -B build -DBUILD_TESTING=ON
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure

Windows executable: build/Release/SLADESHIP_STUDIO.exe.
Open a WAV with O, press Space for pause/resume, Left/Right to seek, and 1/2/3 to change scenes.
Optional command-line argument: an absolute WAV path.

## Limitations and risk
- WAV only; no MP3/FLAC or video export, native timeline editor, sound synthesis, stem separation or MIDI.
- Whole-file analysis occurs before playback; worker prevents UI stalls, but cancel/reimport mid-analysis is not implemented.
- Silence decays per PRISM feature logic; 4,096-frame analysis window produces frequency-dependent response delay.
- Multi-channel source analysis currently uses first two channels; does not implement speaker-layout downmix.
- 60-FPS target on GTX 1650-class GPU is unmeasured. Software WARP fallback is present.
- Windows runtime playback, media event readiness, device loss, long-song memory, sync and manual visual approval remain unverified.
- Somatic descriptors are heuristic creative mappings, not calibrated psychoacoustic measurements.
- Feature frames track original PCM, and visual DSP does not change audio samples.

## Verification gates
- Existing onion-layer regression test must continue passing.
- Timeline test covers streaming WAV decoding, full-song timeline, silence/music, deterministic seeks, stereo antiphase, short WAV and invalid hop.
- On Windows, a shader test compiles both HLSL stages. It does not replace actual GPU runtime tests.
- Do not merge or promote visual canon until CI and Windows gameplay-style recording/review succeed.

Engineering handoff: https://github.com/spiraletech/sladeship_visuals_substrate/issues/2
