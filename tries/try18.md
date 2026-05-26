# Try 18: Structural Parallel-Core Synthesis Probe

Current status: `csynth_passed`

Branch: `try18`

Architecture: FPGA-small/no-statistics parallel reducer with only structural
small rules enabled

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try17 showed no-statistics parallel synthesis completed at `11111 LUT`, `5759 FF`, `2 DSP`, `4 BRAM`, main loop `II=12`, and `-6.14` slack. |
| 2026-05-26 | `implemented` | Added `ENABLE_RULE_COMMUTE`, gated parallel dispatch/application for disabled `COMMUTE`, and added a structural parallel synthesis config. |
| 2026-05-26 | `csim_passed` | Default host syntax and execution passed with `Overall Result: PASS`; structural gated syntax also passed. |
| 2026-05-26 | `csynth_passed` | Structural FPGA-small/no-statistics parallel CSYNTH completed in `csyn_t18_parallel_structural`. |

## Goal

Isolate whether the remaining parallel top cost and `II=12` scheduler limiter
come primarily from allocation-heavy rule bodies (`COMMUTE`, `OPERATE`,
`SWITCH`, `CALL`) or from the two-lane queue/conflict/commit structure itself.

## Hypothesis

If the allocation-heavy rules dominate the parallel datapath, then a structural
parallel-core profile containing only `VOID`, `LINK`, `ERASE`, and
`ANNIHILATE` should substantially reduce LUT/FF use, improve timing, and maybe
reduce the main loop II.

If the structural profile remains `II=12`, then the primary limiter is not the
large rule set. It is the monolithic parallel scheduler loop: local queue read,
candidate dispatch, conflict detection, rule execution, produced-pair enqueue,
and memory writes are still scheduled together.

## Difference From Prior Tries

Try16 measured same-bank conflicts with statistics enabled. Try17 removed the
statistics update network but kept the full FPGA-small parallel rule set except
`CALL`. Try18 is different because it narrows the parallel synthesis datapath
itself. It disables `COMMUTE`, `OPERATE`, `SWITCH`, and `CALL` only for a
focused synthesis profile.

This does not replace the semantic reducer. It is an isolation probe for the
experimental parallel top.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_RULE_COMMUTE`, default `1`.
- Gate `COMMUTE` in interaction dispatch and lane rule application, matching
  the existing `ENABLE_RULE_OPERATE`, `ENABLE_RULE_SWITCH`, and
  `ENABLE_RULE_CALL` pattern.
- Gate `produced_rule_needs_allocation` so disabled allocation-heavy rules do
  not remain in the conflict/allocation-contention datapath.
- Add a focused synthesis config:
  - `hls_config_synth_parallel_fpga_small_structural_nostats.cfg`
  - top: `reducer_core_parallel`
  - profile: `FPGA_SMALL_PROFILE`
  - disabled: `ENABLE_RULE_COMMUTE`, `ENABLE_RULE_OPERATE`,
    `ENABLE_RULE_SWITCH`, `ENABLE_RULE_CALL`, `ENABLE_REDUCER_STATISTICS`,
    `ENABLE_PARALLEL_REDUCER_STATISTICS`
- Preserve defaults so full-profile behavior and existing tests remain
  unchanged.
- Compare against try17:
  - `11111 LUT`
  - `5759 FF`
  - `2 DSP`
  - `4 BRAM`
  - main scheduler loop `II=12`
  - slack `-6.14`

## Validation Commands

Host syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Host execution:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src test\reducer_test.cpp src\reducer_core.cpp src\reducer_core_parallel.cpp -o try18_host_test.exe
.\try18_host_test.exe
```

Structural parallel syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Focused structural parallel CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_nostats.cfg --work_dir csyn_t18_parallel_structural
```

## Results

- Host syntax: passed for the default full-profile build.
- Structural syntax: passed with `FPGA_SMALL_PROFILE`, `COMMUTE`,
  `OPERATE`, `SWITCH`, `CALL`, reducer statistics, and parallel statistics
  disabled.
- Host execution: passed. `try18_host_test.exe` reported `Overall Result:
  PASS` with default gates, proving the new `ENABLE_RULE_COMMUTE` default
  preserves existing semantics and tests.
- Full CSIM: not repeated. This try adds a default-on rule gate and a focused
  synthesis profile; default host execution covered the full testbench.
- FPGA-small CSIM: not repeated. The structural profile intentionally disables
  semantic rules, so it is a synthesis isolation probe rather than a replacement
  correctness baseline.
- CSYNTH: completed with
  `hls_config_synth_parallel_fpga_small_structural_nostats.cfg`.

## HLS Evidence

- Work directory:
  `C:\diploma\interactReduce\csyn_t18_parallel_structural`
- Report paths:
  `C:\diploma\interactReduce\csyn_t18_parallel_structural\hls\syn\report\csynth.rpt`
- LUT: `4631`
- FF: `2873`
- DSP: `0`
- BRAM: `4`
- Main scheduler loop: `VITIS_LOOP_300_2`
- Main scheduler loop II: `8`
- Violation type: `Memory Dependency`
- Timing/slack: `-7.79`
- Estimated Fmax: `66.26 MHz`
- Design size:
  - compile/link instructions: `19608`
  - HW transform optimized instructions: `734`
- Disabled-rule evidence:
  - no `apply_lane_commute_rule`, `apply_lane_operate_rule`,
    `apply_lane_switch_rule`, or `apply_numeric_operation` helper is listed in
    the inlined pipeline summary;
  - `allocated_node_count` and `allocated_variable_count` are marked unused in
    SW I/O, because the structural profile contains no allocation-heavy rules.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try18 succeeded as a structural parallel-core isolation probe.

Compared with try17, disabling `COMMUTE`, `OPERATE`, `SWITCH`, and `CALL`
removes a large amount of datapath logic:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Try17 no statistics, full FPGA-small/no-`CALL` rules | 11111 | 5759 | 2 | 4 | 12 | `-6.14` |
| Try18 structural small rules only | 4631 | 2873 | 0 | 4 | 8 | `-7.79` |

The resource result is positive and clean. The structural top is much smaller,
uses no DSPs, and confirms that allocation-heavy and numeric rule bodies
contribute substantial area to the experimental parallel reducer.

The scheduling result is only partial. The main loop improves from `II=12` to
`II=8`, but the violation remains `Memory Dependency`. This means that rule
body complexity is not the only problem. Even with only `VOID`, `LINK`,
`ERASE`, and `ANNIHILATE`, the parallel top still reads local queues, dispatches
rules, checks conflicts, executes lanes, enqueues produced pairs, updates local
queues, and writes substitution/global memory inside one pipelined loop.

For diploma wording, try18 supports this statement:

```text
Removing allocation-heavy rules makes the parallel reducer much smaller and
reduces the scheduler II, but it does not eliminate the memory-dependency
limiter. Practical parallel execution therefore needs an architectural phase
split or explicit memory arbitration, not just a smaller rule set.
```

## Next Recommendation

The next try should not keep deleting rules. Try18 already proved that rule
body reduction helps but does not solve the scheduler dependency.

A meaningfully different next architecture is a parallel service-phase split:

- refill local lane queues from the global queue in one service phase;
- peek/select candidates in a conflict-decision phase;
- execute at most the accepted lanes in an execution phase;
- commit produced pairs to local/global queues in a separate phase.

The goal should be to make memory ownership and service order visible to HLS,
similar to the successful single-lane compact service-phase architecture.
