cmake_minimum_required(VERSION 3.22)

# Universal binary, macOS 11+. Must be set before project().
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

project(Clipotype VERSION 1.1.0)

option(CLIPOTYPE_COPY_AFTER_BUILD "Copy plugin to the system plugin folder after build" ON)

# Static MSVC runtime so users don't need the VC++ Redistributable.
# Must be set before JUCE is added so it applies to all targets.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(CLIPOTYPE_FORMATS VST3 AU Standalone)
else()
    set(CLIPOTYPE_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Clipotype
    COMPANY_NAME "Mustafa"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD ${CLIPOTYPE_COPY_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE Mstf
    PLUGIN_CODE Clp1
    FORMATS ${CLIPOTYPE_FORMATS}
    HARDENED_RUNTIME_ENABLED TRUE
    HARDENED_RUNTIME_OPTIONS com.apple.security.device.audio-input
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Clipotype needs audio input to process sound in standalone mode."
    PRODUCT_NAME "Clipotype")

juce_add_binary_data(ClipotypeAssets
    SOURCES
        Assets/SpaceMono-Bold.ttf
        Assets/RobotoMono-VariableFont_wght.ttf)

target_sources(Clipotype
    PRIVATE
        Source/ClipotypeLookAndFeel.cpp
        Source/FrequencyMapComponent.cpp
        Source/PillToggleButton.cpp
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp)

target_compile_definitions(Clipotype
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Clipotype
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        ClipotypeAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
