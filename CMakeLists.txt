cmake_minimum_required(VERSION 3.22)

# macOS Universal Binary (Apple Silicon + Intel) - must be set before project().
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS build architectures" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target" FORCE)
endif()

# Windows: link MSVC runtime statically so end users do not need a VC++ Redistributable.
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)

project(DAAURA VERSION 1.0.0 LANGUAGES C CXX)

set(DAAURA_PRODUCT_NAME "D.A AURA")
set(DAAURA_COMPANY_NAME "D.A Production")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(DAAURA_FORMATS AU VST3)
else()
    set(DAAURA_FORMATS VST3)
endif()

juce_add_plugin(DAAURA
    COMPANY_NAME                "${DAAURA_COMPANY_NAME}"
    BUNDLE_ID                   "com.da.aura"
    PLUGIN_MANUFACTURER_CODE    DApr
    PLUGIN_CODE                 Aura
    FORMATS                     ${DAAURA_FORMATS}
    PLUGIN_NAME                 "${DAAURA_PRODUCT_NAME}"
    PRODUCT_NAME                "${DAAURA_PRODUCT_NAME}"
    DESCRIPTION                 "${DAAURA_PRODUCT_NAME}"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Reverb
    AU_MAIN_TYPE                kAudioUnitType_Effect
)

juce_generate_juce_header(DAAURA)

