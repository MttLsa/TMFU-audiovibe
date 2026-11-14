cmake_minimum_required(VERSION 3.22)
project(TWINBOX VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

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
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Drum
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  DESCRIPTION "Dual drum groovebox: synth drums (A) + sample groovebox (B)")

target_sources(Twinbox PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/Params.cpp
  Source/dsp/SampleBank.cpp
  Source/dsp/MicroEngine.cpp
  Source/dsp/LimeEngine.cpp
  Source/gui/LookAndFeel.cpp
  Source/gui/MicroSection.cpp
  Source/gui/LimeSection.cpp
  Source/gui/GlobalPanel.cpp)

target_include_directories(Twinbox PRIVATE Source)

target_compile_definitions(Twinbox PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Twinbox
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
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
    Source/dsp/SampleBank.cpp Source/dsp/MicroEngine.cpp Source/dsp/LimeEngine.cpp
    Source/gui/LookAndFeel.cpp Source/gui/MicroSection.cpp Source/gui/LimeSection.cpp Source/gui/GlobalPanel.cpp)
  target_include_directories(TwinboxTest PRIVATE Source)
  target_compile_definitions(TwinboxTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Twinbox"
    JUCE_STANDALONE_APPLICATION=0 JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(TwinboxTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
