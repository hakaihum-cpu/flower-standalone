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

%PY% "%~dp0generate_master_capture_midi.py" -m "%~dp0capture_manifest.csv" -o "%~dp0MASTER_CAPTURE.mid" --layout "%~dp0MASTER_CAPTURE.layout.json"
if errorlevel 1 (
  echo [ERROR] MASTER_CAPTURE generation failed.
  pause
  exit /b 1
)

echo.
echo MASTER_CAPTURE.mid is ready.
echo Import it ONCE into the target instrument channel in FL Studio.
echo Put the resulting pattern at time 0 in the Playlist, set SONG mode,
echo then save that project as your reusable SAMPLING_MASTER.flp.
pause
