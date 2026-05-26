# Try 19: Structural Parallel Service-Phase Scheduler

Current status: `csynth_passed`

Branch: `try19`

Architecture: gated structural parallel scheduler with explicit refill, select,
execute, and produced-pair commit phases

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try18 structural parallel synthesis completed at `4631 LUT`, `2873 FF`, `0 DSP`, `4 BRAM`, main loop `II=8`, and `Memory Dependency`; next recommendation was a service-phase split. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_SERVICE_PHASE`, phased scheduler state, per-lane produced buffers, one-pair commit service, and a focused phased structural synthesis config. |
| 2026-05-26 | `csim_passed` | Default host syntax and execution passed with `Overall Result: PASS`; phased structural gated syntax also passed. |
| 2026-05-26 | `csynth_passed` | Phased structural FPGA-small/no-statistics parallel CSYNTH completed in `csyn_t19_parallel_structural_phased_rerun`. |

## Goal

Test whether making the experimental parallel scheduler's memory ownership
explicit improves HLS scheduling compared with the monolithic parallel loop.

## Hypothesis

Try18 proved that deleting allocation-heavy rules reduces area but does not
remove the memory-dependency limiter. The remaining structural loop still pops
local queues, executes rules, enqueues produced pairs, updates queue counters,
and writes substitution/global memory in the same pipelined region.

If the structural parallel reducer is split into explicit service phases, HLS
should see smaller memory-access regions:

- refill local queues from the global queue;
- select and conflict-check lane candidates;
- execute selected lane rules into per-lane produced buffers;
- commit produced active pairs back to local/global queues one item at a time.

This may reduce the main-loop II or at least move the limiter to a more
specific phase.

## Difference From Prior Tries

Try18 narrowed the rule set but kept the same monolithic scheduler. Try19 keeps
the same structural small-rule profile as try18 and changes scheduler structure
instead. It is not another rule deletion experiment.

The new path is gated so the existing parallel top and full-profile semantics
remain unchanged unless the focused synthesis config enables the new phase
controller.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_SERVICE_PHASE`, default `0`.
- Under that gate, restructure `reducer_core_parallel` into a small service
  controller for the structural parallel profile:
  - `PARALLEL_SERVICE_REFILL`
  - `PARALLEL_SERVICE_SELECT`
  - `PARALLEL_SERVICE_EXECUTE`
  - `PARALLEL_SERVICE_COMMIT`
- Persist across service ticks:
  - selected active pairs;
  - selected rule kinds;
  - lane commit flags;
  - per-lane produced active-pair buffers;
  - produced-pair commit cursor.
- Keep the focused profile structural:
  - `ENABLE_RULE_COMMUTE=0`
  - `ENABLE_RULE_OPERATE=0`
  - `ENABLE_RULE_SWITCH=0`
  - `ENABLE_RULE_CALL=0`
  - statistics disabled.
- Add a synthesis config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_nostats.cfg`
- Preserve default behavior when the gate is disabled.

## Validation Commands

Host syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Phased structural syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Host execution:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src test\reducer_test.cpp src\reducer_core.cpp src\reducer_core_parallel.cpp -o try19_host_test.exe
.\try19_host_test.exe
```

Focused phased structural parallel CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_nostats.cfg --work_dir csyn_t19_parallel_structural_phased
```

## Results

- Host syntax: passed for the default full-profile build.
- Phased structural syntax: passed with `FPGA_SMALL_PROFILE`,
  `ENABLE_PARALLEL_SERVICE_PHASE=1`, structural rule gates, and statistics
  disabled.
- Host execution: passed. `try19_host_test.exe` reported `Overall Result:
  PASS` with default gates, proving the new path remains isolated from normal
  semantics.
- Full CSIM: not repeated. The default host executable covered the full
  testbench, and the new path is a gated synthesis experiment.
- FPGA-small CSIM: not repeated. The phased structural profile intentionally
  disables several semantic rules, so it is not a replacement correctness
  baseline.
- CSYNTH: completed with
  `hls_config_synth_parallel_fpga_small_structural_phased_nostats.cfg`.

## HLS Evidence

- Work directory:
  `C:\diploma\interactReduce\csyn_t19_parallel_structural_phased_rerun`
- Report paths:
  `C:\diploma\interactReduce\csyn_t19_parallel_structural_phased_rerun\hls\syn\report\csynth.rpt`
- LUT: `4344`
- FF: `1821`
- DSP: `0`
- BRAM: `4`
- Main scheduler loop: `VITIS_LOOP_397_3`
- Main scheduler loop II: `4`
- Violation type: `Memory Dependency`
- Timing/slack: `-2.00`
- Estimated Fmax: `107.48 MHz`
- Design size:
  - compile/link instructions: `20522`
  - HW transform optimized instructions: `748`
- Critical path:
  - begins at `substitution_memory_0` load in `apply_lane_link_rule`;
  - flows through the previous-port/unbound comparison;
  - ends in `local_status_code` mux/store.
- New local storage evidence:
  - HLS inferred small RAMs for per-lane produced-pair buffers:
    `phased_produced_active_pairs_produced_pairs_*`.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try19 succeeded as a service-phase scheduler experiment.

Compared with try18, the explicit refill/select/execute/commit phases improve
both scheduler II and resource use:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try18 structural monolithic scheduler | 4631 | 2873 | 0 | 4 | 8 | `-7.79` | `66.26 MHz` |
| Try19 structural phased scheduler | 4344 | 1821 | 0 | 4 | 4 | `-2.00` | `107.48 MHz` |

The result confirms the architectural hypothesis: the parallel reducer benefits
from making memory service order explicit. Separating lane execution from
produced-pair queue commit cuts the II in half and substantially improves
timing.

The result is not final. The loop is still memory-dependency limited, now on
the structural `LINK` path through `substitution_memory_0` and status-control
muxing. That means the next bottleneck is narrower and more useful:
substitution-memory issue/commit for parallel `LINK`, rather than the whole
monolithic scheduler.

For diploma wording, try19 supports this statement:

```text
The parallel reducer improves when modeled as explicit scheduler services.
Phase splitting does not make the two-lane design final, but it turns the
limiter from broad queue/rule/commit entanglement into a more specific
substitution-memory dependency.
```

## Next Recommendation

The next try should target the remaining `LINK` substitution-memory dependency.
Good options:

- add an explicit substitution-write service for structural `LINK`, so the
  execute phase reads/detects and a later phase performs the write/produced-pair
  emission;
- or specialize the structural parallel profile to separate `LINK` from
  `VOID/ERASE/ANNIHILATE`, measuring whether non-substitution structural rules
  can reach lower II when substitution memory is removed from the execution
  phase.

Do not return to deleting large rules as the main strategy; try18 and try19
already show scheduler phase structure is the stronger architecture direction.
