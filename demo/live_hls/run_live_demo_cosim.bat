@echo off
setlocal
cd /d "%~dp0"
set "DEMO_ROOT=%CD%"
echo Close any opened waveform for this live demo before running C/RTL cosimulation.
"C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat" --mode hls --tcl "%DEMO_ROOT%\run_live_demo_cosim.tcl" --work_dir "%DEMO_ROOT%"
endlocal
