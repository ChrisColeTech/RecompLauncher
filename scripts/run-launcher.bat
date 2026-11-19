@echo off
setlocal
rem nxrs - native game library and build launcher.
rem The vendored Switch2RECOMP engine keeps its AGPL-3.0 notices under third_party/Switch2RECOMP.
rem Run the deployed Qt launcher from a developer checkout.
set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "LAUNCHER=%ROOT%\bin\launcher\nxrs.exe"
if not exist "%LAUNCHER%" (
  echo [RUN] Launcher not found: "%LAUNCHER%"
  echo Build it first with: scripts\build-launcher.bat
  exit /b 1
)
start "" "%LAUNCHER%" %*
exit /b 0