target_sources(DAAURA
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(DAAURA
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(DAAURA
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

# macOS Apple Silicon requires every loaded code bundle to have at least an ad-hoc signature.
if(APPLE)
    add_custom_command(TARGET DAAURA_VST3 POST_BUILD
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:DAAURA_VST3>"
        COMMENT "Ad-hoc signing D.A AURA VST3 bundle"
    )
    add_custom_command(TARGET DAAURA_AU POST_BUILD
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:DAAURA_AU>"
        COMMENT "Ad-hoc signing D.A AURA AU bundle"
    )
endif()

#==============================================================================
# The installer: a JUCE desktop application built with the same framework as the plug-in.
#
#   Windows   "D.A AURA Installer.exe"   carries the VST3. It asks for administrator rights when
#                                        started, so that it may write into
#                                        C:\Program Files\Common Files\VST3.
#   macOS     "D.A AURA Installer.app"   carries the Audio Unit and the VST3.
if(WIN32 OR APPLE)
    juce_add_gui_app(DAAURAInstaller
        PRODUCT_NAME    "${DAAURA_PRODUCT_NAME} Installer"
        COMPANY_NAME    "${DAAURA_COMPANY_NAME}"
        BUNDLE_ID       "com.da.aura.installer"
        ICON_BIG        "${CMAKE_CURRENT_SOURCE_DIR}/Installer/Assets/icon.png"
        ICON_SMALL      "${CMAKE_CURRENT_SOURCE_DIR}/Installer/Assets/icon.png"
    )

    # The design's picture, the three patches that replace its words and buttons on the later
    # pages (made from Installer/Design/Installer.png by Installer/Design/make-assets.py), and
    # the Red Hat Display typefaces used on those pages: compiled into the installer.
    juce_add_binary_data(DAAURAInstallerData
        HEADER_NAME InstallerData.h
        NAMESPACE   InstallerData
        SOURCES
            Installer/Assets/picture.png
            Installer/Assets/patchText.png
            Installer/Assets/patchNoButtons.png
            Installer/Assets/patchFinish.png
            Installer/Assets/RedHatDisplayRegular.ttf
            Installer/Assets/RedHatDisplayMedium.ttf
            Installer/Assets/RedHatDisplaySemiBold.ttf
            Installer/Assets/RedHatDisplayBold.ttf
            Installer/Assets/RedHatDisplayExtraBold.ttf
    )

    # The names and the version, written once for the installer's code to read
    set(DAAURA_INSTALLER_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/installer")
    configure_file(Installer/InstallerBuildConfig.h.in "${DAAURA_INSTALLER_GENERATED}/InstallerBuildConfig.h" @ONLY)

    target_sources(DAAURAInstaller
        PRIVATE
            Installer/Source/InstallerArt.h
            Installer/Source/InstallerAssets.cpp
            Installer/Source/InstallerAssets.h
            Installer/Source/InstallerConfig.h
            Installer/Source/InstallerEngine.cpp
            Installer/Source/InstallerEngine.h
            Installer/Source/InstallerMain.cpp
            Installer/Source/InstallerPicture.cpp
            Installer/Source/InstallerPicture.h
            Installer/Source/InstallerShell.h
            Installer/Source/InstallerView.cpp
            Installer/Source/InstallerView.h
            Installer/Source/InstallerWindow.h
            Installer/Source/PayloadWindows.cpp
            Installer/Source/SystemMouse.cpp
            Installer/Source/SystemWindow.cpp
    )

    target_include_directories(DAAURAInstaller PRIVATE "${DAAURA_INSTALLER_GENERATED}")

    target_compile_definitions(DAAURAInstaller
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
    )

    target_link_libraries(DAAURAInstaller
        PRIVATE
            DAAURAInstallerData
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
    )

    if(APPLE)
        # (for the installer's --self-test: moving the mouse through CoreGraphics)
        find_library(DAAURA_APPLICATION_SERVICES ApplicationServices REQUIRED)
        target_link_libraries(DAAURAInstaller PRIVATE "${DAAURA_APPLICATION_SERVICES}")
    endif()

    # The plug-in bundles that were just built, packed into one zip with the layout
    # InstallerEngine.h expects: VST3/D.A AURA.vst3/... (and AU/D.A AURA.component/... on macOS).
    set(DAAURA_PAYLOAD_ZIP "${DAAURA_INSTALLER_GENERATED}/payload.zip")
    set(DAAURA_PAYLOAD_FROM "$<GENEX_EVAL:$<TARGET_PROPERTY:DAAURA,LIBRARY_OUTPUT_DIRECTORY>>")

    set(DAAURA_PAYLOAD_CONTENTS "VST3/${DAAURA_PRODUCT_NAME}.vst3")
    set(DAAURA_PAYLOAD_TARGETS DAAURA_VST3)

    if(APPLE)
        list(APPEND DAAURA_PAYLOAD_CONTENTS "AU/${DAAURA_PRODUCT_NAME}.component")
        list(APPEND DAAURA_PAYLOAD_TARGETS DAAURA_AU)
    endif()

    add_custom_target(DAAURAInstallerPayload
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DAAURA_INSTALLER_GENERATED}"
        COMMAND "${CMAKE_COMMAND}" -E rm -f "${DAAURA_PAYLOAD_ZIP}"
        COMMAND "${CMAKE_COMMAND}" -E chdir "${DAAURA_PAYLOAD_FROM}"
                "${CMAKE_COMMAND}" -E tar cf "${DAAURA_PAYLOAD_ZIP}" --format=zip -- ${DAAURA_PAYLOAD_CONTENTS}
        BYPRODUCTS "${DAAURA_PAYLOAD_ZIP}"
        COMMENT "Packing ${DAAURA_PRODUCT_NAME} plug-in bundles for the installer"
    )

    add_dependencies(DAAURAInstallerPayload ${DAAURA_PAYLOAD_TARGETS})
    add_dependencies(DAAURAInstaller DAAURAInstallerPayload)

    if(WIN32)
        file(TO_CMAKE_PATH "${DAAURA_PAYLOAD_ZIP}" DAAURA_PAYLOAD_RC_PATH)
        configure_file(Installer/payload.rc.in "${DAAURA_INSTALLER_GENERATED}/payload.rc" @ONLY)
        target_sources(DAAURAInstaller PRIVATE "${DAAURA_INSTALLER_GENERATED}/payload.rc")
        set_source_files_properties("${DAAURA_INSTALLER_GENERATED}/payload.rc" PROPERTIES OBJECT_DEPENDS "${DAAURA_PAYLOAD_ZIP}")

        # C:\Program Files\Common Files\VST3 belongs to administrators: ask Windows on start-up.
        if(MSVC)
            set_property(TARGET DAAURAInstaller APPEND_STRING PROPERTY
                LINK_FLAGS " /MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
        endif()
    elseif(APPLE)
        # Put payload.zip into the .app bundle and re-sign the outer bundle around it.
        add_custom_command(TARGET DAAURAInstaller POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:DAAURAInstaller>/Resources"
            COMMAND "${CMAKE_COMMAND}" -E copy "${DAAURA_PAYLOAD_ZIP}" "$<TARGET_BUNDLE_CONTENT_DIR:DAAURAInstaller>/Resources/payload.zip"
            COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:DAAURAInstaller>"
            COMMENT "Embedding plug-in payload and signing ${DAAURA_PRODUCT_NAME} Installer.app"
        )
    endif()
endif()
