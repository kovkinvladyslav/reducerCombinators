# Try 25: VOID-Only Commit-Writeback Isolation Probe

Current status: `failed_csynth_resources`

Branch: `try25`

Architecture: keep the try21 phased structural parallel scheduler with
register-backed lane queues, but synthesize a focused `VOID`-only profile to
remove produced-pair commit/writeback pressure from the measured scheduler loop.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try24 specialized two-lane SELECT but produced essentially the same result as try21: `3913 LUT`, `2070 FF`, `0 DSP`, `0 BRAM`, main loop `II=3`, slack `-0.02`. |
| 2026-05-26 | `implemented` | Added default-on `ENABLE_RULE_ERASE` and `ENABLE_RULE_ANNIHILATE` gates, guarded dispatch/lane execution, added the focused VOID-only synthesis config, and passed default plus focused syntax checks. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and FPGA-small CSIM both completed with `Overall Result: PASS`. |
| 2026-05-26 | `failed_csynth_resources` | Focused VOID-only synthesis completed and reached loop `II=1`, but timing/resource quality is not acceptable as a next baseline: `2730 LUT`, `1469 FF`, `0 DSP`, `0 BRAM`, top slack `-9.15`, estimated Fmax `60.78 MHz`. |

## Goal

Isolate whether the remaining `II=3` in the focused phased parallel scheduler
depends on rules that produce active pairs and therefore require commit/writeback
service ownership.

## Hypothesis

Try24 shows SELECT/conflict specialization does not move the pipelined result.
The remaining shared state is likely in service ownership: local/global queue
counters, produced-pair buffers, phase/status control, and commit/writeback.

If a `VOID`-only focused profile improves II or simplifies the loop report, then
the produced-pair commit/writeback path is a primary target. If it remains at
`II=3`, then the limiter is broader scheduler bookkeeping even without produced
pairs.

## Difference From Prior Tries

Try20 removed `LINK` to isolate substitution memory. Try21 changed local queue
storage. Try24 changed SELECT/conflict logic.

Try25 changes the rule subset further only for measurement: it disables
`ERASE` and `ANNIHILATE` in the focused synthesis config, leaving only `VOID`.
This directly removes produced-pair generation and commit writeback from active
workloads, instead of changing select logic or queue storage again.

## Implementation Plan

- Add default-on rule gates:
  - `ENABLE_RULE_ERASE`
  - `ENABLE_RULE_ANNIHILATE`
- Guard dispatch and lane rule execution for `ERASE` and `ANNIHILATE` with
  those gates.
- Preserve default behavior with both gates enabled.
- Add a focused synthesis config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_voidonly_regqueue_nostats.cfg`
- The config should enable:
  - `FPGA_SMALL_PROFILE`
  - `ENABLE_PARALLEL_SERVICE_PHASE`
  - `ENABLE_REGISTER_LANE_QUEUE`
- The config should disable:
  - `ENABLE_RULE_LINK`
  - `ENABLE_RULE_ERASE`
  - `ENABLE_RULE_ANNIHILATE`
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

VOID-only focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_RULE_LINK=0 -DENABLE_RULE_ERASE=0 -DENABLE_RULE_ANNIHILATE=0 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir csim_t25_full
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir csim_t25_small
```

Focused VOID-only CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_voidonly_regqueue_nostats.cfg --work_dir csyn_t25_parallel_structural_phased_voidonly_regqueue
```

## Results

Fill this after implementation.

- Host syntax: PASS
- Focused syntax: PASS
- Host execution:
- Full CSIM: PASS (`csim_t25_full`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS (`csim_t25_small`, `Overall Result: PASS`)
- CSYNTH: completed, but marked resource/timing failure

## HLS Evidence

- Work directory: `csyn_t25_parallel_structural_phased_voidonly_regqueue`
- Report paths:
  - `csyn_t25_parallel_structural_phased_voidonly_regqueue/hls/syn/report/csynth.rpt`
  - `csyn_t25_parallel_structural_phased_voidonly_regqueue/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t25_parallel_structural_phased_voidonly_regqueue/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_722_3_csynth.rpt`
  - `csyn_t25_parallel_structural_phased_voidonly_regqueue/hls/syn/report/csynth_design_size.rpt`
- LUT: `2730`
- FF: `1469`
- DSP: `0`
- BRAM: `0`
- II: main scheduler loop `II=1`, iteration latency `2`
- Timing/slack: top slack `-9.15`, estimated Fmax `60.78 MHz`, estimated clock `16.452 ns` for `10 ns` target with `2.70 ns` uncertainty
- Last Vitis log line if frozen: not applicable; synthesis completed

## Conclusion

The try is useful evidence, but not a viable architecture baseline.

Positive evidence:

- Removing `ERASE` and `ANNIHILATE` from the focused profile, leaving only
  `VOID`, lets HLS schedule the parent phased scheduler at `II=1`.
- This confirms that the produced-pair/rule writeback path is part of the
  remaining `II=3` limiter seen in try21 and try24.

Negative evidence:

- Timing becomes much worse than try21/try24: top slack falls from about
  `-0.02` to `-9.15`, and estimated Fmax falls from about `136.57 MHz` to
  `60.78 MHz`.
- The design-size report still shows nontrivial logic in
  `get_interaction_rule_kind`, `lane_conflicts_with_committed_work`, and queue
  counter/status updates even with only `VOID` enabled.
- Therefore, disabling rule bodies is an isolation probe, not a synthesizable
  runtime strategy.

Architectural interpretation: produced-pair commit/writeback explains the
remaining initiation-interval pressure, but the parent scheduler still has too
much combined queue/control/status muxing to be a clean high-frequency
parallel runtime.

## Next Recommendation

The next try should not remove more rules. It should keep the useful
structural profile and split ownership:

- SELECT should produce a one-entry selected-lane command register.
- EXECUTE should consume only that command and produce bounded writeback
  metadata.
- COMMIT should be the only phase that mutates local/global queue counters and
  produced-pair indices.

This is meaningfully different from try22 because it should move state across
explicit registers, not only factor C++ helper functions.
