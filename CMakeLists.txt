cmake_minimum_required(VERSION 3.22)
# macOS: universal binary (Intel x86_64 + Apple Silicon arm64). Must be set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")

project(TWINBOX VERSION 2.11.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: uses ./JUCE if present (or -DJUCE_DIR=/path/to/JUCE), otherwise downloads JUCE 8.0.4
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to JUCE")
if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
  add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

# AU is only buildable on macOS; VST3 everywhere; Standalone for quick testing.
set(TWINBOX_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND TWINBOX_FORMATS AU)
endif()

juce_add_plugin(Twinbox
  COMPANY_NAME "Twinbox Audio"
  COMPANY_WEBSITE ""
  BUNDLE_ID "com.twinbox.twinbox"
  PLUGIN_MANUFACTURER_CODE Twnb
  PLUGIN_CODE Twbx
  FORMATS ${TWINBOX_FORMATS}
  PRODUCT_NAME "Twinbox"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Drum
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  DESCRIPTION "Dual drum groovebox: synth drums (A) + sample groovebox (B)")

target_sources(Twinbox PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/Params.cpp
  Source/dsp/SampleLibrary.cpp
  Source/dsp/MicroEngine.cpp
  Source/dsp/LimeEngine.cpp
  Source/dsp/LimeFX.cpp
  Source/dsp/ModMatrix.cpp
  Source/Presets.cpp
  Source/gui/LookAndFeel.cpp
  Source/gui/PresetBrowser.cpp
  Source/gui/MicroSection.cpp
  Source/gui/LimeSection.cpp
  Source/gui/LfoPanel.cpp
  Source/gui/GlobalPanel.cpp)

target_include_directories(Twinbox PRIVATE Source)

# Embedded resources: factory sample archive + genre preset libraries
juce_add_binary_data(TwinboxData SOURCES
  resources/factory_samples.zip
  resources/presets_A.json
  resources/presets_B.json)
set_target_properties(TwinboxData PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(Twinbox PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Twinbox
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    TwinboxData
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- offline test harness (optional)
option(TWINBOX_BUILD_TESTS "Build the offline render / GUI snapshot test" OFF)
if(TWINBOX_BUILD_TESTS)
  juce_add_console_app(TwinboxTest PRODUCT_NAME "TwinboxTest")
  target_sources(TwinboxTest PRIVATE tests/TestMain.cpp
    Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Params.cpp
    Source/dsp/SampleLibrary.cpp Source/dsp/MicroEngine.cpp Source/dsp/LimeEngine.cpp Source/dsp/LimeFX.cpp Source/dsp/ModMatrix.cpp
    Source/Presets.cpp Source/gui/PresetBrowser.cpp Source/gui/LookAndFeel.cpp Source/gui/MicroSection.cpp Source/gui/LimeSection.cpp Source/gui/LfoPanel.cpp Source/gui/GlobalPanel.cpp)
  target_include_directories(TwinboxTest PRIVATE Source)
  target_compile_definitions(TwinboxTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Twinbox"
    JUCE_STANDALONE_APPLICATION=0 JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(TwinboxTest PRIVATE juce::juce_audio_utils juce::juce_dsp TwinboxData juce::juce_recommended_config_flags)
endif()
