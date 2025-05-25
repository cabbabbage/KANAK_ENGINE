@echo off
setlocal

:: Step 1: Confirm critical files exist
echo Checking required files and directories...

if not exist "CMakeLists.txt" (
    echo ❌ Missing CMakeLists.txt in root directory.
    pause
    exit /b 1
)

if not exist "ENGINE\main.cpp" (
    echo ❌ Missing ENGINE/main.cpp
    pause
    exit /b 1
)

if not exist "ENGINE\engine.cpp" (
    echo ❌ Missing ENGINE/engine.cpp
    pause
    exit /b 1
)

if not exist "vcpkg\scripts\buildsystems\vcpkg.cmake" (
    echo ❌ Missing vcpkg toolchain file at vcpkg/scripts/buildsystems/vcpkg.cmake
    pause
    exit /b 1
)

if not exist "SRC\Map.json" (
    echo ❌ Missing SRC/Map.json
    pause
    exit /b 1
)

echo ✅ All required files found.

:: Step 2: Clean old build
echo Cleaning previous build...
rmdir /s /q build >nul 2>&1
mkdir build

:: Step 3: Run CMake to configure
echo Configuring project with CMake...
cmake -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake" ^
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="%cd%\ENGINE" ^
  -B build

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

:: Step 5: Copy assets to release folder
echo Copying SRC assets to release folder...
xcopy /E /I /Y "SRC" "engine\Release\SRC"

:: Step 6: Run executable
echo Launching built game...

set EXE1=ENGINE\engine.exe
set EXE2=build\Release\engine.exe

if exist "%EXE1%" (
    echo ✅ Build succeeded.
    echo Running: %EXE1%
    "%EXE1%"
) else if exist "%EXE2%" (
    echo ✅ Build succeeded.
    echo Running: %EXE2%"
    "%EXE2%"
) else (
    echo ❌ Executable not found at %EXE1% or %EXE2%
)

pause
endlocal
