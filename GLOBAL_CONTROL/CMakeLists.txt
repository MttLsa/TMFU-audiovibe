cmake_minimum_required(VERSION 3.22)
# GLOBAL CONTROL - standalone macOS app + Max for Live bridge for Ableton Live.
#   gc_core                       JUCE-free core (scales, macro engine, reconciliation, protocol)
#   GlobalControl (app)           JUCE GUI application                       [GC_BUILD_APP=ON]
#   GlobalControlIntegrationTests app logic <-> real bridge over UDP (mock LOM)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")

project(GLOBAL_CONTROL VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(GC_BUILD_APP "Build the JUCE application and its integration tests" ON)

add_library(gc_core STATIC
  GlobalControlApp/Core/ParameterScale.cpp
  GlobalControlApp/MacroEngine/MacroEngine.cpp
  GlobalControlApp/MappingEngine/InstanceReconciler.cpp
  GlobalControlApp/OSC/ProtocolCore.cpp)
target_include_directories(gc_core PUBLIC GlobalControlApp)
set_target_properties(gc_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(gc_core PRIVATE /W4)
else()
  target_compile_options(gc_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_executable(gc_core_tests Tests/CoreTests/CoreTests.cpp)
target_link_libraries(gc_core_tests PRIVATE gc_core)
add_test(NAME core COMMAND gc_core_tests)

find_program(NODE_EXE node)
if(NODE_EXE)
  add_test(NAME bridge COMMAND ${NODE_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/Tests/OSCtests/test_bridge.js)
endif()

if(NOT GC_BUILD_APP)
  return()
endif()

# JUCE: ../JUCE or -DJUCE_DIR=/path/to/JUCE if present, otherwise JUCE 8.0.4 is downloaded
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Path to JUCE")
if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
  add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

set(GC_APP_LOGIC
  GlobalControlApp/Core/BridgeInstaller.cpp
  GlobalControlApp/Models/ProjectModel.cpp
  GlobalControlApp/OSC/BridgeConnection.cpp
  GlobalControlApp/PresetEngine/PresetStore.cpp
  GlobalControlApp/Core/AppController.cpp)

set(GC_DEFS
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_USE_DARK_SPLASH_SCREEN=1
  GC_VERSION=${PROJECT_VERSION} GC_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# --- application -------------------------------------------------------------------------------------------
juce_add_gui_app(GlobalControl
  PRODUCT_NAME "GLOBAL CONTROL"
  COMPANY_NAME "TMFU audiovibe"
  BUNDLE_ID "com.tmfuaudiovibe.globalcontrol"
  VERSION ${PROJECT_VERSION}
  ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/icon_1024.png"
  MICROPHONE_PERMISSION_ENABLED FALSE)
target_sources(GlobalControl PRIVATE
  ${GC_APP_LOGIC}
  GlobalControlApp/GUI/LookAndFeel.cpp
  GlobalControlApp/GUI/MacroKnob.cpp
  GlobalControlApp/GUI/Panels.cpp
  GlobalControlApp/GUI/MacroInspector.cpp
  GlobalControlApp/GUI/MainComponent.cpp
  GlobalControlApp/GUI/Main.cpp)
target_compile_definitions(GlobalControl PRIVATE ${GC_DEFS} JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:GlobalControl,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:GlobalControl,JUCE_VERSION>")
target_link_libraries(GlobalControl PRIVATE gc_core juce::juce_gui_extra juce::juce_osc
  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
# the bridge device ships inside the app (Contents/Resources/Bridge), built by tools/make_amxd.py
set(GC_BRIDGE_BUILD "${CMAKE_CURRENT_SOURCE_DIR}/AbletonBridge/Build")
if(APPLE)
  set(GC_BRIDGE_DEST "$<TARGET_BUNDLE_CONTENT_DIR:GlobalControl>/Resources/Bridge")
else()
  set(GC_BRIDGE_DEST "$<TARGET_FILE_DIR:GlobalControl>/Bridge")
endif()
add_custom_command(TARGET GlobalControl POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${GC_BRIDGE_DEST}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${GC_BRIDGE_BUILD}" "${GC_BRIDGE_DEST}"
  COMMENT "Bundling GLOBAL_CONTROL_BRIDGE.amxd")

# --- integration tests (app logic <-> gc_bridge.js over UDP) -------------------------------------------------
juce_add_console_app(GlobalControlIntegrationTests PRODUCT_NAME "GlobalControlIntegrationTests")
target_sources(GlobalControlIntegrationTests PRIVATE ${GC_APP_LOGIC} Tests/OSCtests/AppIntegrationTests.cpp)
target_compile_definitions(GlobalControlIntegrationTests PRIVATE ${GC_DEFS} JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(GlobalControlIntegrationTests PRIVATE gc_core juce::juce_events juce::juce_osc juce::juce_data_structures
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(NODE_EXE)
  add_test(NAME app_integration COMMAND GlobalControlIntegrationTests ${CMAKE_CURRENT_SOURCE_DIR}/Tests/OSCtests)
endif()
