cmake_minimum_required(VERSION 3.20)

project(EC2S VERSION 1.0 LANGUAGES CXX)
set(EC2S_LIB_NAME ec2s)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "64-bit build required.")
endif()

include(FetchContent)
FetchContent_Declare(
    concurrentqueue
    GIT_REPOSITORY https://github.com/cameron314/concurrentqueue.git
    GIT_TAG 9afb99746f0f5fc94ac8aef737053ae0481ba8d1 # v1.0.5
)
FetchContent_MakeAvailable(concurrentqueue)

# Collect header files for IDE visibility
file(GLOB_RECURSE PROJECT_HEADERS
    CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/include/*.hpp
    ${CMAKE_SOURCE_DIR}/include/*.h
)

# Header-only library
add_library(${EC2S_LIB_NAME} INTERFACE)

target_compile_features(${EC2S_LIB_NAME}
    INTERFACE cxx_std_20
)

target_link_libraries(${EC2S_LIB_NAME}
    INTERFACE concurrentqueue
)

target_include_directories(${EC2S_LIB_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Register headers to the target so IDE shows them
target_sources(${EC2S_LIB_NAME}
    INTERFACE
        ${PROJECT_HEADERS}
)

# Optional: create IDE folder structure
source_group(
    TREE ${CMAKE_SOURCE_DIR}/include
    PREFIX "Header Files"
    FILES ${PROJECT_HEADERS}
)

add_subdirectory(test)

enable_testing()
add_subdirectory(GoogleTest)
