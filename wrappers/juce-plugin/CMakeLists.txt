# JUCE is fetched rather than vendored, so the repository stays small and the
# version in use is a single line to change.
include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${STANKFACE_JUCE_TAG}
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(StankfacePlugin
    PRODUCT_NAME "Stankface"
    COMPANY_NAME "Stankface"
    BUNDLE_ID "com.stankface.synth"
    PLUGIN_MANUFACTURER_CODE Stnk
    PLUGIN_CODE Stkf
    FORMATS ${STANKFACE_PLUGIN_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${STANKFACE_INSTALL_PLUGIN}
)

target_sources(StankfacePlugin PRIVATE
    PluginProcessor.cpp
    ${CMAKE_SOURCE_DIR}/ui/PluginEditor.cpp
)

# The editor includes PluginProcessor.h and vice versa; both directories are on
# the path so neither has to know where the other sits.
target_include_directories(StankfacePlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ui
)

target_compile_definitions(StankfacePlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(StankfacePlugin
    PRIVATE
        stankface_engine
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
