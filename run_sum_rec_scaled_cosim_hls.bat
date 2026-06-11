@echo off
REM This creates/updates the Vitis HLS component for SUM_REC_SCALED C/RTL simulation.
REM It intentionally uses hls_config_sum_rec_scaled_cosim.cfg, not the general hls_config.cfg.
pushd "%~dp0"
call C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat --mode hls --config hls_config_sum_rec_scaled_cosim.cfg --work_dir sum_rec_scaled_cosim --csim
if errorlevel 1 goto :failed

call C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat -c --mode hls --config hls_config_sum_rec_scaled_cosim.cfg --work_dir sum_rec_scaled_cosim
if errorlevel 1 goto :failed

call C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat --mode hls --config hls_config_sum_rec_scaled_cosim.cfg --work_dir sum_rec_scaled_cosim --cosim
if errorlevel 1 goto :failed

popd
exit /b 0

:failed
popd
exit /b 1
