@echo off
setlocal

rem Usage: build.bat [release^|debug]
set "BUILD_TYPE=%~1"
if not defined BUILD_TYPE set "BUILD_TYPE=release"

if /I "%BUILD_TYPE%"=="release" (
    set "BUILD_DIR=cmake-build-release"
    set "CMAKE_BUILD_TYPE=Release"
) else if /I "%BUILD_TYPE%"=="debug" (
    set "BUILD_DIR=cmake-build-debug"
    set "CMAKE_BUILD_TYPE=Debug"
) else (
    echo Usage: %~nx0 [release^|debug]
    exit /b 1
)

where cmake >nul 2>&1
if errorlevel 1 (
    echo Error: cmake not found in PATH.
    exit /b 1
)

where ninja >nul 2>&1
if errorlevel 1 (
    echo Error: ninja not found in PATH.
    exit /b 1
)

if not defined CC set "CC=clang"
if not defined CXX set "CXX=clang++"

where "%CC%" >nul 2>&1
if errorlevel 1 (
    echo Error: C compiler "%CC%" not found in PATH.
    exit /b 1
)

where "%CXX%" >nul 2>&1
if errorlevel 1 (
    echo Error: C++ compiler "%CXX%" not found in PATH.
    exit /b 1
)

echo Building wavmarker in %CMAKE_BUILD_TYPE% mode...
echo Build directory: %BUILD_DIR%
echo C compiler: %CC%
echo C++ compiler: %CXX%

cmake -S . -B "%BUILD_DIR%" -G Ninja ^
    -DCMAKE_BUILD_TYPE="%CMAKE_BUILD_TYPE%" ^
    -DCMAKE_C_COMPILER="%CC%" ^
    -DCMAKE_CXX_COMPILER="%CXX%"
if errorlevel 1 (
    echo Error: CMake configuration failed.
    exit /b 1
)

cmake --build "%BUILD_DIR%" --parallel
if errorlevel 1 (
    echo Error: Build failed.
    exit /b 1
)

set "BINARY=%BUILD_DIR%\wavmarker.exe"
if not exist "%BINARY%" (
    echo Error: wavmarker executable not found at %BINARY%.
    exit /b 1
)

echo Build successful: %BINARY%
endlocal
