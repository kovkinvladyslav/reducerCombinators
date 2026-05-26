# Try 54: Focused Success-Only Scheduler Status

Current status: `failed_csynth_resources`

Branch: `try54`

Architecture: keep the try53 ERASE-only request/response service, but add a
focused synthesis profile that removes selected-command error/status writes
from the measured hot scheduler path for valid benchmark graphs.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try53 restored scheduler `II=1` and improved LUT to `3549`, but timing was dominated by `while.cond` and the shared `local_status_code` phi/store. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_ASSUME_VALID_SELECTED_COMMANDS`, removed selected-command status writes and the hot-loop status condition in the focused profile, and added `hls_config_try54_success_status_nostats.cfg`. |
| 2026-05-26 | `csim_passed` | Default syntax, focused syntax, full-profile CSIM, and FPGA-small CSIM passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed at `3302 LUT`, `1164 FF`, `0 DSP`, `0 BRAM`, scheduler `II=1`, and `121.44 MHz`; this is a positive measurement but still misses the effective clock budget. |

## Goal

Measure whether the global scheduler status/control surface is now the dominant
timing object by removing status production from the focused selected-command
success path.

## Hypothesis

Try49 through try53 show that selected `ERASE` datapath reshaping is no longer
enough. Try53's critical path is:

```text
while.cond
executed_steps / maximum_steps loop condition
local_status_code phi/mux
local_status_code_write_ln1060
```

If the measured profile assumes valid selected-command benchmark graphs and
does not write `local_status_code` from selected-command rule services, HLS may
remove or shrink the status phi from the pipelined scheduler loop. This is not
a replacement for full semantics; it is a diploma measurement profile to prove
whether status/error reporting must become a separate hardware service.

## Difference From Prior Tries

This is not another selected `ERASE` staging try. Try54 keeps try53's
request/read/emit structure.

This is also not try50 repeated. Try50 moved one invalid selected-ERASE status
into a pending-status register but still allowed the hot loop to produce the
same shared `local_status_code` phi. Try54 deliberately creates a focused
valid-graph profile where selected-command status writes are removed from the
measured path.

Default full-profile and FPGA-small profile behavior remain unchanged because
the new gate defaults to `0` and is enabled only by the try54 focused config.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_ASSUME_VALID_SELECTED_COMMANDS`
  - default `0`
- In the selected-command focused path, when the gate is enabled:
  - avoid writing `local_status_code` for unsupported selected-command cases;
  - avoid selected-ERASE service invalid-address status writes;
  - keep successful `VOID`, `LINK`, `ERASE`, and existing selected operations
    unchanged for valid graphs.
- Keep normal error/status behavior in all existing configs.
- Add focused config:
  - `hls_config_try54_success_status_nostats.cfg`
- Compare against try53:
  - try53: `3549 LUT`, `1260 FF`, `II=1`, `116.18 MHz`.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try54 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT=1 -DENABLE_PARALLEL_TRUE_CLASS_ONLY_SELECTED=1 -DENABLE_PARALLEL_DIRECT_SELECTED_ERASE=1 -DENABLE_PARALLEL_SELECTED_ERASE_SERVICE=1 -DENABLE_PARALLEL_ASSUME_VALID_SELECTED_COMMANDS=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a54
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b54
```

Focused CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_try54_success_status_nostats.cfg --work_dir csyn_t54_parallel_success_status
```

## Results

- Host syntax: passed for default testbench, focused top, and focused testbench syntax.
- Host execution: not run separately; covered by Vitis CSIM testbench.
- Full CSIM: passed with `hls_config.cfg` in work directory `a54`.
- FPGA-small CSIM: passed with `hls_config_fpga_small.cfg` in work directory `b54`.
- CSYNTH: completed in `csyn_t54_parallel_success_status`; improved area and Fmax versus try53, but still did not meet the effective timing budget.

## HLS Evidence

- Work directory: `csyn_t54_parallel_success_status`
- Report paths:
  - `csyn_t54_parallel_success_status/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t54_parallel_success_status/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_1202_3_csynth.rpt`
  - `csyn_t54_parallel_success_status/hls/syn/report/csynth_design_size.rpt`
- LUT: `3302` top-level total, `3076` in the scheduler pipeline.
- FF: `1164` top-level total, `1093` in the scheduler pipeline.
- DSP: `0`.
- BRAM: `0`.
- II: scheduler loop `VITIS_LOOP_1202_3` target `II=1`, achieved `II=1`, iteration latency `2`.
- Timing/slack: estimated clock `8.235 ns`, uncertainty `2.70 ns`, effective budget `7.30 ns`, estimated Fmax `121.44 MHz`; approximately `-0.935 ns` against the effective budget.
- Design size: `19,360` compile/link instructions and `591` HW-transform instructions.
- Last Vitis log line if frozen: not applicable; synthesis completed.

Critical path evidence:

```text
active_pair_queue local queue read/update
candidate_active_pair.left_port sparsemux/select
selected_command_active_pair.left_port
selected_command_lane_index_write_ln1086
```

Relevant Vitis log lines:

```text
Cannot meet target clock period from basic block while.cond to
selected_command_lane_index_write_ln1086 ... combination delay: 8.92086 ns

Pipelining result : Target II = 1, Final II = 1, Depth = 3, loop
VITIS_LOOP_1202_3

Loop Constraint Status: All loop constraints were satisfied.
Estimated Fmax: 121.44 MHz
```

## Conclusion

Try54 is a positive measurement but still a failed timing result.

The experiment confirms the try53 diagnosis. Removing selected-command status
writes and the hot-loop status condition:

- reduced LUT from try53 `3549` to `3302`;
- reduced FF from try53 `1260` to `1164`;
- preserved scheduler `II=1`;
- improved estimated Fmax from try53 `116.18 MHz` to `121.44 MHz`;
- moved the critical path away from `local_status_code`.

This is strong FPGA evidence that status/error handling should become a
separate architectural concern rather than staying in the success hot loop.
However, the design still misses the effective `7.30 ns` clock budget, and the
new critical path is now selection/refill state:

```text
local lane queue -> candidate pair mux -> selected command registers
```

Because the focused profile assumes valid selected-command graphs, it is not a
replacement for the full semantic runtime. It is useful as an evaluation
profile and as justification for a future status side-channel.

## Next Recommendation

The next try should target selected-command capture/refill, not `ERASE` or
status.

A meaningfully different next architecture would register the selected command
over two ticks:

```text
SELECT_SCAN: choose lane and remember lane index only
SELECT_CAPTURE: read/capture the selected active pair
CLASSIFY/EXECUTE: operate on the registered pair
```

This should test whether the new critical path is caused by selecting the lane,
reading the local queue, and writing the selected command record in one
pipelined iteration.
