@echo off
REM Runs C simulation, HLS synthesis and C/RTL co-simulation for SUM_REC_SCALED.
pushd "%~dp0"
C:\AMDDesignTools\2025.2\Vitis\bin\vitis.bat -s tools\run_sum_rec_scaled_hls_flow.py
set EXIT_CODE=%ERRORLEVEL%
popd
exit /b %EXIT_CODE%
