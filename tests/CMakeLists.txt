include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(chordsearch_tests
    test_PitchClassSet.cpp
    test_MidiNoteSet.cpp
    test_ChordRecognizer.cpp
    test_Harmony.cpp
    test_Backing.cpp
)

target_link_libraries(chordsearch_tests PRIVATE chordsearch_core Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(chordsearch_tests)
