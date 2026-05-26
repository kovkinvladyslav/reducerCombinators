# Try 50: Direct ERASE Pending Status

Current status: `failed_csynth_resources`

Branch: `try50`

Architecture: keep try49's direct selected `ERASE` datapath, but route the rare
invalid-node status through a tiny ERASE-specific status service instead of
writing `local_status_code` in the same cycle as produced-pair formation.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try49 reduced LUT to `3661`, but timing stayed at `118.28 MHz` and about `-1.16 ns` slack. The critical path remained selected ERASE node-address validation into `local_status_code`. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_DIRECT_ERASE_PENDING_STATUS`, a direct-ERASE pending status flag/code, and focused config `hls_config_try50_direct_erase_pending_status_nostats.cfg`. |
| 2026-05-26 | `csim_passed` | Default syntax, focused syntax, full-profile CSIM, and FPGA-small CSIM passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed, but timing did not improve and the critical path still reaches `local_status_code`; LUT is only `2` lower than try49. |

## Goal

Test whether separating selected `ERASE` invalid-address status from the common
success datapath reduces the focused scheduler critical path.

## Hypothesis

Try49 improved area by specializing selected `ERASE`, but timing did not move
because the range-check failure path still writes `local_status_code` in the
same service tick. In normal valid reductions, selected `ERASE` succeeds and
emits two produced pairs. The invalid-node path is rare and can be committed
through a tiny status service without changing the external status result.

If the direct ERASE helper records only a pending status flag/code on invalid
address, and a later status service writes `local_status_code`, HLS may remove
the `local_status_code` phi/store from the selected ERASE datapath.

## Difference From Prior Tries

Try33 tried a broad pending-status mechanism for many selected-command errors
and increased control/resources. Try50 is intentionally narrower:

- only selected direct `ERASE`;
- only invalid node-address status;
- keep the try49 direct success datapath;
- do not defer staged `LINK`, queue overflow, or generic non-link status.

This makes the experiment a targeted status-path probe rather than another
general status protocol.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_DIRECT_ERASE_PENDING_STATUS`
  - default `0`
- Add pending status registers only when the gate is enabled:
  - `direct_erase_pending_status_valid`
  - `direct_erase_pending_status_code`
- Let `PARALLEL_SERVICE_STATUS` be reachable when either the old broad
  pending-status gate or the new direct-ERASE gate is enabled.
- In `execute_selected_erase_direct(...)` when the new gate is enabled:
  - on invalid node address, set direct-ERASE pending status and route to
    `PARALLEL_SERVICE_STATUS`;
  - do not write `local_status_code` in the helper;
  - return not committed.
- Preserve the existing direct status write when the new gate is disabled.
- Add a focused synthesis config enabling:
  - `ENABLE_PARALLEL_TRUE_CLASS_ONLY_SELECTED=1`
  - `ENABLE_PARALLEL_DIRECT_SELECTED_ERASE=1`
  - `ENABLE_PARALLEL_DIRECT_ERASE_PENDING_STATUS=1`

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try50 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT=1 -DENABLE_PARALLEL_TRUE_CLASS_ONLY_SELECTED=1 -DENABLE_PARALLEL_DIRECT_SELECTED_ERASE=1 -DENABLE_PARALLEL_DIRECT_ERASE_PENDING_STATUS=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a50
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b50
```

Focused CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_try50_direct_erase_pending_status_nostats.cfg --work_dir csyn_t50_parallel_direct_erase_pending_status
```

## Results

- Host syntax: passed for default testbench and focused `reducer_core_parallel.cpp`.
- Host execution: not run separately; covered by Vitis CSIM testbench.
- Full CSIM: passed with `hls_config.cfg` in work directory `a50`.
- FPGA-small CSIM: passed with `hls_config_fpga_small.cfg` in work directory `b50`.
- CSYNTH: completed in `csyn_t50_parallel_direct_erase_pending_status`, but did not meet the timing-improvement goal.

## HLS Evidence

- Work directory: `csyn_t50_parallel_direct_erase_pending_status`
- Report paths:
  - `csyn_t50_parallel_direct_erase_pending_status/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t50_parallel_direct_erase_pending_status/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_1164_3_csynth.rpt`
  - `csyn_t50_parallel_direct_erase_pending_status/hls/syn/report/csynth_design_size.rpt`
- LUT: `3659`
- FF: `1165`
- DSP: `0`
- BRAM: `0`
- Main scheduler loop II: `1`
- Timing/slack: estimated period `8.455 ns`, uncertainty `2.70 ns`, effective target `7.30 ns`, estimated Fmax `118.28 MHz`; approximately `-1.155 ns` against the effective budget.
- Design size: `19,738` instructions after compile/link, `602` after HW transforms.
- Last Vitis log line if frozen: not applicable; synthesis completed.

The critical path remained:

```text
selected_command_produced_active_pairs.produced_pairs.left_port
is_erasure_port / node-address select
node-address range check
local_status_code phi/store
```

The log still reports:

```text
Cannot meet target clock period ... selected_command_lane_index ... combination delay: 9.01814 ns
critical path ... local_status_code_write_ln1036
All loop constraints were satisfied.
Estimated Fmax: 118.28 MHz
```

## Conclusion

Try50 is a negative timing result with a tiny area improvement. The added
direct-ERASE pending status path reduced total LUT from try49's `3661` to
`3659`, but it did not remove the selected ERASE range-check/status path from
the scheduler critical path. HLS still merges the invalid-status path into the
main `local_status_code` phi even though the source code routes it through a
separate service phase.

This suggests the bottleneck is no longer just the explicit write inside the
direct ERASE helper. It is the broader status-selection structure at the end of
the scheduler tick, where HLS combines status candidates from multiple service
paths.

## Next Recommendation

Do not continue extending pending-status flags. The next different approach
should remove selected `ERASE` status production from the focused fast path
more structurally, for example by making focused selected `ERASE` assume
prevalidated binary-node addresses and moving validation to the classify/read
boundary, or by splitting successful produced-pair emission and error/status
commit into separate mutually exclusive scheduler phases before HLS creates
the shared `local_status_code` phi.
