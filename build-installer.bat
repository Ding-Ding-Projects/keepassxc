@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "BUILD_ARGS=%*"
if /I "%~1"=="/s" set "BUILD_ARGS=-Silent %~2 %~3 %~4 %~5 %~6 %~7 %~8 %~9"
if /I "%~1"=="--silent" set "BUILD_ARGS=-Silent %~2 %~3 %~4 %~5 %~6 %~7 %~8 %~9"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%scripts\build-squirrel.ps1" %BUILD_ARGS%
exit /b %ERRORLEVEL%
