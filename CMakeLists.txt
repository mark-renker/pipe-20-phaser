# Pipe-20 Phaser — independent behavioural model inspired by the Tau "The Pipe" phaser / Jürgen Haible redesign.
# Targets: pipe20_tests (unit tests), pipe20_render (WAV->WAV), Pipe20Phaser (AU / VST3 / CLAP / standalone).
cmake_minimum_required(VERSION 3.22)
project(Pipe20Phaser VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(PIPE20_ARCHS "arm64;x86_64" CACHE STRING "macOS architectures (use arm64 for a fast dev build)")
  set(CMAKE_OSX_ARCHITECTURES "${PIPE20_ARCHS}" CACHE STRING "" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(PIPE20_BUILD_PLUGINS "Build the JUCE plug-in targets" ON)
option(PIPE20_BUILD_CLAP "Build the CLAP format via clap-juce-extensions" ON)

# ---- JUCE-free core: tests + renderer ---------------------------------------
add_executable(pipe20_tests tests/UnitTests.cpp)
add_executable(pipe20_render tools/render.cpp)
foreach(t pipe20_tests pipe20_render)
  if(NOT MSVC)
    target_compile_options(${t} PRIVATE -Wall -Wextra -O2)
  endif()
endforeach()
enable_testing()
add_test(NAME unit COMMAND pipe20_tests)

if(NOT PIPE20_BUILD_PLUGINS)
  return()
endif()

# ---- JUCE: local checkout when present, otherwise fetch 8.0.6 --------------
include(FetchContent)
if(DEFINED PIPE20_JUCE_PATH AND EXISTS "${PIPE20_JUCE_PATH}/CMakeLists.txt")
  add_subdirectory("${PIPE20_JUCE_PATH}" JUCE)
elseif(EXISTS "$ENV{HOME}/JUCE/CMakeLists.txt")
  message(STATUS "Using local JUCE at $ENV{HOME}/JUCE")
  add_subdirectory("$ENV{HOME}/JUCE" JUCE)
else()
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

set(PIPE20_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND PIPE20_FORMATS AU)
endif()

if(PIPE20_BUILD_CLAP)
  FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG main          # tagged releases predate JUCE 8
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(clap-juce-extensions)
endif()

juce_add_plugin(Pipe20Phaser
  PRODUCT_NAME "Pipe-20 Phaser"
  COMPANY_NAME "Renky Denk"
  BUNDLE_ID com.renkydenk.pipe20phaser
  PLUGIN_MANUFACTURER_CODE Rnky
  PLUGIN_CODE P20p
  FORMATS ${PIPE20_FORMATS}
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(Pipe20Phaser PRIVATE src/plugin/PluginProcessor.cpp src/plugin/PluginEditor.cpp)
target_compile_definitions(Pipe20Phaser PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Pipe20Phaser
  PRIVATE juce::juce_audio_utils
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(PIPE20_BUILD_CLAP)
  clap_juce_extensions_plugin(TARGET Pipe20Phaser
    CLAP_ID "com.renkydenk.pipe20phaser"
    CLAP_FEATURES audio-effect phaser modulation)
endif()
