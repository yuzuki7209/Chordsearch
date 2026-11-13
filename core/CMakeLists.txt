# Pure C++ music-theory engine. Must not depend on JUCE.
add_library(chordsearch_core STATIC
    src/NoteNames.cpp
    src/Chord.cpp
    src/ChordRecognizer.cpp
    src/Key.cpp
    src/Harmony.cpp
    src/Scales.cpp
    src/KeyDetector.cpp
    src/ProgressionLibrary.cpp
    src/ChordSuggester.cpp
    src/AnalysisEngine.cpp
    src/backing/Generator.cpp
    src/backing/Synth.cpp
)

target_include_directories(chordsearch_core PUBLIC include)
target_compile_features(chordsearch_core PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(chordsearch_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
