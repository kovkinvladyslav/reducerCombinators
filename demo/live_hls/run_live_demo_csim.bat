@echo off
setlocal
cd /d "%~dp0"
set "DEMO_ROOT=%CD%"
"C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat" --mode hls --tcl "%DEMO_ROOT%\run_live_demo_csim.tcl" --work_dir "%DEMO_ROOT%"
endlocal
