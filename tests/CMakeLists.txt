juce_add_console_app(PictureLockTests PRODUCT_NAME "PictureLockTests")

target_sources(PictureLockTests PRIVATE
    TestMain.cpp
    TimecodeTests.cpp
    PicturePlacementTests.cpp
    TransportClockTests.cpp
    VideoDecoderTests.cpp
    ProcessorSyncTests.cpp
    DecodeBenchmark.cpp)

picturelock_configure_target(PictureLockTests)

target_compile_definitions(PictureLockTests PRIVATE
    PICTURELOCK_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
    JucePlugin_Name="PictureLock"
    JUCE_MODAL_LOOPS_PERMITTED=1)

add_test(NAME PictureLockTests COMMAND PictureLockTests)
