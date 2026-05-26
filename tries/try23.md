# Try 23: Unpipelined Parallel Service Dispatch Probe

Current status: `csynth_passed`

Branch: `try23`

Architecture: keep the try21 no-`LINK`, register-backed, phased structural
parallel profile, but remove the forced `PIPELINE II=1` pragma from the parent
service-dispatch loop in a focused synthesis build.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try22 showed that non-inlined helper factoring worsened the scheduler to `6309 LUT`, `5390 FF`, main loop `II=6`; HLS still scheduled all helper calls inside one parent pipeline. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_UNPIPELINED_SERVICE_DISPATCH`, conditionally omitted the parent scheduler-loop pipeline pragma, and added a focused synthesis config. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and normal FPGA-small CSIM both passed with `Overall Result: PASS`. |
| 2026-05-26 | `csynth_passed` | Focused CSYNTH completed at `4124 LUT`, `1863 FF`, `0 DSP`, `0 BRAM`; all loop constraints were satisfied, but the parent scheduler loop is no longer pipelined and top timing slack is `-2.19`. |

## Goal

Measure whether the residual parallel scheduler cost comes from forcing refill,
select, execute, and commit dispatch through a single parent loop pipeline.

## Hypothesis

The try21 parent loop is explicitly pipelined with `II=1`, even though each
iteration executes only one service phase. HLS still has to satisfy a single
loop schedule containing all phase branches. Try22 proved that non-inlined helper
functions do not solve this, because the helper instances still sit inside the
same parent pipeline.

Removing the parent pipeline pragma in a focused build may lower control/resource
pressure and show cleaner service-level area/timing, even if it sacrifices a
reported loop II. This is not a final throughput architecture; it is a probe to
separate "bad pipeline constraint" from "bad memory ownership".

## Difference From Prior Tries

Try22 changed C++ function boundaries and kept the parent pipeline. Try23 keeps
the existing inlined service bodies and changes the scheduling constraint: the
parent service-dispatch loop is no longer forcibly pipelined in the focused
profile.

This is closer to the recommendation from try22 because it changes HLS scheduling
ownership instead of adding helper instances.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_UNPIPELINED_SERVICE_DISPATCH`, default `0`.
- When enabled, omit the `#pragma HLS PIPELINE II=1` on the
  `reducer_core_parallel` phased scheduler loop.
- Keep all normal builds and the try21 profile unchanged when the gate is
  disabled.
- Add a focused synthesis config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_unpipelined_nostats.cfg`
- The config should enable:
  - `FPGA_SMALL_PROFILE`
  - `ENABLE_PARALLEL_SERVICE_PHASE`
  - `ENABLE_REGISTER_LANE_QUEUE`
  - `ENABLE_PARALLEL_UNPIPELINED_SERVICE_DISPATCH`
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

Unpipelined service-dispatch focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_UNPIPELINED_SERVICE_DISPATCH=1 -DENABLE_RULE_LINK=0 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir csim_t23_full
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir csim_t23_small
```

Focused unpipelined service-dispatch CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_unpipelined_nostats.cfg --work_dir csyn_t23_parallel_structural_phased_nolink_regqueue_unpipelined
```

## Results

Fill this after implementation.

- Host syntax: passed for the default full-profile build.
- Focused syntax: passed with `FPGA_SMALL_PROFILE`,
  `ENABLE_PARALLEL_SERVICE_PHASE=1`, `ENABLE_REGISTER_LANE_QUEUE=1`,
  `ENABLE_PARALLEL_UNPIPELINED_SERVICE_DISPATCH=1`, no-`LINK` structural rule
  gates, and statistics disabled.
- Host execution: not run as a direct local executable; Vitis CSIM was used for
  executable validation.
- Full CSIM: passed in `csim_t23_full` with `Overall Result: PASS`.
- FPGA-small CSIM: passed in `csim_t23_small` with `Overall Result: PASS`.
- CSYNTH: completed with
  `hls_config_synth_parallel_fpga_small_structural_phased_nolink_regqueue_unpipelined_nostats.cfg`.

## HLS Evidence

Fill this when reports or logs exist.

- Work directory:
  `C:\diploma\interactReduce\csyn_t23_parallel_structural_phased_nolink_regqueue_unpipelined`
- Report paths:
  - `C:\diploma\interactReduce\csyn_t23_parallel_structural_phased_nolink_regqueue_unpipelined\hls\syn\report\csynth.rpt`
  - `C:\diploma\interactReduce\csyn_t23_parallel_structural_phased_nolink_regqueue_unpipelined\hls\syn\report\reducer_core_parallel_csynth.rpt`
  - `C:\diploma\interactReduce\csyn_t23_parallel_structural_phased_nolink_regqueue_unpipelined\hls\syn\report\csynth_design_size.rpt`
- LUT: `4124`
- FF: `1863`
- DSP: `0`
- BRAM: `0`
- Main scheduler loop: `VITIS_LOOP_593_3`
- Main scheduler loop II: not reported because the parent loop is not pipelined.
- Main scheduler loop iteration latency: `2 ~ 89` cycles.
- Loop constraint status: all loop constraints were satisfied.
- Timing/slack: `-2.19`
- Estimated Fmax: `105.35 MHz`
- Design size:
  - compile/link instructions: `14741`
  - HW transform optimized instructions: `1017`
- HLS evidence:
  - `VITIS_LOOP_593_3` is listed as `Pipelined: no`;
  - the remaining long timing path is reported in
    `lane_conflicts_with_committed_work`, with latency up to `41` cycles and
    slack `-0.92`;
  - HLS inferred small auto RAMs for the phased produced-pair buffer, but total
    internal BRAM remains `0`.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try23 succeeded as a diagnostic probe, but it is not a final parallel
throughput architecture.

Compared with the two previous focused profiles:

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try21 parent pipeline, register queues | 3914 | 2071 | 0 | 0 | 3 | `-0.02` | `136.57 MHz` |
| Try22 helper-factored services | 6309 | 5390 | 0 | 0 | 6 | `-3.76` | `90.42 MHz` |
| Try23 unpipelined parent dispatch | 4124 | 1863 | 0 | 0 | n/a | `-2.19` | `105.35 MHz` |

Removing the parent pipeline avoids the try22 helper-instance explosion and
keeps area close to try21. It also eliminates the explicit parent-loop II
violation because the loop is no longer constrained as a pipeline. That is
evidence that the `II=3` in try21 is partly a consequence of forcing all service
branches into one parent pipeline schedule.

The cost is throughput ambiguity: without the parent pipeline, the report no
longer gives a meaningful active-pair issue interval. The long residual timing
path moves into conflict checking, especially `lane_conflicts_with_committed_work`.

For diploma wording:

```text
Try23 shows that removing the parent pipeline constraint can keep the structural
parallel reducer small and satisfy loop constraints, but it does not provide a
usable throughput target. The result supports a service-loop architecture: each
phase should become its own narrow pipelined loop or controller state, rather
than one pipelined dispatcher containing all phases.
```

## Next Recommendation

The next try should convert the try23 diagnostic into an explicit architecture:

- keep the parent controller unpipelined or lightly controlled;
- pipeline only the service loops that do repeated work;
- start with conflict/select, because the unpipelined report identifies
  `lane_conflicts_with_committed_work` as the longest remaining timing path;
- consider a simpler pairwise two-lane conflict comparator instead of the
  generic committed-work loop for the fixed `REDUCER_LANE_COUNT=2` case.
