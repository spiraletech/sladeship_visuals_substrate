# PRISM LAB DSP -> SLADESHIP (integration branch)

## Intent and provenance
PRISM LAB source: https://github.com/spiraletech/prismanalysis
Reconstructed source reference: PRISM_LAB_ORBMODES.exe / prism.cpp (GitHub Actions build, 2026-10-04).
This is a scoped port of PRISM LAB **analysis** logic, not a wholesale application merge.
The existing eight-layer SLADESHIP launcher and visual design remain unchanged.

## Available portable C++17 backend
- 4096-sample radix-2 Hann-windowed FFT, 256 log-spaced visualization bands.
- RMS/peak dBFS, Mid/Side power, stereo correlation/width.
- Dominant spectral frequency, centroid, chroma, 64 coarse bands, spectral flux and harmonicity.
- PRISM-derived low-frequency sub/body/punch envelopes and 14 SomaticState values.
- Bounded rolling pressure/turbulence history.
- Native Win32/WASAPI/GDI are explicitly **not** part of the reusable core.
- Stateless read-only analysis of PCM input: audio samples are not processed for playback or modified.

## Initial imported-audio adapter
\`PrismWav.h\` / \`PrismWav.cpp\` can scan RIFF/WAVE with PCM16/24/32 or IEEE-float32
(also basic WAVE_FORMAT_EXTENSIBLE subformats). It decodes frames in chunks and
returns the **last** analyzed snapshot. This is a diagnostic bridge, not yet
a timeline, full-song aggregate, visualizer, or user-facing importer UI.
MP3/FLAC/AAC, RF64 and sample-accurate time synchronization are deferred.

Example:
\`\`\`sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/sladeship --analyze-wav path/to/song.wav
\`\`\`
On MSVC use \`build/Release/sladeship.exe\`. The existing \`--output preview\`,
\`--list-layers\` and \`--verify\` commands are unchanged.

## Gates before merging to main
1. Linux and Windows CMake/CTest succeed, including existing onion-layer regression.
2. Benchmark CPU/FFT cost and verify no GUI frame stalls in sustained imported playback.
3. Define audio-feature clocking and seek/reset behavior for a timeline.
4. Distinguish heuristic somatic parameters from calibrated psychoacoustic measurements.
5. Wire renderer/studio features only after explicit design approval.

Status: implementation branch, **not** final production or visual-canon promotion.
