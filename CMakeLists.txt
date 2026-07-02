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
        src/Parser.cpp
        src/Parser.h
        src/Generator.h
        util/FileStream.h
        src/Container.h
        src/Reflection.h
        src/JSON/ast/JSONValue.cpp
        src/JSON/ast/JSONValue.h
        src/JSON/visitor/JSONPrintVisitor.h
        src/JSON/visitor/JSONVisitor.h
        src/wavmarker/WavFile.cpp
        src/wavmarker/WavFile.h
        util/Error.cpp
        util/Error.h
        util/StreamUtils.h
        util/StringUtils.h
        src/wavmarker/ChunkKind.h
        src/wavmarker/Patcher.h
        src/CliApplication.h
        src/CliApplication.cpp
        src/CliHandling.h
)

target_include_directories(wavmarker
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GENERATED_DIR}
)
