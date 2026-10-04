@echo off
setlocal
cd /d "%~dp0"

where py >nul 2>nul
if not errorlevel 1 (
  set "PY=py -3"
) else (
  where python >nul 2>nul
  if errorlevel 1 (
    echo [ERROR] Python 3 was not found.
    pause
    exit /b 1
  )
  set "PY=python"
)

%PY% "%~dp0generate_fast_capture_midi.py" -o "%~dp0FAST_CAPTURE.mid" --layout "%~dp0FAST_CAPTURE.layout.json"
if errorlevel 1 (
  echo [ERROR] FAST_CAPTURE generation failed.
  pause
  exit /b 1
)

echo.
echo FAST_CAPTURE.mid is ready.
echo Import it ONCE into the target instrument channel in FL Studio.
echo Put the pattern at time 0 in the Playlist and save the reusable FLP in SONG mode.
echo.
echo Capture specification:
echo   Range: C2-C8
echo   Source notes: every 3 semitones ^(25 notes^)
echo   Velocity: 16 / 48 / 80 / 127
echo   RR: 1 capture only
echo   Duration: about 7 min 05 sec
pause
