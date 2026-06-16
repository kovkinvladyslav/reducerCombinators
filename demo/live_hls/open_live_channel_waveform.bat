@echo off
setlocal
cd /d "%~dp0"
"C:\AMDDesignTools\2025.2\Vivado\bin\vivado.bat" -source "%~dp0open_live_channel_waveform.tcl"
endlocal
