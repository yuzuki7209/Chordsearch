# Ableton Live does not host VST/AU MIDI effects, so Chordsearch is built as an
# instrument that receives MIDI (and outputs the backing band).
if(APPLE)
    set(CHORDSEARCH_FORMATS VST3 AU Standalone)
    set(CHORDSEARCH_COPY_AFTER_BUILD TRUE)
else()
    set(CHORDSEARCH_FORMATS VST3 Standalone)
    set(CHORDSEARCH_COPY_AFTER_BUILD FALSE) # system VST3 folder needs admin rights on Windows
endif()

juce_add_plugin(Chordsearch
    COMPANY_NAME                "Chordsearch"
    PRODUCT_NAME                "Chordsearch"
    BUNDLE_ID                   com.chordsearch.plugin
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${CHORDSEARCH_COPY_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE    Cdsr
    PLUGIN_CODE                 Chs1
    FORMATS                     ${CHORDSEARCH_FORMATS}
    VST3_CATEGORIES             Instrument Tools
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

target_sources(Chordsearch PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    MidiInputTracker.cpp
    ui/KeyboardView.cpp
    ui/Panels.cpp
    ui/ProgressionStrip.cpp
    ui/ChordPicker.cpp
    ui/BackingPanel.cpp
    ui/LeadSheetView.cpp)

target_include_directories(Chordsearch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(Chordsearch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Chordsearch
    PRIVATE
        chordsearch_core
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
