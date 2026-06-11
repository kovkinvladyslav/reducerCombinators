@echo off
REM This runs the SUM_TREE_SCALED HLS C-simulation and C/RTL co-simulation.
pushd "%~dp0"
call C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat --mode hls --config hls_config_sum_tree_scaled_cosim.cfg --work_dir sum_tree_scaled_cosim --csim
if errorlevel 1 goto :failed

call C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat -c --mode hls --config hls_config_sum_tree_scaled_cosim.cfg --work_dir sum_tree_scaled_cosim
if errorlevel 1 goto :failed

call C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat --mode hls --config hls_config_sum_tree_scaled_cosim.cfg --work_dir sum_tree_scaled_cosim --cosim
if errorlevel 1 goto :failed

popd
exit /b 0

:failed
popd
exit /b 1
