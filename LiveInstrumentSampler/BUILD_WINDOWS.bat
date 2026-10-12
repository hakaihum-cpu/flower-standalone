@echo off
setlocal EnableExtensions
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] CMake is not in PATH.
  pause
  exit /b 1
)

cmake -S . -B build-win -A x64
if errorlevel 1 (
  echo [ERROR] CMake configure failed.
  pause
  exit /b 1
)

cmake --build build-win --config Release --target EP_Live_Instrument_Sampler LiveInstrumentSamplerCoreTests
if errorlevel 1 (
  echo [ERROR] Windows build failed.
  pause
  exit /b 1
)

ctest --test-dir build-win -C Release --output-on-failure
if errorlevel 1 (
  echo [ERROR] Core tests failed.
  pause
  exit /b 1
)

echo.
echo Build + Core tests completed.
echo EXE is under build-win\EP_Live_Instrument_Sampler_artefacts\Release\
echo IMPORTANT: ASIO is not verified until RUN ASIO CHECK passes on the target interface.
pause
