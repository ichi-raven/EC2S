cmake_minimum_required(VERSION 3.20)

project(MyProject VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Collect header files for IDE visibility
file(GLOB_RECURSE PROJECT_HEADERS
    CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/include/*.hpp
    ${CMAKE_SOURCE_DIR}/include/*.h
)

# Header-only library
add_library(MyProject INTERFACE)

target_compile_features(MyProject
    INTERFACE cxx_std_20
)

target_include_directories(MyProject
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Register headers to the target so IDE shows them
target_sources(MyProject
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
