@echo off
setlocal
rem nxrs - native game library and build launcher.
rem The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
rem One-time setup: Python venv with pinned build tools, plus GCC validation.
rem Safe to re-run: installed components are reused.
set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"

call "%~dp0_python.bat"
if errorlevel 1 (
  echo [SETUP] No usable Python found. Creating .venv with the system Python 3...
  py -3 -m venv "%ROOT%\.venv" || (echo [SETUP] FAILED - cannot create .venv& exit /b 1)
  call "%~dp0_python.bat" || (echo [SETUP] FAILED - .venv is not runnable& exit /b 1)
)
echo [SETUP] Python: %SWR_PYTHON%

"%SWR_PYTHON%" -m pip install --quiet --disable-pip-version-check cmake==3.31.10 ninja==1.13.2 backports.zstd
if errorlevel 1 (echo [SETUP] FAILED - could not install pinned build tools& exit /b 1)

if exist "%ROOT%\third_party\gcc\Library\bin\x86_64-w64-mingw32-gcc.exe" (
  echo [SETUP] GCC portable toolchain found: third_party\gcc
) else if exist "%ROOT%\gcc\Library\bin\x86_64-w64-mingw32-gcc.exe" (
  echo [SETUP] GCC portable toolchain found: gcc
) else (
  where gcc >nul 2>nul || (echo [SETUP] FAILED - GCC missing. See docs\BUILD_ENVIRONMENT_GUIDE.md& exit /b 1)
  echo [SETUP] Using gcc from PATH
)

"%SWR_PYTHON%" -c "import cmake, ninja"
if errorlevel 1 (echo [SETUP] FAILED - CMake/Ninja probe& exit /b 1)
echo [SETUP] SUCCESS - Python, CMake, Ninja and GCC are ready
exit /b 0
