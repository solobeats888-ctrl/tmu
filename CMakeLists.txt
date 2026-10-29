cmake_minimum_required(VERSION 3.22)
project(TMU VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(TMU
    COMPANY_NAME "viszeversa"
    PRODUCT_NAME "TMU"
    BUNDLE_ID com.viszeversa.tmu
    PLUGIN_MANUFACTURER_CODE Vszv
    PLUGIN_CODE Tmu1
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS AU VST3)

juce_generate_juce_header(TMU)

target_sources(TMU PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Library.cpp
    Source/TMUCore.cpp
    Source/tsf_impl.cpp)

target_compile_definitions(TMU PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TMU
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
