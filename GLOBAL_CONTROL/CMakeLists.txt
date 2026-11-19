cmake_minimum_required(VERSION 3.22)
# GLOBAL CONTROL - standalone macOS app + Max for Live bridge for Ableton Live.
# Current state (Milestones 1-6, proof of concept): the JUCE-free core library and its unit tests.
# The JUCE GUI application target is added in Milestone 7, after the PoC is validated in Live
# (see Documentation/MILESTONE_PLAN.md).
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")

project(GLOBAL_CONTROL VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(gc_core STATIC
  GlobalControlApp/Core/ParameterScale.cpp
  GlobalControlApp/MacroEngine/MacroEngine.cpp
  GlobalControlApp/MappingEngine/InstanceReconciler.cpp
  GlobalControlApp/OSC/ProtocolCore.cpp)
target_include_directories(gc_core PUBLIC GlobalControlApp)
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
