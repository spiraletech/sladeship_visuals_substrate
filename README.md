# SLADESHIP — EtherTech Visuals Substrate

C++17 concept prototype for SLADESHIP's simplified onion-layer title screen. This is an **experimental implementation**, not the approved final image or a native GPU visualizer.

## Build and verify

Run:

    cmake -S . -B build
    cmake --build build
    ctest --test-dir build --output-on-failure
    ./build/sladeship --output preview

Open \`preview/index.html\` in a browser. Windows multi-configuration builds can run \`build/Debug/sladeship.exe --output preview\`.

The C++ executable generates **five independently addressable vector SVG layers**, **two live text layers**, and a working accessible **ENTER STUDIO** button that opens a placeholder studio view. Canvas is 960×540, 16:9; implementation order L00 background → L01 reflection → L02 orb → L03 rings → L04 glint → L05 title → L06 subtitle → L07 button. The contract is in \`specs/sladeship_onion_layers_v0_1.json\`.

**Not implemented:** native GPU renderer, shader fidelity, audio input/FFT, orbital animation, scene persistence, studio editor, final export pipeline. The original user-provided painting is not silently baked into one background image. Visual canon remains **PROPOSED_NOT_APPROVED** pending review.
