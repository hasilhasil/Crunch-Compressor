cmake_minimum_required(VERSION 3.24)

project(CrunchCompressor VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# JUCE 8.0.4 setup: prefer local JUCE-8.0.4 tree so offline builds work.
# ---------------------------------------------------------------------------
if(NOT JUCE_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE-8.0.4/CMakeLists.txt")
        set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE-8.0.4")
    else()
        include(FetchContent)
        set(JUCE_VERSION "8.0.4" CACHE STRING "JUCE version fetched when JUCE_DIR is not set")
        FetchContent_Declare(
            juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        ${JUCE_VERSION}
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(juce)
        set(JUCE_DIR ${juce_SOURCE_DIR})
    endif()
endif()

add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce")

# ---------------------------------------------------------------------------
# Plugin target (Hasil brand identity + preserved VST3 manufacturer/plugin IDs)
# ---------------------------------------------------------------------------
juce_add_plugin(CrunchCompressor
    PRODUCT_NAME                "Crunch Compressor"
    COMPANY_NAME                "Hasil"
    BUNDLE_ID                   "com.hasil.crunchcompressor"
    VERSION                     1.0.0
    PLUGIN_MANUFACTURER_CODE    Kilo
    PLUGIN_CODE                 CCMP
    FORMATS                     VST3 Standalone
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Dynamics
)

juce_generate_juce_header(CrunchCompressor)

# Embedded SIL OFL 1.1 UI typefaces (Google Sans Flex + Noto Sans SC)
set(FONT_SOURCES
    src/ui/Fonts/GoogleSansFlex-Regular.ttf
    src/ui/Fonts/GoogleSansFlex-Medium.ttf
    src/ui/Fonts/GoogleSansFlex-SemiBold.ttf
    src/ui/Fonts/GoogleSansFlex-Bold.ttf
    src/ui/Fonts/NotoSansSC-Regular.ttf
    src/ui/Fonts/NotoSansSC-Medium.ttf
    src/ui/Fonts/NotoSansSC-Bold.ttf
)
juce_add_binary_data(CrunchCompressorData SOURCES ${FONT_SOURCES})

target_sources(CrunchCompressor PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/Parameters.cpp
    src/dsp/Compressor.cpp
    src/ui/Anim.h
    src/ui/CompTheme.h
    src/ui/EditorLayout.h
    src/ui/LookAndFeel.h
    src/ui/PackPopupOverlay.h
    src/ui/Knob.h
    src/ui/Knob.cpp
    src/ui/DisplayView.h
    src/ui/DisplayView.cpp
    src/ui/BandControlPanel.h
    src/ui/BandControlPanel.cpp
    src/ui/core/PaletteContract.h
    src/ui/core/ThemeContract.h
    src/ui/core/LayoutContract.h
    src/ui/core/LocaleRuntime.h
    src/ui/core/NeuPainter.h
    src/ui/core/UiComposition.h
    src/ui/packs/palettes/BuiltInPalettes.h
    src/ui/packs/themes/BuiltInThemes.h
    src/ui/packs/layouts/BuiltInLayouts.h
    src/ui/packs/locales/BuiltInLocales.h
)

target_compile_definitions(CrunchCompressor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(CrunchCompressor
    PRIVATE
        CrunchCompressorData
        juce::juce_audio_plugin_client
        juce::juce_audio_devices
        juce::juce_audio_utils
        juce::juce_recommended_config_flags
    PUBLIC
        juce::juce_recommended_lto_flags
)

if(MSVC)
    target_compile_options(CrunchCompressor PRIVATE /utf-8 /permissive- /MP $<$<CONFIG:Release>:/Gy /GF /Gw>)
    target_link_options(CrunchCompressor PUBLIC $<$<CONFIG:Release>:/OPT:REF /OPT:ICF>)
else()
    target_compile_options(CrunchCompressor PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Headless Colour DSP, Four-Pack UI & Snapshot Verification Suite
# ---------------------------------------------------------------------------
juce_add_console_app(CrunchCompressorTests PRODUCT_NAME "CrunchCompressorTests")
juce_generate_juce_header(CrunchCompressorTests)
target_sources(CrunchCompressorTests PRIVATE
    tests/CrunchCompressorVerify.cpp
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/Parameters.cpp
    src/dsp/Compressor.cpp
    src/ui/Knob.cpp
    src/ui/DisplayView.cpp
    src/ui/BandControlPanel.cpp
)
target_include_directories(CrunchCompressorTests PRIVATE src)
target_compile_definitions(CrunchCompressorTests PRIVATE
    JucePlugin_Name="Crunch Compressor"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)
target_link_libraries(CrunchCompressorTests PRIVATE
    CrunchCompressorData
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
)
if(MSVC)
    target_compile_options(CrunchCompressorTests PRIVATE /utf-8 /permissive- /MP)
else()
    target_compile_options(CrunchCompressorTests PRIVATE -Wall -Wextra)
endif()
