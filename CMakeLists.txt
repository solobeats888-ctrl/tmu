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

if(APPLE)
    set(TMU_FORMATS AU VST3)
else()
    set(TMU_FORMATS VST3)   # Windows: solo VST3
endif()

juce_add_plugin(TMU
    COMPANY_NAME "Viszeversza"
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
    FORMATS ${TMU_FORMATS})

juce_generate_juce_header(TMU)

# lista de sonidos repetidos de la biblioteca (tools: tmufp + analisis de huellas)
file(GLOB TMU_FONTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/fonts/*.ttf" "${CMAKE_CURRENT_SOURCE_DIR}/Resources/fonts/*.otf")
juce_add_binary_data(TMUData SOURCES Resources/repetidos.tsv Resources/viszeversa.png ${TMU_FONTS})

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
    PRIVATE TMUData juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
