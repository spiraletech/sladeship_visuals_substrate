cmake_minimum_required(VERSION 3.16)
project(sladeship_visuals_substrate VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(sladeship_prism_dsp
  src/dsp/PrismDsp.cpp
  src/dsp/PrismWav.cpp
  src/dsp/PrismTimeline.cpp
)
target_include_directories(sladeship_prism_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_executable(sladeship src/main.cpp)
target_link_libraries(sladeship PRIVATE sladeship_prism_dsp)
if(MSVC)
  target_compile_options(sladeship PRIVATE /W4)
else()
  target_compile_options(sladeship PRIVATE -Wall -Wextra -Wpedantic)
endif()
include(CTest)
if(BUILD_TESTING)
  add_test(NAME onion_layers COMMAND sladeship --verify)
  add_executable(prism_dsp_tests tests/prism_dsp_tests.cpp)
  target_link_libraries(prism_dsp_tests PRIVATE sladeship_prism_dsp)
  add_test(NAME prism_dsp COMMAND prism_dsp_tests)
  add_executable(prism_timeline_tests tests/prism_timeline_tests.cpp)
  target_link_libraries(prism_timeline_tests PRIVATE sladeship_prism_dsp)
  add_test(NAME prism_timeline COMMAND prism_timeline_tests)
endif()

# Windows-native producer studio experiment (separate from approved onion-layer launcher).
if(WIN32)
  add_executable(sladeship_studio WIN32 src/windows/StudioPreview.cpp)
  target_include_directories(sladeship_studio PRIVATE src)
  target_compile_definitions(sladeship_studio PRIVATE UNICODE _UNICODE NOMINMAX)
  target_link_libraries(sladeship_studio PRIVATE
    sladeship_prism_dsp d3d11 d3dcompiler dxgi mfplat mfplay mfreadwrite mfuuid
    ole32 comdlg32 shell32
  )
  set_target_properties(sladeship_studio PROPERTIES OUTPUT_NAME "SLADESHIP_STUDIO")
  if(MSVC)
    target_compile_options(sladeship_studio PRIVATE /W4 /utf-8)
  endif()
  if(BUILD_TESTING)
    add_executable(studio_shader_tests tests/studio_shader_tests.cpp)
    target_include_directories(studio_shader_tests PRIVATE src)
    target_link_libraries(studio_shader_tests PRIVATE d3dcompiler)
    add_test(NAME studio_shader COMMAND studio_shader_tests)
  endif()
endif()
