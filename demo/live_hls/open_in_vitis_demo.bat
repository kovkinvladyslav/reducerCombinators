@echo off
setlocal
cd /d "%~dp0"
set "DEMO_ROOT=%CD%"
"C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat" --workspace "%DEMO_ROOT%"
endlocal
