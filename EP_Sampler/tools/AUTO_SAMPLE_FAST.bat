@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "FLP=%~1"
if not defined FLP (
  echo Drag a prepared .flp onto this BAT, or run:
  echo AUTO_SAMPLE_FAST.bat "C:\path\to\Instrument.flp"
  pause
  exit /b 2
)
if not exist "%FLP%" (
  echo [ERROR] FLP not found: %FLP%
  pause
  exit /b 2
)

where ffmpeg >nul 2>nul
if errorlevel 1 (
  echo [ERROR] ffmpeg is not in PATH.
  pause
  exit /b 2
)
where ffprobe >nul 2>nul
if errorlevel 1 (
  echo [ERROR] ffprobe is not in PATH.
  pause
  exit /b 2
)
where py >nul 2>nul
if not errorlevel 1 (
  set "PY=py -3"
) else (
  where python >nul 2>nul
  if errorlevel 1 (
    echo [ERROR] Python 3 was not found.
    pause
    exit /b 2
  )
  set "PY=python"
)

if not exist "%~dp0FAST_CAPTURE.layout.json" (
  %PY% "%~dp0generate_fast_capture_midi.py" -o "%~dp0FAST_CAPTURE.mid" --layout "%~dp0FAST_CAPTURE.layout.json"
  if errorlevel 1 (
    echo [ERROR] FAST_CAPTURE layout generation failed.
    pause
    exit /b 1
  )
)

if not defined FL_EXE (
  if exist "C:\Program Files\Image-Line\FL Studio 2025\FL64.exe" (
    set "FL_EXE=C:\Program Files\Image-Line\FL Studio 2025\FL64.exe"
  )
)
if not defined FL_EXE (
  echo [ERROR] Expected FL Studio executable was not found:
  echo C:\Program Files\Image-Line\FL Studio 2025\FL64.exe
  echo.
  echo If FL Studio moves later, set FL_EXE manually in this BAT.
  pause
  exit /b 2
)

tasklist /FI "IMAGENAME eq FL64.exe" 2>nul | find /I "FL64.exe" >nul
if not errorlevel 1 (
  echo [ERROR] Close FL Studio before AUTO_SAMPLE_FAST.
  pause
  exit /b 2
)
tasklist /FI "IMAGENAME eq FL.exe" 2>nul | find /I "FL.exe" >nul
if not errorlevel 1 (
  echo [ERROR] Close FL Studio before AUTO_SAMPLE_FAST.
  pause
  exit /b 2
)

for %%I in ("%FLP%") do (
  set "NAME=%%~nI"
  set "PARENT=%%~dpI"
)
set "OUT=%PARENT%%NAME%_AUTO_SAMPLE_FAST"
set "MASTER_BASE=%OUT%\%NAME%_FAST_MASTER"
set "MASTER=%MASTER_BASE%.flac"
set "BANK=%OUT%\%NAME%_fast_epbank.bin"

if not exist "%OUT%" mkdir "%OUT%"
if exist "%MASTER%" del /q "%MASTER%"
if exist "%BANK%" del /q "%BANK%"

echo.
echo [1/2] FL Studio offline render...
echo Project: %FLP%
echo Target : %MASTER%
echo.
start "" /wait "%FL_EXE%" /R"%MASTER_BASE%" /Eflac "%FLP%"

if not exist "%MASTER%" (
  echo [ERROR] FL Studio did not create the expected FLAC:
  echo %MASTER%
  pause
  exit /b 1
)

for /f %%D in ('ffprobe -v error -show_entries format^=duration -of default^=noprint_wrappers^=1:nokey^=1 "%MASTER%"') do set "DURATION=%%D"
%PY% -c "import sys; d=float(sys.argv[1]); sys.exit(0 if d >= 423.0 else 1)" "%DURATION%"
if errorlevel 1 (
  echo [ERROR] FL Studio render is too short: %DURATION% sec
  echo Expected about 425 sec.
  echo.
  echo Open the FLP in FL Studio and confirm:
  echo   1. SONG mode is selected, not PATTERN.
  echo   2. FAST_CAPTURE pattern is placed in the Playlist at bar 1.
  echo   3. The Playlist extends to about 7 min 05 sec.
  echo   4. Pressing Play in SONG mode actually plays the VST sequence.
  echo Save the FLP, close FL Studio, then run this BAT again.
  pause
  exit /b 1
)

echo.
echo Render duration: %DURATION% sec
echo [2/2] Building C2-C8 EPBANK1...
%PY% "%~dp0build_fast_bank.py" "%MASTER%" -l "%~dp0FAST_CAPTURE.layout.json" -o "%BANK%"
if errorlevel 1 (
  echo [ERROR] Bank build failed.
  echo Confirm the render is stereo / 48000 Hz / FLAC 16-bit or 24-bit.
  pause
  exit /b 1
)

echo.
echo DONE
echo %BANK%
pause
