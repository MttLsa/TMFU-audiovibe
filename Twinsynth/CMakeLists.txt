cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")

project(TWINSYNTH VERSION 1.0.0 LANGUAGES C CXX)
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

set(TS_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND TS_FORMATS AU)
endif()

juce_add_plugin(Twinsynth
  COMPANY_NAME "Twinbox Audio"
  BUNDLE_ID "com.twinbox.twinsynth"
  PLUGIN_MANUFACTURER_CODE Twnb
  PLUGIN_CODE Twsy
  FORMATS ${TS_FORMATS}
  PRODUCT_NAME "Twinsynth"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Synth
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  DESCRIPTION "Analog (A) + Digital (D) synth with famous sounds 1960s-2020s")

set(TS_SOURCES
  Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Params.cpp Source/Presets.cpp
  Source/dsp/SynthEngine.cpp Source/dsp/Wavetables.cpp)
target_sources(Twinsynth PRIVATE ${TS_SOURCES})
target_include_directories(Twinsynth PRIVATE Source)

juce_add_binary_data(TwinsynthData SOURCES resources/presets.json)
set_target_properties(TwinsynthData PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(TS_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_compile_definitions(Twinsynth PUBLIC ${TS_DEFS})
target_link_libraries(Twinsynth
  PRIVATE juce::juce_audio_utils juce::juce_dsp TwinsynthData
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# offline render / preset calibration / GUI snapshot test
option(TWINSYNTH_BUILD_TESTS "Build the offline test harness" OFF)
if(TWINSYNTH_BUILD_TESTS)
  juce_add_console_app(TwinsynthTest PRODUCT_NAME "TwinsynthTest")
  target_sources(TwinsynthTest PRIVATE tests/TestMain.cpp ${TS_SOURCES})
  target_include_directories(TwinsynthTest PRIVATE Source)
  target_compile_definitions(TwinsynthTest PRIVATE ${TS_DEFS} JucePlugin_Name="Twinsynth" JUCE_STANDALONE_APPLICATION=0 JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(TwinsynthTest PRIVATE juce::juce_audio_utils juce::juce_dsp TwinsynthData juce::juce_recommended_config_flags)
endif()
