@echo off
setlocal enabledelayedexpansion

set "EXTRA_ARGS="
if "%~1"=="-r" (
    echo [INFO] RebuildAssets mode requested.
    set "EXTRA_ARGS=-r"
)

:: === Step 1: Ensure vcpkg exists and is bootstrapped ===
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

:: === Step 2: Confirm critical files exist ===
echo Checking required files and directories...
if not exist "CMakeLists.txt" (
    echo [ERROR] Missing CMakeLists.txt in root directory.
    pause & exit /b 1
)
if not exist "ENGINE\\main.cpp" (
    echo [ERROR] Missing ENGINE\\main.cpp
    pause & exit /b 1
)
if not exist "ENGINE\\engine.cpp" (
    echo [ERROR] Missing ENGINE\\engine.cpp
    pause & exit /b 1
)
if not exist "vcpkg\\scripts\\buildsystems\\vcpkg.cmake" (
    echo [ERROR] Missing toolchain file at vcpkg\\scripts\\buildsystems\\vcpkg.cmake
    pause & exit /b 1
)

:: === Step 2.5: Ensure stb_image_resize2.h is present ===
set "STB_DEST=vcpkg\installed\x64-windows\include\custom"
if not exist "%STB_DEST%\stb_image_resize2.h" (
    echo [INFO] Downloading stb_image_resize2.h...
    mkdir "%STB_DEST%" >nul 2>&1
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12; " ^
        "Invoke-WebRequest https://raw.githubusercontent.com/nothings/stb/master/stb_image_resize2.h -OutFile '%STB_DEST%\stb_image_resize2.h'"
    if errorlevel 1 (
        echo [ERROR] Failed to download stb_image_resize2.h
        pause & exit /b 1
    )
)



:: === Step 3: Install missing dependencies ===
set "VCPKG_TRIPLET=x64-windows"
set "DEPENDENCIES=sdl2 sdl2-image sdl2-mixer sdl2-ttf sdl2-gfx nlohmann-json glad"

echo Checking for missing vcpkg dependencies (triplet: !VCPKG_TRIPLET!)...

for %%D in (!DEPENDENCIES!) do (
    vcpkg\\vcpkg list | findstr /i "%%D:x64-windows" >nul
    if errorlevel 1 (
        echo [INFO] Installing missing package: %%D
        vcpkg\\vcpkg install %%D --triplet !VCPKG_TRIPLET!
        if errorlevel 1 (
            echo [ERROR] Failed to install %%D
            pause & exit /b 1
        )
    ) else (
        echo [OK] %%D already installed.
    )
)
echo ✅ All dependencies verified or installed.

:: === Step 4: Clean build ===
echo Cleaning previous CMake cache...
rmdir /s /q build >nul 2>&1
mkdir build

:: === Step 5: Configure with CMake ===
echo Configuring project...
cmake -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE="%cd%\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake" ^
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="%cd%\\ENGINE" ^
  -B build -S .
if errorlevel 1 (
    echo ❌ CMake configuration failed.
    pause & exit /b 1
)

:: === Step 6: Build ===
echo Building project...
cmake --build build --config Release
if errorlevel 1 (
    echo ❌ Build failed.
    pause & exit /b 1
)

:: === Step 7: Launch executable ===
set EXE1=%~dp0ENGINE\\Release\\engine.exe
set EXE2=%~dp0build\\Release\\engine.exe

echo Launching built game...
if exist "%EXE1%" (
    "%EXE1%" %EXTRA_ARGS%
) else if exist "%EXE2%" (
    "%EXE2%" %EXTRA_ARGS%
) else (
    echo Executable not found at "%EXE1%" or "%EXE2%"
)

pause
endlocal
