cmake_minimum_required(VERSION 3.22.1)
project(epaudio)

add_library(epaudio SHARED
        native-lib.cpp
        AudioEngine.cpp
        SampleBank.cpp
        PhysicalViolin.cpp
        InstrumentModels.cpp
        DreamyEffect.cpp
        SpaceEffect.cpp
        TapeEffect.cpp)

find_library(log-lib log)
find_library(android-lib android)
find_library(aaudio-lib aaudio)

target_link_libraries(epaudio
        ${log-lib}
        ${android-lib}
        ${aaudio-lib})
