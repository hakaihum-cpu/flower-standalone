cmake_minimum_required(VERSION 3.22)
project(EP_Live_Instrument_Sampler VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LIVE_SAMPLER_CORE_ONLY "Build only the platform-independent Core tests" OFF)

enable_testing()

add_executable(LiveInstrumentSamplerCoreTests
    tests/test_core.cpp
    Core/WavIO.cpp
    Core/SamplerCore.cpp
)
target_include_directories(LiveInstrumentSamplerCoreTests PRIVATE Core)
target_compile_features(LiveInstrumentSamplerCoreTests PRIVATE cxx_std_20)
add_test(NAME LiveInstrumentSamplerCoreTests COMMAND LiveInstrumentSamplerCoreTests)

if (NOT LIVE_SAMPLER_CORE_ONLY)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)

    juce_add_gui_app(EP_Live_Instrument_Sampler
        PRODUCT_NAME "EP Live Instrument Sampler"
        COMPANY_NAME "analoglav"
    )

    juce_generate_juce_header(EP_Live_Instrument_Sampler)

    target_sources(EP_Live_Instrument_Sampler PRIVATE
        App/Main.cpp
        App/MainComponentUi.cpp
        App/MainComponentAudio.cpp
        App/MainComponentSession.cpp
        App/MainComponentFinish.cpp
        App/MainComponent.h
        App/AudioEngine.cpp
        App/AudioEngine.h
        App/WaveformTrimComponent.cpp
        App/WaveformTrimComponent.h
        Core/WavIO.cpp
        Core/WavIO.h
        Core/SamplerCore.cpp
        Core/SamplerCore.h
    )

    target_compile_definitions(EP_Live_Instrument_Sampler PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )

    if (WIN32)
        target_compile_definitions(EP_Live_Instrument_Sampler PRIVATE JUCE_ASIO=1 JUCE_WASAPI=1)
    endif()

    target_link_libraries(EP_Live_Instrument_Sampler PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )
endif()
