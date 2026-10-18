cmake_minimum_required(VERSION 3.22)
project(ReplicantEdgy VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------------------
# JUCE dependency (same JUCE 8 line as Replicant303)
# ------------------------------------------------------------------------------
include(FetchContent)

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(JUCE)

# ------------------------------------------------------------------------------
# Sources shared by the plugin and every tool, so they can never drift.
# ------------------------------------------------------------------------------
set(REDG_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h

    # DSP
    Source/DSP/CommonEdgy.h
    Source/DSP/OscillatorEdgy.h
    Source/DSP/OscillatorEdgy.cpp
    Source/DSP/NoiseEdgy.h
    Source/DSP/LadderFilter.h
    Source/DSP/LadderFilter.cpp
    Source/DSP/EnvelopeEdgy.h
    Source/DSP/StepSequencer.h
    Source/DSP/StepSequencer.cpp
    Source/DSP/PatchBay.h
    Source/DSP/PatchBay.cpp
    Source/DSP/SynthEngine.h
    Source/DSP/SynthEngine.cpp

    # Presets
    Source/Presets/Preset.h
    Source/Presets/Preset.cpp
    Source/Presets/PresetManager.h
    Source/Presets/PresetManager.cpp

    # GUI
    Source/GUI/EdgyTheme.h
    Source/GUI/EdgyTheme.cpp
    Source/GUI/EdgyLookAndFeel.h
    Source/GUI/EdgyLookAndFeel.cpp
    Source/GUI/PanelLayout.h
    Source/GUI/PresetDrawer.h
    Source/GUI/PresetDrawer.cpp
)

set(REDG_INCLUDE_DIRS Source Source/DSP Source/GUI Source/Presets)
set(REDG_JUCE_LIBS
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# ------------------------------------------------------------------------------
# Plugin target
# ------------------------------------------------------------------------------
set(PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

juce_add_plugin(ReplicantEdgy
    COMPANY_NAME "DanimanX"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE DnmX
    PLUGIN_CODE REDG
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "ReplicantEdgy"
    DESCRIPTION "Cyberpunk Semi-Modular Percussion Synthesizer with a Patchable 8-Step Sequencer"
    BUNDLE_ID "com.danimanx.replicantedgy"
)

target_sources(ReplicantEdgy PRIVATE ${REDG_SOURCES})

target_compile_definitions(ReplicantEdgy PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_include_directories(ReplicantEdgy PRIVATE ${REDG_INCLUDE_DIRS})
target_link_libraries(ReplicantEdgy PRIVATE ${REDG_JUCE_LIBS})

# ------------------------------------------------------------------------------
# Internal tools (excluded from the default build)
# ------------------------------------------------------------------------------
function(redg_add_tool target source)
    add_executable(${target} EXCLUDE_FROM_ALL ${source} ${REDG_SOURCES})
    target_include_directories(${target} PRIVATE ${REDG_INCLUDE_DIRS})
    target_compile_definitions(${target} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
    )
    target_link_libraries(${target} PRIVATE ${REDG_JUCE_LIBS})
endfunction()

# Renders the editor off-screen for the README / promo images.
redg_add_tool(generate_snapshot tools/generate_snapshot.cpp)

# Headless regression suite (DSP, sequencer, patch bay, presets, editor).
redg_add_tool(run_tests tools/run_tests.cpp)

# Renders the cyberpunk demo track through the real plugin DSP.
redg_add_tool(render_audio_demo tools/render_audio_demo.cpp)

# Writes the factory preset examples into presets/.
redg_add_tool(generate_presets tools/generate_presets.cpp)
