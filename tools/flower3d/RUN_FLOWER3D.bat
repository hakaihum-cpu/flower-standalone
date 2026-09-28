@echo off
setlocal
cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_student01_windows.ps1" -Out "%~dp0output"

if errorlevel 1 (
  echo.
  echo FLOWER 3D render FAILED.
  pause
  exit /b 1
)

echo.
echo FLOWER 3D render completed.
echo Output: %~dp0output
pause
