# Try 17: Parallel No-Statistics Synthesis Checkpoint

Current status: `csynth_passed`

Branch: `try17`

Architecture: focused FPGA-small parallel reducer synthesis with statistics
and `CALL` disabled

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try16 showed the focused parallel-bank-metric top completed synthesis but was far too large, with a `1152`-bit statistics port, `39085 LUT`, `51611 FF`, `II=12`, and negative slack. |
| 2026-05-26 | `implemented` | Added a focused no-statistics/no-`CALL` parallel synthesis config. |
| 2026-05-26 | `csim_passed` | Host syntax and host execution passed with `Overall Result: PASS`. |
| 2026-05-26 | `csynth_passed` | No-statistics/no-`CALL` parallel CSYNTH completed in `csyn_t17_parallel_small_nostats`. |

## Goal

Determine whether the experimental two-lane reducer's current synthesis cost is
dominated by the full statistics/control output surface or by the parallel
datapath and local queue scheduler itself.

## Hypothesis

If the try16 resource explosion is mostly caused by the wide
`ReducerStatistics` interface and statistics update network, then disabling
both reducer statistics and parallel statistics should substantially reduce
LUT/FF use and may improve timing.

If the no-statistics parallel top remains large and still has a scheduler
memory-dependency II violation, then the next architecture work should target
the parallel datapath, local queues, and explicit memory arbitration instead of
only adding narrower counters.

## Difference From Prior Tries

Try16 added new bank-conflict counters and intentionally kept statistics enabled
to prove those counters were synthesized in the parallel scheduler path. Try17
does not add more counters and does not change scheduler semantics. It is a
diagnostic synthesis checkpoint that removes the statistics surface from the
same FPGA-small/no-`CALL` parallel top to isolate the control/datapath cost.

This is also different from tries 2-15, which focused on the single-lane
controller, staged `CALL`, queue service phases, and full-profile numeric
resource policy.

## Implementation Plan

- Add a focused synthesis config:
  - `hls_config_synth_parallel_fpga_small_nostats_nocall.cfg`
  - top: `reducer_core_parallel`
  - profile: `FPGA_SMALL_PROFILE`
  - disabled: `ENABLE_RULE_CALL`, `ENABLE_REDUCER_STATISTICS`,
    `ENABLE_PARALLEL_REDUCER_STATISTICS`
- Do not change interaction-combinator semantics.
- Do not remove try16's bank metrics; they remain available in statistics
  builds.
- Compare against try16's focused parallel-bank-metric synthesis result:
  - `39085 LUT`
  - `51611 FF`
  - `2 DSP`
  - `4 BRAM`
  - main scheduler loop `II=12`
  - top slack `-9.93`
  - statistics port width `1152` bits

## Validation Commands

Host syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Host execution:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src test\reducer_test.cpp src\reducer_core.cpp src\reducer_core_parallel.cpp -o try17_host_test.exe
.\try17_host_test.exe
```

Focused no-statistics parallel CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_nostats_nocall.cfg --work_dir csyn_t17_parallel_small_nostats
```

## Results

- Host syntax: passed.
- Host execution: passed. `try17_host_test.exe` reported `Overall Result:
  PASS`.
- Full CSIM: not repeated. This try did not change reducer semantics or
  runtime code; host execution covered the full testbench and the branch only
  adds a synthesis config.
- FPGA-small CSIM: not repeated for the same reason. The new config is a
  CSYNTH-only no-statistics measurement profile.
- CSYNTH: completed with
  `hls_config_synth_parallel_fpga_small_nostats_nocall.cfg`.

## HLS Evidence

- Work directory:
  `C:\diploma\interactReduce\csyn_t17_parallel_small_nostats`
- Report paths:
  `C:\diploma\interactReduce\csyn_t17_parallel_small_nostats\hls\syn\report\csynth.rpt`
- LUT: `11111`
- FF: `5759`
- DSP: `2`
- BRAM: `4`
- Main scheduler loop: `VITIS_LOOP_300_2`
- Main scheduler loop II: `12`
- Violation type: `Memory Dependency`
- Timing/slack: `-6.14`
- Estimated Fmax: `74.42 MHz`
- `reducer_statistics` argument: still present as an unused `1152`-bit
  `ap_none` input because it remains in the C top-level signature, but HLS marks
  it `unused` in SW I/O and no statistics update logic is synthesized.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try17 succeeded as a diagnostic synthesis checkpoint.

Disabling reducer and parallel statistics removed most of the try16 parallel
resource explosion:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Try16 bank metrics, statistics enabled | 39085 | 51611 | 2 | 4 | 12 | `-9.93` |
| Try17 no statistics | 11111 | 5759 | 2 | 4 | 12 | `-6.14` |

The improvement proves that the wide statistics update/output network is not a
minor detail in the experimental parallel reducer. It dominates LUT/FF in the
statistics-enabled measurement top.

However, the no-statistics top still does not become a practical parallel FPGA
runtime. It remains over half of the `xc7a35t` LUT budget, still has negative
timing slack, and still has the same `II=12` memory-dependency limiter. The
critical path reported by HLS starts at a load from
`reducer_lanes.local_active_pair_queue.left_port`, flows through rule dispatch,
and ends in a substitution-memory write path. This shows that after statistics
are removed, the real architecture problem is the monolithic two-lane scheduler
loop with local queue reads, conflict checks, rule dispatch, and memory writes
all in one pipeline.

## Next Recommendation

Do not add a narrow bank-only statistics interface yet. Try17 shows that it
would recover measurement visibility, but it would not solve the `II=12`
parallel scheduler limiter.

The next try should reduce the parallel datapath itself. Good next directions:

- create a structural parallel-core synthesis profile that includes only
  `VOID`, `LINK`, `ERASE`, and `ANNIHILATE` to isolate whether `COMMUTE`,
  `SWITCH`, and `OPERATE` are responsible for most of the parallel LUT/timing
  cost;
- or split the parallel reducer into explicit phases for local-queue pop,
  conflict decision, lane execution, and commit, matching the successful
  single-lane service-phase pattern.

For diploma wording, try17 supports this distinction:

```text
Statistics made the experimental parallel reducer much larger, but removing
statistics did not fix the throughput limiter. The remaining blocker is the
combined two-lane scheduling and memory-access loop, not only measurement
instrumentation.
```
