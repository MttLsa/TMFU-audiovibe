cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")

project(FAMPAT VERSION 1.0.0 LANGUAGES C CXX)
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

juce_add_plugin(FamPat
  COMPANY_NAME "TMFU audiovibe"
  COMPANY_WEBSITE ""
  BUNDLE_ID "com.tmfuaudiovibe.fampat"
  PLUGIN_MANUFACTURER_CODE Tmfu
  PLUGIN_CODE Fmpt
  FORMATS ${SZ_FORMATS}
  PRODUCT_NAME "FamPat"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Synth
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  DESCRIPTION "Famous patches: 200 famous synth sounds 1963-2023, a preset player")

set(TS_SOURCES
  Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/PlayerEditor.cpp Source/Params.cpp Source/Presets.cpp
  Source/dsp/SynthEngine.cpp Source/dsp/Wavetables.cpp)
target_sources(FamPat PRIVATE ${TS_SOURCES})
target_include_directories(FamPat PRIVATE Source)

juce_add_binary_data(FamPatData SOURCES resources/presets.json resources/fonts/Inter-Regular.ttf resources/fonts/Inter-SemiBold.ttf resources/fonts/Rostex-Regular.ttf)
set_target_properties(FamPatData PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(TS_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_compile_definitions(FamPat PUBLIC ${TS_DEFS})
target_link_libraries(FamPat
  PRIVATE juce::juce_audio_utils juce::juce_dsp FamPatData
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# offline render / preset calibration / GUI snapshot test
option(FAMPAT_BUILD_TESTS "Build the offline test harness" OFF)
if(FAMPAT_BUILD_TESTS)
  juce_add_console_app(FamPatTest PRODUCT_NAME "FamPatTest")
  target_sources(FamPatTest PRIVATE tests/TestMain.cpp ${TS_SOURCES})
  target_include_directories(FamPatTest PRIVATE Source)
  target_compile_definitions(FamPatTest PRIVATE ${TS_DEFS} JucePlugin_Name="FamPat" JucePlugin_VersionString="1.0.0" JUCE_STANDALONE_APPLICATION=0 JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(FamPatTest PRIVATE juce::juce_audio_utils juce::juce_dsp FamPatData juce::juce_recommended_config_flags)
endif()
