@echo off
setlocal

:: Get the directory where this batch file is located
set "SCRIPT_DIR=%~dp0"

:: Path to the cloned repo root
set "REPO_DIR=%SCRIPT_DIR%KANAK_ENGINE"

:: Path to the game executable
set "GAME_EXE=%REPO_DIR%\ENGINE\Release\engine.exe"

if not exist "%GAME_EXE%" (
    echo [RUN_GAME] ERROR: Game executable not found at:
    echo   %GAME_EXE%
    pause
    exit /b 1
)

echo [RUN_GAME] Changing directory to %REPO_DIR%
cd /d "%REPO_DIR%"

echo [RUN_GAME] Launching game...
"%GAME_EXE%" %*

endlocal
