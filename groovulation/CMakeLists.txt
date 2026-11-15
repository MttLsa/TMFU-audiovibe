cmake_minimum_required(VERSION 3.22)
# macOS: universal binary (Intel x86_64 + Apple Silicon arm64). Must be set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")

project(GROOVULATION VERSION 1.1.7 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: ../JUCE or ./JUCE if present (or -DJUCE_DIR=/path/to/JUCE), otherwise downloads JUCE 8.0.4
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

set(GROOV_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND GROOV_FORMATS AU)
endif()

juce_add_plugin(Groovulation
  COMPANY_NAME "Twinbox Audio"
  COMPANY_WEBSITE ""
  BUNDLE_ID "com.twinbox.groovulation"
  PLUGIN_MANUFACTURER_CODE Twnb
  PLUGIN_CODE Grvl
  FORMATS ${GROOV_FORMATS}
  PRODUCT_NAME "Groovulation"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Sampler
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  DESCRIPTION "Loop & sample mixer: 9 loop modules, key-aware SCALE mode, remix tools")

set(GROOV_SOURCES
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/Params.cpp
  Source/dsp/Analysis.cpp
  Source/dsp/LoopLibrary.cpp
  Source/dsp/ModuleEngine.cpp
  Source/dsp/Fx.cpp
  Source/gui/LookAndFeel.cpp
  Source/gui/ModuleViews.cpp
  Source/gui/MasterPanel.cpp
  Source/gui/DeckWindows.cpp)

target_sources(Groovulation PRIVATE ${GROOV_SOURCES})
target_include_directories(Groovulation PRIVATE Source)

juce_add_binary_data(GroovulationData SOURCES resources/factory_loops.zip)
set_target_properties(GroovulationData PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(GROOV_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_USE_MP3AUDIOFORMAT=1 JUCE_USE_FLAC=1 JUCE_USE_OGGVORBIS=1)
if(APPLE)
  list(APPEND GROOV_DEFS SIGNALSMITH_USE_ACCELERATE ACCELERATE_NEW_LAPACK)
endif()
target_compile_definitions(Groovulation PUBLIC ${GROOV_DEFS})

set(GROOV_LIBS juce::juce_audio_utils juce::juce_dsp GroovulationData)
if(APPLE)
  list(APPEND GROOV_LIBS "-framework Accelerate")
endif()
target_link_libraries(Groovulation
  PRIVATE ${GROOV_LIBS}
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- offline test harness (optional)
option(GROOV_BUILD_TESTS "Build the offline render / analysis / GUI snapshot test" OFF)
if(GROOV_BUILD_TESTS)
  juce_add_console_app(GroovulationTest PRODUCT_NAME "GroovulationTest")
  target_sources(GroovulationTest PRIVATE tests/TestMain.cpp ${GROOV_SOURCES})
  target_include_directories(GroovulationTest PRIVATE Source)
  target_compile_definitions(GroovulationTest PRIVATE ${GROOV_DEFS} JucePlugin_Name="Groovulation" JucePlugin_VersionString="1.1.7"
    JUCE_STANDALONE_APPLICATION=0 JUCE_MODAL_LOOPS_PERMITTED=1)
  target_link_libraries(GroovulationTest PRIVATE ${GROOV_LIBS} juce::juce_recommended_config_flags)
endif()
