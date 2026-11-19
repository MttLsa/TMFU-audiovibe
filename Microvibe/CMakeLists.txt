cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")

project(MICROVIBE VERSION 1.1.0 LANGUAGES C CXX)
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

set(MV_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND MV_FORMATS AU)
endif()

juce_add_plugin(Microvibe
  COMPANY_NAME "TMFU audiovibe"
  COMPANY_WEBSITE ""
  BUNDLE_ID "com.tmfuaudiovibe.microvibe"
  PLUGIN_MANUFACTURER_CODE Tmfu
  PLUGIN_CODE Mcvb
  FORMATS ${MV_FORMATS}
  PRODUCT_NAME "Microvibe"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Modulation Delay Reverb
  AU_MAIN_TYPE kAudioUnitType_MusicEffect
  DESCRIPTION "Glitch, textures, organic percussion and FX for minimal and microhouse")

set(MV_SOURCES
  Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Params.cpp Source/Presets.cpp
  Source/dsp/Engine.cpp Source/dsp/Tuning.cpp third_party/MTS-ESP/libMTSClient.cpp)
target_sources(Microvibe PRIVATE ${MV_SOURCES})
set_source_files_properties(third_party/MTS-ESP/libMTSClient.cpp PROPERTIES COMPILE_OPTIONS "-w")
target_include_directories(Microvibe PRIVATE Source)

juce_add_binary_data(MicrovibeData SOURCES resources/presets.json
  resources/fonts/Urbanist-Light.ttf resources/fonts/Urbanist-SemiBold.ttf)
set_target_properties(MicrovibeData PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(MV_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_compile_definitions(Microvibe PUBLIC ${MV_DEFS})
target_link_libraries(Microvibe
  PRIVATE juce::juce_audio_utils juce::juce_dsp MicrovibeData ${CMAKE_DL_LIBS}
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# offline render / check / GUI snapshot harness
option(MICROVIBE_BUILD_TESTS "Build the offline test harness" OFF)
if(MICROVIBE_BUILD_TESTS)
  juce_add_console_app(MicrovibeTest PRODUCT_NAME "MicrovibeTest")
  target_sources(MicrovibeTest PRIVATE tests/TestMain.cpp ${MV_SOURCES})
  target_include_directories(MicrovibeTest PRIVATE Source)
  target_compile_definitions(MicrovibeTest PRIVATE ${MV_DEFS} JucePlugin_Name="Microvibe" JUCE_STANDALONE_APPLICATION=0 JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(MicrovibeTest PRIVATE juce::juce_audio_utils juce::juce_dsp MicrovibeData juce::juce_recommended_config_flags ${CMAKE_DL_LIBS})
endif()
