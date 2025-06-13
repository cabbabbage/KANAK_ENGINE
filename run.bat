@echo off
setlocal

:: Step 1: Confirm critical files exist
echo Checking required files and directories...

if not exist "CMakeLists.txt" (
    echo [ERROR] Missing CMakeLists.txt in root directory.
    pause
    exit /b 1
)

if not exist "ENGINE\main.cpp" (
    echo [ERROR] Missing ENGINE/main.cpp
    pause
    exit /b 1
)

if not exist "ENGINE\engine.cpp" (
    echo [ERROR] Missing ENGINE/engine.cpp
    pause
    exit /b 1
)

if not exist "vcpkg\scripts\buildsystems\vcpkg.cmake" (
    echo [ERROR] Missing vcpkg toolchain file at vcpkg/scripts/buildsystems/vcpkg.cmake
    pause
    exit /b 1
)

echo ✅ All required files found.

:: Step 2: Clean old build cache
echo Cleaning previous CMake cache...
rmdir /s /q build >nul 2>&1
mkdir build

:: Step 3: Configure project with CMake
echo Configuring project...
cmake -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE="%cd%\vcpkg\scripts\buildsystems\vcpkg.cmake" ^
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="%cd%\ENGINE" ^
  -B build -S .

if errorlevel 1 (
    echo ❌ CMake configuration failed.
    pause
    exit /b 1
)

:: Step 4: Build project
echo Building project...
cmake --build build --config Release

if errorlevel 1 (
    echo ❌ Build failed.
    pause
    exit /b 1
)

:: Step 5: Run the executable
echo Launching built game...

set "EXE1=%cd%\ENGINE\engine.exe"
set "EXE2=%cd%\build\Release\engine.exe"

if exist "%EXE1%" (
    echo ✅ Build succeeded.
    echo Running: %EXE1%
    "%EXE1%"
) else if exist "%EXE2%" (
    echo ✅ Build succeeded.
    echo Running: %EXE2%
    "%EXE2%"
) else (
    echo ❌ Executable not found at %EXE1% or %EXE2%
)

pause
endlocal
