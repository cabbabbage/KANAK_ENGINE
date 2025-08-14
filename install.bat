@echo off
setlocal enabledelayedexpansion

echo [INSTALL] Step 1: Installing Git...
where git >nul 2>nul
if errorlevel 1 (
    winget install -e --id Git.Git
    if errorlevel 1 (
        echo [ERROR] Failed to install Git.
        pause & exit /b 1
    )
) else (
    echo [OK] Git found.
)

echo [INSTALL] Step 2: Installing Python...
where python >nul 2>nul
if errorlevel 1 (
    winget install -e --id Python.Python.3
    if errorlevel 1 (
        echo [ERROR] Failed to install Python.
        pause & exit /b 1
    )
) else (
    echo [OK] Python found.
)

echo [INSTALL] Step 3: Cloning KANAK_ENGINE...
git clone https://github.com/cabbabbage/KANAK_ENGINE
if errorlevel 1 (
    echo [ERROR] Failed to clone repository.
    pause & exit /b 1
)

cd KANAK_ENGINE

echo [INSTALL] Checking out kaden_branch...
git fetch origin
git checkout kaden_branch 2>nul
if errorlevel 1 (
    echo [INSTALL] Branch kaden_branch does not exist locally. Creating...
    git checkout -b kaden_branch
)


echo [INSTALL] Step 4: Installing Python requirements...
if exist requirements.txt (
    python -m pip install --upgrade pip
    python -m pip install -r requirements.txt
    if errorlevel 1 (
        echo [ERROR] Failed to install requirements.txt
        pause & exit /b 1
    )
) else (
    echo [WARN] requirements.txt not found.
)

echo [INSTALL] Step 5: Building Kanak_Manager executable...
if not exist "Kanak_Manager.exe" (
    python -m PyInstaller --onefile --windowed Kanak_Manager/main.py
    if errorlevel 1 (
        echo [ERROR] PyInstaller build failed.
        pause & exit /b 1
    )
    if exist "dist\main.exe" (
        move /Y "dist\main.exe" "Kanak_Manager.exe" >nul
        echo [OK] Executable moved to root as Kanak_Manager.exe
    ) else (
        echo [ERROR] PyInstaller did not produce dist\main.exe
        pause & exit /b 1
    )
) else (
    echo [OK] Kanak_Manager.exe already exists.
)

echo [INSTALL] Step 6: Checking vcpkg setup...
if not exist "vcpkg" (
    git clone https://github.com/microsoft/vcpkg.git
    if errorlevel 1 (
        echo [ERROR] Failed to clone vcpkg.
        pause & exit /b 1
    )
)
if not exist "vcpkg\vcpkg.exe" (
    pushd vcpkg
    call bootstrap-vcpkg.bat
    popd
    if not exist "vcpkg\vcpkg.exe" (
        echo [ERROR] vcpkg bootstrap failed.
        pause & exit /b 1
    )
)

echo [INSTALL] Step 7: Ensuring stb_image_resize2.h is present...
set "STB_DEST=vcpkg\installed\x64-windows\include\custom"
if not exist "%STB_DEST%\stb_image_resize2.h" (
    mkdir "%STB_DEST%" >nul 2>&1
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
        "[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12; " ^
        "Invoke-WebRequest https://raw.githubusercontent.com/nothings/stb/master/stb_image_resize2.h -OutFile '%STB_DEST%\stb_image_resize2.h'"
    if errorlevel 1 (
        echo [ERROR] Failed to download stb_image_resize2.h
        pause & exit /b 1
    )
)

echo [INSTALL] Step 8: Cleaning previous build...
rmdir /s /q build >nul 2>&1
mkdir build

echo [INSTALL] Step 9: Configuring C++ project...
cmake -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE="%cd%\\vcpkg\\scripts\\buildsystems\\vcpkg.cmake" ^
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="%cd%\\ENGINE" ^
  -B build -S .
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    pause & exit /b 1
)

echo [INSTALL] Step 10: Building C++ executable...
cmake --build build --config Release
if errorlevel 1 (
    echo [ERROR] Build failed.
    pause & exit /b 1
)


:: Move Run_Game.bat and Run_Asset_Manager.bat to the directory this script was started from
echo [INSTALL] Moving Run_Game.bat and Run_Asset_Manager.bat to original script location...
set "SCRIPT_DIR=%~dp0"

if exist "Run_Game.bat" (
    move /Y "Run_Game.bat" "%SCRIPT_DIR%" >nul
)
if exist "Run_Asset_Manager.bat" (
    move /Y "Run_Asset_Manager.bat" "%SCRIPT_DIR%" >nul
)

echo [INSTALL] Batch files moved to %SCRIPT_DIR%


echo [INSTALL] Step 11: Running C++ executable with -r...
set EXE1=%cd%\\ENGINE\\Release\\engine.exe
set EXE2=%cd%\\build\\Release\\engine.exe
if exist "%EXE1%" (
    "%EXE1%" -r
) else if exist "%EXE2%" (
    "%EXE2%" -r
) else (
    echo [ERROR] Executable not found.
    pause & exit /b 1
)

pause
endlocal
