cmake_minimum_required(VERSION 3.30)
project(wavmarker VERSION 0.0.1)

set(PRE_RELEASE_VERSION "" CACHE STRING "Optional prerelease suffix, e.g. -beta1")

set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")
configure_file(
        "${CMAKE_SOURCE_DIR}/version.h.in"
        "${GENERATED_DIR}/version.h"
        @ONLY
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-DPROJECT_DIR="${CMAKE_SOURCE_DIR}")

add_executable(wavmarker
        main.cpp
        Parser.cpp
        Parser.h
        Generator.h
        FileStream.h
        nodes/Container.h
        nodes/Reflection.h
        nodes/JSON/ast/JSONValue.cpp
        nodes/JSON/ast/JSONValue.h
        nodes/JSON/visitor/JSONPrintVisitor.h
        nodes/JSON/visitor/JSONVisitor.h
        nodes/wavmarker/WavFile.cpp
        nodes/wavmarker/WavFile.h
        util/Error.cpp
        util/Error.h
        util/StreamUtils.h
        util/StringUtils.h
)

target_include_directories(wavmarker
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GENERATED_DIR}
)
