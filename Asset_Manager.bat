@echo off
setlocal

:: Get the directory where this batch file is located
set "SCRIPT_DIR=%~dp0"

:: Path to the cloned repo root
set "REPO_DIR=%SCRIPT_DIR%KANAK_ENGINE"

:: Path to the Python script
set "PY_SCRIPT=%REPO_DIR%\Kanak_Manager\main.py"

if not exist "%PY_SCRIPT%" (
    echo [RUN_ASSET_MANAGER] ERROR: Python script not found at:
    echo   %PY_SCRIPT%
    pause
    exit /b 1
)

echo [RUN_ASSET_MANAGER] Changing directory to %REPO_DIR%
cd /d "%REPO_DIR%"

echo [RUN_ASSET_MANAGER] Launching Asset Manager...
python "%PY_SCRIPT%" %*

endlocal
