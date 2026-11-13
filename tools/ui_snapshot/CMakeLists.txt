# Renders the plugin editor to a PNG with a scripted chord progression.
# Usage: ./build/tools/ui_snapshot/ChordsearchUISnapshot_artefacts/<cfg>/ChordsearchUISnapshot out.png
juce_add_console_app(ChordsearchUISnapshot PRODUCT_NAME "ChordsearchUISnapshot")

set(PLUGIN_DIR ${CMAKE_SOURCE_DIR}/plugin)

target_sources(ChordsearchUISnapshot PRIVATE
    main.cpp
    ${PLUGIN_DIR}/PluginProcessor.cpp
    ${PLUGIN_DIR}/PluginEditor.cpp
    ${PLUGIN_DIR}/MidiInputTracker.cpp
    ${PLUGIN_DIR}/ui/KeyboardView.cpp
    ${PLUGIN_DIR}/ui/Panels.cpp
    ${PLUGIN_DIR}/ui/ProgressionStrip.cpp
    ${PLUGIN_DIR}/ui/ChordPicker.cpp
    ${PLUGIN_DIR}/ui/BackingPanel.cpp
    ${PLUGIN_DIR}/ui/LeadSheetView.cpp)

target_include_directories(ChordsearchUISnapshot PRIVATE ${PLUGIN_DIR})

target_compile_definitions(ChordsearchUISnapshot PRIVATE
    JucePlugin_Name="Chordsearch"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(ChordsearchUISnapshot PRIVATE
    chordsearch_core
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_recommended_config_flags)
