cmake_minimum_required(VERSION 3.22)

# macOS: universal binary (Apple Silicon + Intel). Must be set before project().
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)

# Windows: link the MSVC runtime statically so users need no redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(DreamyReverb VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DREAMY_BUILD_TESTS "Build the DSP and plugin self tests" OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(DREAMY_FORMATS VST3 AU)
else()
    set(DREAMY_FORMATS VST3)
endif()

# The plugin codes and the bundle id are the same as in version 1.0 ("D.A Dreamy Reverb"),
# so existing sessions keep loading after the rename.
juce_add_plugin(DreamyReverb
    COMPANY_NAME                "D.A"
    BUNDLE_ID                   "com.da.dreamyreverb"
    PLUGIN_MANUFACTURER_CODE    Drma
    PLUGIN_CODE                 Drvb
    FORMATS                     ${DREAMY_FORMATS}
    PRODUCT_NAME                "D.A Dreamy Shimmer Reverb"
    DESCRIPTION                 "Lush shimmer reverb"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Reverb
    AU_MAIN_TYPE                kAudioUnitType_Effect)

# Embedded typeface (Jost, SIL Open Font License - see Assets/Fonts/OFL.txt)
juce_add_binary_data(DreamyAssets SOURCES
    Assets/Fonts/JostLight.ttf
    Assets/Fonts/JostBook.ttf
    Assets/Fonts/JostMedium.ttf)

juce_generate_juce_header(DreamyReverb)

target_sources(DreamyReverb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(DreamyReverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(DreamyReverb
    PRIVATE
        DreamyAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# ---- self tests (used by the GitHub workflow: -DDREAMY_BUILD_TESTS=ON) ------------------
if(DREAMY_BUILD_TESTS)
    enable_testing()

    # the reverb algorithm on its own (no JUCE)
    add_executable(DreamyDspTest Tests/DspTest.cpp)
    target_include_directories(DreamyDspTest PRIVATE Source)
    add_test(NAME DreamyDspTest COMMAND DreamyDspTest)

    # the complete plugin (processor, presets, state, editor) without a host
    add_executable(DreamyPluginTest Tests/PluginTest.cpp)
    target_include_directories(DreamyPluginTest PRIVATE
        Source
        $<TARGET_PROPERTY:DreamyReverb,INCLUDE_DIRECTORIES>)
    target_compile_definitions(DreamyPluginTest PRIVATE
        $<TARGET_PROPERTY:DreamyReverb,COMPILE_DEFINITIONS>)
    target_link_libraries(DreamyPluginTest PRIVATE DreamyReverb)
endif()
