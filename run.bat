@echo off
setlocal enabledelayedexpansion

:: === Step 0: Ensure vcpkg exists and is bootstrapped ===
echo Checking vcpkg setup...
if not exist "vcpkg" (
    echo [INFO] Cloning vcpkg...
    git clone https://github.com/microsoft/vcpkg.git
    if errorlevel 1 (
        echo [ERROR] Failed to clone vcpkg.
        pause
        exit /b 1
    )
)
if not exist "vcpkg\vcpkg.exe" (
    echo [INFO] Bootstrapping vcpkg...
    pushd vcpkg
    call bootstrap-vcpkg.bat
    popd
    if not exist "vcpkg\vcpkg.exe" (
        echo [ERROR] vcpkg bootstrap failed.
        pause
        exit /b 1
    )
)

:: === Step 1: Confirm critical files exist ===
echo Checking required files and directories...
if not exist "CMakeLists.txt" (
    echo [ERROR] Missing CMakeLists.txt in root directory.
    pause & exit /b 1
)
if not exist "ENGINE\main.cpp" (
    echo [ERROR] Missing ENGINE\main.cpp
    pause & exit /b 1
)
if not exist "ENGINE\engine.cpp" (
    echo [ERROR] Missing ENGINE\engine.cpp
    pause & exit /b 1
)
if not exist "vcpkg\scripts\buildsystems\vcpkg.cmake" (
    echo [ERROR] Missing toolchain file at vcpkg\scripts\buildsystems\vcpkg.cmake
    pause & exit /b 1
)

:: === Step 2: Install dependencies if missing ===
set "VCPKG_TRIPLET=x64-windows"
set "DEPENDENCIES=sdl2 sdl2-image sdl2-mixer sdl2-ttf sdl2-gfx nlohmann-json glad"

if not exist "vcpkg\installed\!VCPKG_TRIPLET!" (
    echo Installing vcpkg dependencies: !DEPENDENCIES!  (triplet: !VCPKG_TRIPLET!)
    vcpkg\vcpkg install !DEPENDENCIES! --triplet !VCPKG_TRIPLET!
    if errorlevel 1 (
        echo [ERROR] vcpkg install failed.
        pause & exit /b 1
    )
)
echo ✅ Dependencies installed.

:: === Step 3: Clean old build cache ===
echo Cleaning previous CMake cache...
rmdir /s /q build >nul 2>&1
mkdir build

:: === Step 4: Configure project with CMake ===
echo Configuring project...
cmake -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE="%cd%\vcpkg\scripts\buildsystems\vcpkg.cmake" ^
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="%cd%\ENGINE" ^
  -B build -S .
if errorlevel 1 (
    echo ❌ CMake configuration failed.
    pause & exit /b 1
)

:: === Step 5: Build project ===
echo Building project...
cmake --build build --config Release
if errorlevel 1 (
    echo ❌ Build failed.
    pause & exit /b 1
)

:: After successful build, launch the executable
set EXE1=%~dp0ENGINE\Release\engine.exe
set EXE2=%~dp0build\Release\engine.exe

echo Launching built game...
if exist "%EXE1%" (
    "%EXE1%"
) else if exist "%EXE2%" (
    "%EXE2%"
) else (
    echo Executable not found at "%EXE1%" or "%EXE2%"
)


pause
endlocal
