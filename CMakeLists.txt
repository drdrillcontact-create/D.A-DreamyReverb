cmake_minimum_required(VERSION 3.22)

# macOS: universal binary (Apple Silicon + Intel). Must be set before project().
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)

# Windows: link the MSVC runtime statically so users need no redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(DreamyReverb VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(DREAMY_BUILD_TESTS "Build the DSP and plugin self tests" OFF)
option(DREAMY_BUILD_INSTALLER "Build the D.A AURA installer app" ON)

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

# "D.A AURA" is a separate plugin with its own code and bundle id. The older
# "D.A Dreamy Reverb" (code Drvb, com.da.dreamyreverb) and "D.A Dreamy Shimmer
# Reverb" (code Dshr, com.da.dreamyshimmerreverb) may still be installed next to
# it: two plugins sharing one id make hosts load only one of them.
juce_add_plugin(DreamyReverb
    COMPANY_NAME                "D.A"
    BUNDLE_ID                   "com.da.aura"
    PLUGIN_MANUFACTURER_CODE    Drma
    PLUGIN_CODE                 Aura
    FORMATS                     ${DREAMY_FORMATS}
    PRODUCT_NAME                "D.A AURA"
    DESCRIPTION                 "Lush shimmer reverb"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Reverb
    AU_MAIN_TYPE                kAudioUnitType_Effect)

# Embedded assets: the Jost typeface (SIL Open Font License - see OFL.txt) and
# the installer background image.
juce_add_binary_data(DreamyAssets SOURCES
    Assets/Fonts/JostLight.ttf
    Assets/Fonts/JostBook.ttf
    Assets/Fonts/JostMedium.ttf
    Assets/Installer/installer-welcome.png)

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

# ---- the installer app (D.A AURA Installer) ------------------------------------------
# A small GUI app that shows the welcome screen, lets the user pick the install
# folder (default C:\Program Files\Common Files\VST3 / ~/Library/Audio/Plug-Ins),
# copies the plugin bundle(s) there and shows a Finish button. Resizable.
if(DREAMY_BUILD_INSTALLER)
    juce_add_gui_app(DreamyInstaller
        COMPANY_NAME    "D.A"
        PRODUCT_NAME    "D.A AURA Installer"
        BUNDLE_ID       "com.da.aura.installer"
        VERSION         "2.0.0")

    juce_generate_juce_header(DreamyInstaller)

    target_sources(DreamyInstaller PRIVATE
        Source/Installer/InstallerApp.cpp)

    target_compile_definitions(DreamyInstaller PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

    target_link_libraries(DreamyInstaller PRIVATE
        DreamyAssets
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

    # the installer reads the plugin bundles from a "plugins" folder next to
    # the built app, so the workflow places them there before packaging
    if(APPLE)
        target_compile_definitions(DreamyInstaller PRIVATE DREAMY_INSTALLER_DEFAULT_DIR="/Library/Audio/Plug-Ins")
    else()
        target_compile_definitions(DreamyInstaller PRIVATE DREAMY_INSTALLER_DEFAULT_DIR="C:/Program Files/Common Files/VST3")
    endif()
endif()

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
