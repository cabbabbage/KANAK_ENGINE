@echo off
setlocal EnableDelayedExpansion

set "ROOT=C:\Users\cal_m\OneDrive\Documents\GitHub\tarot_game"
set "OUTPUT=project_structure.txt"
set /a MAX_DEPTH=3
set /a FILE_LIMIT=15

echo 📁 Project structure for: %ROOT% > "%OUTPUT%"
echo. >> "%OUTPUT%"

call :scan "%ROOT%" 0 >> "%OUTPUT%"

echo. >> "%OUTPUT%"
echo Done. Output saved to %OUTPUT%
pause
exit /b

:scan
set "FOLDER=%~1"
set /a DEPTH=%2

REM Skip ignored folders
for %%X in (__pycache__ vcpkg .git .vs build) do (
    echo %FOLDER% | findstr /I /C:\"%%X\" >nul && goto :eof
)

REM Stop if too deep
if %DEPTH% GEQ %MAX_DEPTH% (
    set "indent="
    for /L %%N in (1,1,%DEPTH%) do set "indent=!indent!    "
    echo !indent!... >> "%OUTPUT%"
    goto :eof
)

pushd "%FOLDER%" >nul 2>&1 || goto :eof

set "indent="
for /L %%N in (1,1,%DEPTH%) do set "indent=!indent!    "

set /a count=0
for %%F in (*.cpp *.h *.hpp *.json) do set /a count+=1

if !count! GTR %FILE_LIMIT% (
    echo !indent!... (!count! files) >> "%OUTPUT%"
) else (
    for %%F in (*.cpp *.h *.hpp *.json) do (
        echo !indent!%%F >> "%OUTPUT%"
    )
)

for /D %%D in (*) do (
    echo !indent!%%D >> "%OUTPUT%"
    call :scan "%%D" !DEPTH!+1
)

popd
exit /b
