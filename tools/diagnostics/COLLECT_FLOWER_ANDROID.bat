@echo off
setlocal EnableExtensions

where adb >nul 2>nul
if errorlevel 1 (
  echo [ERROR] adb was not found in PATH.
  echo Install Android platform-tools or add adb.exe to PATH.
  pause
  exit /b 1
)

adb wait-for-device >nul 2>nul
if errorlevel 1 (
  echo [ERROR] No Android device is available through adb.
  pause
  exit /b 1
)

for /f %%I in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd_HHmmss"') do set TS=%%I

set "OUT=%~dp0logs\%TS%"
mkdir "%OUT%" >nul 2>nul

echo [FLOWER] Device:
adb devices -l

echo [FLOWER] Clearing logcat before reproduction...
adb logcat -c

echo.
echo Launch FLOWER on the Android device and reproduce the problem.
echo Leave this window open.
echo When the problem has happened, return here and press any key.
pause >nul

echo [FLOWER] Collecting diagnostics to:
echo %OUT%

adb logcat -d -v threadtime > "%OUT%\logcat-all.txt"
adb logcat -b crash -d -v threadtime > "%OUT%\logcat-crash.txt"
adb shell dumpsys activity exit-info local.flower.standalone > "%OUT%\exit-info.txt"
adb shell dumpsys activity activities > "%OUT%\activities.txt"
adb shell getprop > "%OUT%\device-properties.txt"

findstr /I /C:"FLOWER_DIAG" /C:"AndroidRuntime" /C:"Fatal signal" /C:"F DEBUG" /C:"libc" /C:"ANR in local.flower.standalone" "%OUT%\logcat-all.txt" > "%OUT%\flower-focus.txt"

echo.
echo [FLOWER] Done.
echo Main files:
echo   %OUT%\flower-focus.txt
echo   %OUT%\logcat-crash.txt
echo   %OUT%\exit-info.txt
echo   %OUT%\logcat-all.txt
echo.
pause
