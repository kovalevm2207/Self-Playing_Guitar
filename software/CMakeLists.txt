cmake_minimum_required(VERSION 3.28)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Use cmake --preset debug.")
endif()

project(SelfGuitar VERSION 0.1.0 LANGUAGES CXX)

option(GUITAR_BUILD_APP "Build the Qt desktop application" ON)

include(CTest)

add_library(guitar_protocol INTERFACE)
target_include_directories(guitar_protocol INTERFACE "${PROJECT_SOURCE_DIR}/shared")

function(guitar_configure_target target)
    target_compile_features(${target} PRIVATE cxx_std_17)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

if(GUITAR_BUILD_APP)
    find_package(Qt6 6.8 CONFIG REQUIRED COMPONENTS Quick QuickControls2 SerialPort)
    add_subdirectory(desktop)
endif()

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_custom_target(format
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/code_style.py" --fix
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Format C++ with LLVM 18 and QML with qmlformat"
    )
    add_custom_target(format-check
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/code_style.py"
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        COMMENT "Check C++ and QML formatting"
    )
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
