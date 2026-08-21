@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "BUILD_ARGS=%*"
if /I "%~1"=="/s" set "BUILD_ARGS=-Silent"
if /I "%~1"=="--silent" set "BUILD_ARGS=-Silent"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%scripts\build-windows.ps1" %BUILD_ARGS%
exit /b %ERRORLEVEL%
