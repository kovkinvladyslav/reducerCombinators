# Live HLS reducer demo

This directory contains a compact AMD Vitis HLS demo for the diploma reducer.
It demonstrates a prepared active-pair input table, a 4-channel reduction step, C simulation output, C synthesis, C/RTL co-simulation, waveform inspection, and RTL schematic inspection.

Main files:

- `src/prepared_network_format.hpp` - input table row format.
- `src/parallel_reducer_live_demo_top.hpp` - top-level HLS interface.
- `src/parallel_reducer_live_demo_top.cpp` - live demo reducer logic.
- `test/live_demo_test.cpp` - test bench that provides the prepared network and checks the result.
- `hls_live_demo_config.cfg` - Vitis HLS component configuration.

Run order:

1. `run_live_demo_csim.bat` - C simulation and readable execution journal.
2. `run_live_demo_csynth.bat` - HLS synthesis and generated RTL.
3. `run_live_demo_cosim.bat` - C/RTL co-simulation with waveform data.
4. `open_live_channel_waveform.bat` - opens the waveform view in Vivado.
5. `open_prepared_network_schematic.bat` - opens the elaborated RTL schematic in Vivado.

The demo computes the sum 0 + 1 + ... + 15 = 120 using prepared active pairs distributed across four reduction channels.
