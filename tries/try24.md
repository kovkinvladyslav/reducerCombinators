# Try 24: Fixed Two-Lane Pairwise Select Probe

Current status: `csynth_passed`

Branch: `try24`

Architecture: keep the try21 no-`LINK`, register-backed, phased structural
parallel profile, but replace the generic committed-work conflict loop with a
fixed two-lane pairwise select path in the focused synthesis build.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try23 completed at `4124 LUT`, `1863 FF`, `0 DSP`, `0 BRAM` with the parent loop unpipelined; it identified `lane_conflicts_with_committed_work` as the long remaining timing path, with latency up to `41` cycles. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_PAIRWISE_SELECT`, direct pairwise node-conflict helpers, a fixed two-lane SELECT path, and a focused pairwise-select synthesis config. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and normal FPGA-small CSIM both passed with `Overall Result: PASS`. |
| 2026-05-26 | `csynth_passed` | Focused CSYNTH completed at `3913 LUT`, `2070 FF`, `0 DSP`, `0 BRAM`, main loop `II=3`, and slack `-0.02`; this is essentially unchanged from try21. |

## Goal

Reduce the select/conflict logic in the phased two-lane scheduler while keeping
the parent scheduler loop pipelined, so the result is comparable to try21.

## Hypothesis

`REDUCER_LANE_COUNT` is fixed at `2`, but the current select stage still uses a
generic committed-work loop and committed arrays. HLS therefore schedules loop
control, array indexing, and 2x2 helper loops for a case that can be expressed
as one direct pairwise decision: accept lane 0 if possible, then accept lane 1
only if it does not conflict with lane 0.

Replacing the generic committed-work path with a fixed two-lane comparator may
reduce timing and resource pressure in the SELECT phase while preserving the
same conservative semantics for two lanes.

## Difference From Prior Tries

Try22 used non-inlined helper functions and made the parent pipeline worse.
Try23 removed the parent pipeline and therefore lost a meaningful issue-interval
measurement.

Try24 keeps the try21 parent pipeline and queue architecture. It changes only
the select/conflict implementation for the fixed two-lane focused profile. This
targets the concrete long path reported by try23 instead of changing global
scheduler structure again.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_PAIRWISE_SELECT`, default `0`.
- Add direct pairwise conflict helpers for the fixed two-lane case:
  - same unsupported-rule behavior;
  - `VOID`/`VOID` remains non-conflicting;
  - same substitution-variable conflict when `LINK` is enabled;
  - same binary-node conflict behavior;
  - same allocation-contention behavior;
  - bank-conflict counters remain metrics only.
- Under the gate, replace the phased SELECT service body with:
  - reset both lanes;
  - peek lane 0 and lane 1;
  - classify both rule kinds;
  - accept lane 0 when eligible;
  - accept lane 1 when eligible and either lane 0 was not accepted or pairwise
    conflict check returns false.
- Preserve the old generic path when the gate is disabled.
- Add a focused synthesis config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_pairwise_select_nostats.cfg`
- The config should enable:
  - `FPGA_SMALL_PROFILE`
  - `ENABLE_PARALLEL_SERVICE_PHASE`
  - `ENABLE_REGISTER_LANE_QUEUE`
  - `ENABLE_PARALLEL_PAIRWISE_SELECT`
- The config should disable:
  - `ENABLE_RULE_LINK`
  - `ENABLE_RULE_COMMUTE`
  - `ENABLE_RULE_OPERATE`
  - `ENABLE_RULE_SWITCH`
  - `ENABLE_RULE_CALL`
  - reducer and parallel statistics.

## Validation Commands

Host syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Pairwise-select focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_PAIRWISE_SELECT=1 -DENABLE_RULE_LINK=0 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir csim_t24_full
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir csim_t24_small
```

