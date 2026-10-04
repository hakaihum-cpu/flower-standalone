@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "FLP=%~1"
if not defined FLP (
  echo Usage: AUTO_SAMPLE.bat "C:\path\to\YourInstrument.flp"
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

if not exist "%~dp0MASTER_CAPTURE.layout.json" (
  %PY% "%~dp0generate_master_capture_midi.py" -m "%~dp0capture_manifest.csv" -o "%~dp0MASTER_CAPTURE.mid" --layout "%~dp0MASTER_CAPTURE.layout.json"
  if errorlevel 1 (
    echo [ERROR] MASTER_CAPTURE layout generation failed.
    pause
    exit /b 1
  )
)

if not defined FL_EXE (
  if exist "C:\Program Files\Image-Line\FL Studio 2025\FL64.exe" set "FL_EXE=C:\Program Files\Image-Line\FL Studio 2025\FL64.exe"
)
if not defined FL_EXE (
  for /d %%D in ("C:\Program Files\Image-Line\FL Studio*") do (
    if exist "%%~fD\FL64.exe" set "FL_EXE=%%~fD\FL64.exe"
  )
)
if not defined FL_EXE (
  echo [ERROR] FL64.exe was not found.
  echo Set FL_EXE to the full path and run this BAT again.
  pause
  exit /b 2
)

tasklist /FI "IMAGENAME eq FL64.exe" 2>nul | find /I "FL64.exe" >nul
if not errorlevel 1 (
  echo [ERROR] Close FL Studio before starting AUTO_SAMPLE.
  echo This avoids a running instance intercepting the command-line render.
  pause
  exit /b 2
)

for %%I in ("%FLP%") do (
  set "NAME=%%~nI"
  set "PARENT=%%~dpI"
)

set "OUT=%PARENT%%NAME%_AUTO_SAMPLE"
set "CAP=%OUT%\capture_flac"
set "MASTER=%OUT%\%NAME%.flac"
set "BANK=%OUT%\%NAME%_epbank_24bit_dedup.bin"

if not exist "%OUT%" mkdir "%OUT%"
if not exist "%CAP%" mkdir "%CAP%"
if exist "%MASTER%" del /q "%MASTER%"

echo.
echo [1/3] Rendering FL Studio project to FLAC...
echo FL Studio: %FL_EXE%
echo Project  : %FLP%
echo Output   : %OUT%
echo.
start "" /wait "%FL_EXE%" /R /Eflac /O"%OUT%" "%FLP%"

if not exist "%MASTER%" (
  echo [ERROR] Expected render was not created:
  echo %MASTER%
  echo FL Studio command-line rendering uses the project name for the output file.
  pause
  exit /b 1
)

echo.
echo [2/3] Splitting MASTER render into 24 capture files...
%PY% "%~dp0split_master_render.py" "%MASTER%" -l "%~dp0MASTER_CAPTURE.layout.json" -o "%CAP%"
if errorlevel 1 (
  echo [ERROR] Split/validation failed.
  echo The MASTER render must be 48000 Hz, stereo, 24-bit FLAC.
  pause
  exit /b 1
)

echo.
echo [3/3] Building EPBANK1...
%PY% "%~dp0build_bank_flac_dedup.py" "%CAP%" -m "%~dp0capture_manifest.csv" -o "%BANK%"
if errorlevel 1 (
  echo [ERROR] Bank build failed.
  pause
  exit /b 1
)

echo.
echo SHA-256:
certutil -hashfile "%BANK%" SHA256 | findstr /v /c:"hash of file" /c:"CertUtil"
echo.
echo DONE
echo %BANK%
pause
