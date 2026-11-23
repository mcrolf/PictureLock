cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(PictureLock VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PICTURELOCK_BUILD_TESTS "Build the unit and integration tests" ON)
set(PICTURELOCK_FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.deps/ffmpeg-install"
    CACHE PATH "Prefix of the static FFmpeg build (see scripts/build-ffmpeg.sh)")

add_subdirectory(third_party/JUCE)
include(cmake/FFmpeg.cmake)

juce_add_plugin(PictureLock
    PRODUCT_NAME "PictureLock"
    COMPANY_NAME "PictureLock"
    BUNDLE_ID "org.picturelock.plugin"
    PLUGIN_MANUFACTURER_CODE Pclk
    PLUGIN_CODE Pvid
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Tools
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Sources shared by the plugin and the test app. Each target compiles them with its
# own copy of the JUCE modules, as JUCE expects.
set(PICTURELOCK_SOURCES
    src/timecode/FrameRate.cpp
    src/timecode/Timecode.cpp
    src/sync/PicturePlacement.cpp
    src/sync/TransportClock.cpp
    src/video/VideoDecoder.cpp
    src/video/FrameCache.cpp
    src/video/PictureEngine.cpp
    src/audio/GuideAudioExporter.cpp
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp
    src/plugin/PictureSettings.cpp
    src/ui/PictureView.cpp
    src/ui/VideoWindow.cpp
    src/ui/SettingsPanel.cpp
    src/ui/LookAndFeel.cpp)
list(TRANSFORM PICTURELOCK_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

function(picturelock_configure_target target)
    target_sources(${target} PRIVATE ${PICTURELOCK_SOURCES})
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src")
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(${target} PRIVATE
        PictureLock::FFmpeg
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endfunction()

picturelock_configure_target(PictureLock)
target_link_libraries(PictureLock PRIVATE juce::juce_recommended_lto_flags)

if(PICTURELOCK_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
