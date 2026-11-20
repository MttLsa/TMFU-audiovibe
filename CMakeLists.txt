cmake_minimum_required(VERSION 3.22)
# macOS: universal binary (Intel x86_64 + Apple Silicon arm64). Must be set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")

project(DUALBOX VERSION 2.16.0 LANGUAGES C CXX)

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
set(DUALBOX_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND DUALBOX_FORMATS AU)
endif()

juce_add_plugin(Dualbox
  COMPANY_NAME "TMFU audiovibe"                       # manufacturer shown in the DAW plug-in browser (this and future plug-ins)
  COMPANY_WEBSITE ""
  BUNDLE_ID "com.tmfuaudiovibe.dualbox"
  PLUGIN_MANUFACTURER_CODE Twnb                       # kept from Twinbox: saved DAW projects keep finding the plug-in
  PLUGIN_CODE Twbx                                    # kept from Twinbox (same reason)
  FORMATS ${DUALBOX_FORMATS}
  PRODUCT_NAME "DUALBOX"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Drum
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  DESCRIPTION "Dual drum groovebox: synth drums (A) + sample groovebox (B)")

target_sources(Dualbox PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/Params.cpp
  Source/dsp/SampleLibrary.cpp
  Source/dsp/KeyAnalysis.cpp
  Source/dsp/BeltStretch.cpp
  Source/dsp/MicroEngine.cpp
  Source/dsp/LimeEngine.cpp
  Source/dsp/LimeFX.cpp
  Source/dsp/ModMatrix.cpp
  Source/Presets.cpp
  Source/PatternImport.cpp
  Source/gui/LookAndFeel.cpp
  Source/gui/PresetBrowser.cpp
  Source/gui/MicroSection.cpp
  Source/gui/LimeSection.cpp
  Source/gui/LfoPanel.cpp
  Source/gui/GlobalPanel.cpp)

target_include_directories(Dualbox PRIVATE Source)

# Embedded resources: factory sample archive + genre preset libraries
juce_add_binary_data(DualboxData SOURCES
  resources/factory_samples.zip
  resources/presets_A.json
  resources/presets_B.json
  resources/fonts/Inter-Regular.ttf
  resources/fonts/Inter-SemiBold.ttf)
set_target_properties(DualboxData PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(APPLE)
  set(DUALBOX_ACCEL SIGNALSMITH_USE_ACCELERATE ACCELERATE_NEW_LAPACK)   # Signalsmith Stretch FFT on Accelerate
endif()
target_compile_definitions(Dualbox PUBLIC ${DUALBOX_ACCEL}
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Dualbox
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    DualboxData
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(APPLE)
  target_link_libraries(Dualbox PRIVATE "-framework Accelerate")   # FFT of Signalsmith Stretch (Belt pitch shift)
endif()

# ---------------------------------------------------------------- offline test harness (optional)
option(DUALBOX_BUILD_TESTS "Build the offline render / GUI snapshot test" OFF)
if(DUALBOX_BUILD_TESTS)
  juce_add_console_app(DualboxTest PRODUCT_NAME "DualboxTest")
  target_sources(DualboxTest PRIVATE tests/TestMain.cpp
    Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Params.cpp
    Source/dsp/SampleLibrary.cpp Source/dsp/KeyAnalysis.cpp Source/dsp/BeltStretch.cpp Source/dsp/MicroEngine.cpp Source/dsp/LimeEngine.cpp Source/dsp/LimeFX.cpp Source/dsp/ModMatrix.cpp
    Source/Presets.cpp Source/PatternImport.cpp Source/gui/PresetBrowser.cpp Source/gui/LookAndFeel.cpp Source/gui/MicroSection.cpp Source/gui/LimeSection.cpp Source/gui/LfoPanel.cpp Source/gui/GlobalPanel.cpp)
  target_include_directories(DualboxTest PRIVATE Source)
  target_compile_definitions(DualboxTest PRIVATE ${DUALBOX_ACCEL} JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="DUALBOX"
    JUCE_STANDALONE_APPLICATION=0 JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(DualboxTest PRIVATE juce::juce_audio_utils juce::juce_dsp DualboxData juce::juce_recommended_config_flags)
  if(APPLE)
    target_link_libraries(DualboxTest PRIVATE "-framework Accelerate")
  endif()
endif()
