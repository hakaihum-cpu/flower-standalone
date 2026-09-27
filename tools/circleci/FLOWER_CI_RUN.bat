@echo off
setlocal EnableExtensions
set "PS1=%~dp0flower_ci.ps1"

if not exist "%PS1%" (
  echo [ERROR] flower_ci.ps1 was not found next to this BAT.
  pause
  exit /b 2
)

if /I "%~1"=="/setup" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PS1%" -Setup
) else if "%~1"=="" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PS1%"
) else (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PS1%" -Branch "%~1"
)

set "RC=%ERRORLEVEL%"
echo.
if "%RC%"=="0" (
  echo FLOWER CI completed successfully.
) else (
  echo FLOWER CI failed. Exit code: %RC%
)
pause
exit /b %RC%
