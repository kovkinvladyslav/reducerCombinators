@echo off
setlocal
cd /d "%~dp0"
"C:\AMDDesignTools\2025.2\Vivado\bin\vivado.bat" -source "%~dp0open_prepared_network_schematic.tcl"
endlocal
