@echo off
setlocal
title ODrive Firmware Build
pushd "%~dp0"
if errorlevel 1 (
    echo Failed to open the project directory.
    pause
    exit /b 1
)

powershell.exe -NoLogo -NoProfile -File "%~dp0build.ps1" %*
set "BUILD_EXIT_CODE=%ERRORLEVEL%"
echo.
if "%BUILD_EXIT_CODE%"=="0" (
    echo Build completed successfully.
    echo Output: "%~dp0Firmware\build"
) else (
    echo Build failed. Exit code: %BUILD_EXIT_CODE%
    echo Log, if compilation started: "%~dp0Firmware\build\build.log"
)
popd
echo.
pause
exit /b %BUILD_EXIT_CODE%
