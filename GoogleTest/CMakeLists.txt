include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG main
)

# for Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS *.cpp)

add_executable(UnitTests ${TEST_SOURCES})

target_link_libraries(UnitTests
    PRIVATE
        ${EC2S_LIB_NAME}
        gtest
        gtest_main
)

include(GoogleTest)
gtest_discover_tests(UnitTests)
