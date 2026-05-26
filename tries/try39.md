# Try 39: Staged LINK Read-Decode Bit

Current status: `failed_csynth_resources`

Branch: `try39`

Architecture: preserve the try38 compact produced buffer and direct staged
`LINK` append path, then cache the unbound-marker decision from the staged
`LINK` substitution read so the writeback stage no longer compares the full
previous port against the unbound marker.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try38 completed at `3749 LUT`, `1148 FF`, `II=1`, slack `-1.97`; its critical path is `substitution_memory_0_load -> selected_command_link_previous_port -> unbound-marker comparison -> local_status_code phi/store`. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_LINK_READ_DECODE`, cached `selected_command_link_previous_is_unbound`, and a focused read-decode synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, focused top syntax, focused testbench syntax, full CSIM (`a39`), and FPGA-small CSIM (`b39`) passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed with scheduler `II=1`, `3751 LUT`, `1149 FF`, `0 DSP`, `0 BRAM`, and estimated Fmax `107.48 MHz`; timing/resource result is slightly worse than try38. |

## Goal

Reduce the selected-command staged `LINK` read/compare/status dependency in the
focused parallel FPGA-small profile while preserving default full-profile and
FPGA-small behavior.

## Hypothesis

The try38 critical path still carries the previous substitution port through a
full-width equality comparison in the staged `LINK` writeback tick. If the
unbound-marker decision is captured as a one-bit state value during the
substitution read stage, the writeback stage can select between binding and
emitting a pair using that bit instead of re-comparing the full port. This may
reduce LUT depth and improve timing without changing `LINK` semantics.

## Difference From Prior Tries

Try37 reduced selected-command produced-pair state width. Try38 specialized the
selected-command staged `LINK` append path. Try39 keeps both as the base and
targets the next reported limiter: the previous-port unbound comparison itself.

This is not another produced-buffer experiment, not another append
specialization, and not a service-phase split. It changes only where the
unbound-marker predicate is computed and stored.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_LINK_READ_DECODE`
  - default `0`
- Add one selected-command staged `LINK` state bit:
  - `selected_command_link_previous_is_unbound`
- When the selected-command staged `LINK` read stage loads
  `substitution_memory[selected_command_link_variable_index]`, also compute and
  store whether the previous port equals `make_port(PORT_TAG_REFERENCE,
  INVALID_PORT_VALUE)`.
- In the split writeback stage, use the cached bit when the gate is enabled.
- Preserve the existing full-width comparison when the gate is disabled.
- Reset the cached bit when selected-command state is cleared and when a staged
  `LINK` transaction finishes.
- Add a focused synthesis config that enables try38 gates plus
  `ENABLE_PARALLEL_LINK_READ_DECODE=1`.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try39 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_READ_DECODE=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a39
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b39
```

Focused read-decode CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_singlepop_compact_produced_direct_link_read_decode_nostats.cfg --work_dir csyn_t39_parallel_link_read_decode
```

## Results

- Host syntax: PASS
- Try39 focused syntax: PASS for both `test/reducer_test.cpp` and direct
  `src/reducer_core_parallel.cpp` top-file syntax.
- Host execution: focused all-test executable is not applicable because the
  focused synthesis flags deliberately disable `COMMUTE`, `OPERATE`, `SWITCH`,
  and `CALL` while the full testbench still expects them.
- Full CSIM: PASS (`a39`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS (`b39`, `Overall Result: PASS`)
- CSYNTH: completed after one initial top-only syntax failure was fixed; final
  focused synthesis used `csyn_t39_parallel_link_read_decode_rerun`, kept
  scheduler `II=1`, but slightly worsened area/timing relative to try38.

## HLS Evidence

- Work directory: `csyn_t39_parallel_link_read_decode_rerun`
- Report paths:
  - `csyn_t39_parallel_link_read_decode_rerun/hls/syn/report/csynth.rpt`
  - `csyn_t39_parallel_link_read_decode_rerun/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t39_parallel_link_read_decode_rerun/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_902_3_csynth.rpt`
  - `csyn_t39_parallel_link_read_decode_rerun/hls/syn/report/csynth_design_size.rpt`
- LUT: `3751` top-level total, `3593` in the scheduler pipeline
- FF: `1149` top-level total, `1078` in the scheduler pipeline
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_902_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`
- Timing/slack: estimated clock period `9.304 ns`, estimated Fmax
  `107.48 MHz`. With the configured `2.70 ns` uncertainty, this remains about
  `-2.00 ns` against the effective `7.30 ns` budget.
- Critical path evidence: Vitis now reports
  `substitution_memory_0_load -> previous_port -> selected_command_link_previous_is_unbound -> selected_command_link_previous_is_unbound_load -> local_status_code phi/store`.
  The cached bit did not remove the dependency; it became an explicit state
  element in the same loop-carried path.
- Design-size evidence: final HW-transform design size is `576` instructions,
  slightly above try38's `574`.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try39 is a negative result. It preserves semantics and keeps `II=1`, but the
one-bit cached predicate does not improve the focused parallel scheduler. The
resource and timing numbers are slightly worse than try38:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try37 compact produced buffer | 3998 | 1148 | 0 | 0 | 1 | `-2.00` | `107.48 MHz` |
| Try38 direct staged `LINK` append | 3749 | 1148 | 0 | 0 | 1 | `-1.97` | `107.85 MHz` |
| Try39 staged `LINK` read-decode bit | 3751 | 1149 | 0 | 0 | 1 | about `-2.00` | `107.48 MHz` |

The FPGA lesson is that moving the unbound comparison into a cached state bit
is not enough. HLS still schedules the substitution read, predicate production,
predicate consumption, and `local_status_code` selection in one loop-carried
region. The issue is no longer the width of the equality comparison; it is the
selected-command controller's dependence on substitution-memory read results
while also selecting status/service control.

## Next Recommendation

Return to try38 as the better focused base. The next try should avoid adding a
read-decode bit and instead remove the success path from `local_status_code`
selection for the selected-command staged `LINK` service:

- keep compact produced buffer and direct staged `LINK` append;
- do not cache the unbound predicate as a separate state bit;
- split selected-command staged `LINK` writeback so success never writes
  `local_status_code`, or isolate only error-status writes behind an explicit
  error-valid bit;
- keep the experiment focused on `LINK` service/status routing, not on generic
  completion service from try36.
