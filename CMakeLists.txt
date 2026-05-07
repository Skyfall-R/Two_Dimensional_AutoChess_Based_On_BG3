cmake_minimum_required(VERSION 3.21)

project(
    AutoChess
    VERSION 2.0.0
    LANGUAGES CXX
)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AUTOCHESS_BUILD_GUI "Build the raylib 2D GUI" ON)
option(AUTOCHESS_BUILD_TESTS "Build the core test executable" ON)

add_library(autochess_core
    lib/header/lib.hpp
    lib/lib.cpp
)
target_include_directories(autochess_core PUBLIC lib/header)
target_compile_features(autochess_core PUBLIC cxx_std_17)

add_executable(autochess_cli
    main/main.cpp
)
target_link_libraries(autochess_cli PRIVATE autochess_core)

if(AUTOCHESS_BUILD_TESTS)
    enable_testing()
    add_executable(autochess_tests
        tests/test_core.cpp
    )
    target_link_libraries(autochess_tests PRIVATE autochess_core)
    add_test(NAME autochess_core_tests COMMAND autochess_tests)
endif()

if(AUTOCHESS_BUILD_GUI)
    find_package(raylib QUIET)
    if(NOT raylib_FOUND)
        include(FetchContent)
        set(FETCHCONTENT_QUIET OFF)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            raylib
            URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(raylib)
    endif()

    add_executable(autochess_gui
        gui/main.cpp
    )
    target_link_libraries(autochess_gui PRIVATE autochess_core raylib)
endif()
