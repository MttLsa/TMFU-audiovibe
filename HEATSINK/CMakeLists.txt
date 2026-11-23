cmake_minimum_required(VERSION 3.22)
# macOS: universal binary (Intel x86_64 + Apple Silicon arm64). Must be set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")

project(HEATSINK VERSION 1.0.0 LANGUAGES C CXX)
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

set(HS_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND HS_FORMATS AU)
endif()

# VST2 only when the (non-redistributable) VST2 SDK is provided: -DVST2_SDK_PATH=/path/to/vstsdk2.4
set(VST2_SDK_PATH "" CACHE PATH "Path to the VST2 SDK (optional)")
if(VST2_SDK_PATH)
  juce_set_vst2_sdk_path("${VST2_SDK_PATH}")
  list(APPEND HS_FORMATS VST)
endif()

juce_add_plugin(HEATSINK
  COMPANY_NAME "TMFU audiovibe"
  COMPANY_WEBSITE ""
  BUNDLE_ID "com.tmfuaudiovibe.heatsink"
  PLUGIN_MANUFACTURER_CODE Tmfu
  PLUGIN_CODE Htsk
  FORMATS ${HS_FORMATS}
  PRODUCT_NAME "HEATSINK"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Distortion Filter EQ
  VST2_CATEGORY kPlugCategEffect
  AU_MAIN_TYPE kAudioUnitType_Effect
  DESCRIPTION "Analog distortion, filter and EQ processor")

target_sources(HEATSINK PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(HEATSINK PRIVATE Source)

juce_add_binary_data(HeatsinkData SOURCES
  resources/fonts/Urbanist-Light.ttf resources/fonts/Urbanist-SemiBold.ttf)
set_target_properties(HeatsinkData PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_definitions(HEATSINK PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(HEATSINK
  PRIVATE juce::juce_audio_utils juce::juce_dsp HeatsinkData
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
