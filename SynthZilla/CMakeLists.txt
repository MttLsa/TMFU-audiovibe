cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")

project(SYNTHZILLA VERSION 2.0.2 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: ./JUCE, ../JUCE, -DJUCE_DIR=..., otherwise downloads JUCE 8.0.4
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

set(SZ_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND SZ_FORMATS AU)
endif()

juce_add_plugin(SynthZilla
  COMPANY_NAME "TMFU audiovibe"
  COMPANY_WEBSITE ""
  BUNDLE_ID "com.tmfuaudiovibe.synthzilla"
  PLUGIN_MANUFACTURER_CODE Tmfu
  PLUGIN_CODE Szla
  FORMATS ${SZ_FORMATS}
  PRODUCT_NAME "SynthZilla"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Synth
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  DESCRIPTION "Up to 5 analog / digital synth modules with famous sounds 1960s-2020s")

set(TS_SOURCES
  Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Params.cpp Source/Presets.cpp
  Source/dsp/SynthEngine.cpp Source/dsp/Wavetables.cpp)
target_sources(SynthZilla PRIVATE ${TS_SOURCES})
target_include_directories(SynthZilla PRIVATE Source)

juce_add_binary_data(SynthZillaData SOURCES resources/presets.json resources/fonts/Urbanist-Light.ttf resources/fonts/Urbanist-Bold.ttf resources/fonts/Rostex-Outline.ttf resources/logo/kaiju_keyboard.png)
set_target_properties(SynthZillaData PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(TS_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_compile_definitions(SynthZilla PUBLIC ${TS_DEFS})
target_link_libraries(SynthZilla
  PRIVATE juce::juce_audio_utils juce::juce_dsp SynthZillaData
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# offline render / preset calibration / GUI snapshot test
option(SYNTHZILLA_BUILD_TESTS "Build the offline test harness" OFF)
if(SYNTHZILLA_BUILD_TESTS)
  juce_add_console_app(SynthZillaTest PRODUCT_NAME "SynthZillaTest")
  target_sources(SynthZillaTest PRIVATE tests/TestMain.cpp ${TS_SOURCES})
  target_include_directories(SynthZillaTest PRIVATE Source)
  target_compile_definitions(SynthZillaTest PRIVATE ${TS_DEFS} JucePlugin_Name="SynthZilla" JucePlugin_VersionString="2.0.2" JUCE_STANDALONE_APPLICATION=0 JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(SynthZillaTest PRIVATE juce::juce_audio_utils juce::juce_dsp SynthZillaData juce::juce_recommended_config_flags)
endif()
