cmake_minimum_required(VERSION 3.22)

project(PokeChord VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows : runtime statique, pas besoin d'installer les redistribuables Visual C++
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(JUCE)

# Les images (Pokemon + Poke Ball) sont integrees dans le plugin
juce_add_binary_data(PokeChordAssets SOURCES
    Assets/mon0.png
    Assets/mon1.png
    Assets/mon2.png
    Assets/mon3.png
    Assets/mon4.png
    Assets/mon5.png
    Assets/mon6.png
    Assets/mon7.png
    Assets/mon8.png
    Assets/mon9.png
    Assets/mon10.png
    Assets/ball.png
    Assets/noctali.png
    Assets/Poppins-Bold.ttf
    Assets/Poppins-Medium.ttf)

juce_add_plugin(PokeChord
    COMPANY_NAME "MyPlugins"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE Mypl
    PLUGIN_CODE Pkch
    FORMATS VST3
    PRODUCT_NAME "PokeChord"
    PLUGIN_NAME "PokeChord"
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(PokeChord PRIVATE
    Source/ChordEngine.cpp
    Source/ChordEngine.h
    Source/ChordAnalyser.cpp
    Source/ChordAnalyser.h
    Source/SoundPresets.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_definitions(PokeChord PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

# Les textes contiennent des accents : on force l'UTF-8 sous Windows
target_compile_options(PokeChord PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

target_link_libraries(PokeChord
    PRIVATE
        PokeChordAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags)
