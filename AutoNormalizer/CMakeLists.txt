cmake_minimum_required(VERSION 3.22)
# macOS: universal binary (Intel + Apple Silicon). Must be set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")

project(AutoNormalizer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------- JUCE
# Uses ./JUCE (or ../JUCE, or -DJUCE_DIR=...) if present, otherwise downloads JUCE 8.0.4.
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to JUCE")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE/CMakeLists.txt")
  set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE")
endif()
if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
  add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------- formats
# VST3 everywhere, AU on macOS. VST2 needs the legacy Steinberg VST2 SDK, which can no
# longer be downloaded/redistributed: pass -DVST2_SDK_DIR=/path/to/vstsdk2.4 if you own it.
set(AN_FORMATS VST3)
if(APPLE)
  list(APPEND AN_FORMATS AU)
endif()
set(VST2_SDK_DIR "" CACHE PATH "Path to the legacy VST2 SDK (folder containing pluginterfaces/vst2.x)")
if(VST2_SDK_DIR)
  juce_set_vst2_sdk_path("${VST2_SDK_DIR}")
  list(APPEND AN_FORMATS VST)
  message(STATUS "AutoNormalizer: VST2 enabled (${VST2_SDK_DIR})")
else()
  message(STATUS "AutoNormalizer: VST2 disabled (set -DVST2_SDK_DIR=... to enable)")
endif()
option(AN_STANDALONE "Also build a Standalone app (for quick testing)" OFF)
if(AN_STANDALONE)
  list(APPEND AN_FORMATS Standalone)
endif()

juce_add_plugin(AutoNormalizer
  COMPANY_NAME "AutoNormalizer"
  BUNDLE_ID "com.autonormalizer.autonormalizer"
  PLUGIN_MANUFACTURER_CODE Atnm
  PLUGIN_CODE AtNz
  FORMATS ${AN_FORMATS}
  PRODUCT_NAME "AutoNormalizer"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE          # to receive MIDI Program Change
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Tools
  AU_MAIN_TYPE kAudioUnitType_MusicEffect   # 'aumf': audio effect that also receives MIDI
  VST2_CATEGORY kPlugCategEffect
  DESCRIPTION "Normalizes the previous plugin to 0 dB once per preset change, then holds the gain")

target_sources(AutoNormalizer PRIVATE
  Source/NormalizerEngine.cpp
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)
target_include_directories(AutoNormalizer PRIVATE Source)

target_compile_definitions(AutoNormalizer PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AutoNormalizer
  PRIVATE
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- engine test (no JUCE needed)
option(AN_BUILD_TESTS "Build the offline engine test" ON)
if(AN_BUILD_TESTS)
  add_executable(AutoNormalizerEngineTest tests/EngineTest.cpp Source/NormalizerEngine.cpp)
  enable_testing()
  add_test(NAME engine COMMAND AutoNormalizerEngineTest)
endif()

# ---------------------------------------------------------------- plugin smoke test + GUI snapshot (optional)
option(AN_BUILD_PLUGIN_TEST "Build the host-side plugin smoke test" OFF)
if(AN_BUILD_PLUGIN_TEST)
  juce_add_console_app(AutoNormalizerPluginTest PRODUCT_NAME "AutoNormalizerPluginTest")
  target_sources(AutoNormalizerPluginTest PRIVATE tests/PluginTest.cpp
    Source/NormalizerEngine.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
  target_include_directories(AutoNormalizerPluginTest PRIVATE Source)
  target_compile_definitions(AutoNormalizerPluginTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JucePlugin_Name="AutoNormalizer" JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(AutoNormalizerPluginTest PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