Focused pairwise-select CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_pairwise_select_nostats.cfg --work_dir csyn_t24_parallel_structural_phased_nolink_regqueue_pairwise_select
```

## Results

Fill this after implementation.

- Host syntax: passed for the default full-profile build.
- Focused syntax: passed with `FPGA_SMALL_PROFILE`,
  `ENABLE_PARALLEL_SERVICE_PHASE=1`, `ENABLE_REGISTER_LANE_QUEUE=1`,
  `ENABLE_PARALLEL_PAIRWISE_SELECT=1`, no-`LINK` structural rule gates, and
  statistics disabled.
- Host execution: not run as a direct local executable; Vitis CSIM was used for
  executable validation.
- Full CSIM: passed in `csim_t24_full` with `Overall Result: PASS`.
- FPGA-small CSIM: passed in `csim_t24_small` with `Overall Result: PASS`.
- CSYNTH: completed with
  `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_pairwise_select_nostats.cfg`.

## HLS Evidence

Fill this when reports or logs exist.

- Work directory:
  `C:\diploma\interactReduce\csyn_t24_parallel_structural_phased_nolink_regqueue_pairwise_select`
- Report paths:
  - `C:\diploma\interactReduce\csyn_t24_parallel_structural_phased_nolink_regqueue_pairwise_select\hls\syn\report\csynth.rpt`
  - `C:\diploma\interactReduce\csyn_t24_parallel_structural_phased_nolink_regqueue_pairwise_select\hls\syn\report\reducer_core_parallel_csynth.rpt`
  - `C:\diploma\interactReduce\csyn_t24_parallel_structural_phased_nolink_regqueue_pairwise_select\hls\syn\report\reducer_core_parallel_Pipeline_VITIS_LOOP_722_3_csynth.rpt`
  - `C:\diploma\interactReduce\csyn_t24_parallel_structural_phased_nolink_regqueue_pairwise_select\hls\syn\report\csynth_design_size.rpt`
- LUT: `3913`
- FF: `2070`
- DSP: `0`
- BRAM: `0`
- Main scheduler loop: `VITIS_LOOP_722_3`
- Main scheduler loop II: `3`
- Violation type: `Resource Limitation`
- Timing/slack: `-0.02`
- Estimated Fmax: `136.57 MHz`
- Design size:
  - compile/link instructions: `17816`
  - HW transform optimized instructions: `618`
- HLS evidence:
  - the pairwise helper is inlined into the parent pipeline;
  - only one top pipeline remains in the report;
  - no separate `lane_conflicts_with_committed_work` function appears in the
    focused top report, but the parent-loop II remains `3`;
  - total internal BRAM remains `0`.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try24 succeeded as a measurement, but it did not improve the practical scheduler
result.

Compared with try21:

| Build | LUT | FF | DSP | BRAM | Main loop II | Violation | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | --- | ---: | ---: |
| Try21 generic select/conflict | 3914 | 2071 | 0 | 0 | 3 | Resource Limitation | `-0.02` | `136.57 MHz` |
| Try24 fixed pairwise select | 3913 | 2070 | 0 | 0 | 3 | Resource Limitation | `-0.02` | `136.57 MHz` |

The fixed two-lane select path removed the generic committed-work loop from the
focused top, but HLS produced effectively the same hardware. This means the
generic conflict loop observed in try23 is not the active limiter once the
parent scheduler is pipelined. The remaining `II=3` is more likely caused by
state updates shared across service phases: local queue counters, global queue
head/tail/count, produced-pair buffer state, and phase/status control.

For diploma wording:

```text
Try24 shows that specializing the conflict check to the fixed two-lane case does
not materially change the pipelined scheduler. The remaining II limit is not the
generic committed-work loop by itself; it is the combined service-state update
inside the parent scheduler pipeline.
```

## Next Recommendation

The next try should target service-state ownership rather than select logic.
Good candidates:

- keep pairwise select disabled unless needed, because it does not improve the
  focused result;
- split commit/writeback into a separate narrow loop or service that is the only
  owner of local/global queue counters;
- or add a one-entry selected-lane command register so SELECT and EXECUTE do not
  update candidate state, produced-pair state, and queue counters in the same
  parent pipeline.
