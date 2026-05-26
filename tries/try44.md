# Try 44: Compact Selected-Command Classify

Current status: `failed_csynth_resources`

Branch: `try44`

Architecture: return to the try41 focused scheduler base and replace full
selected-command rule classification with a compact focused-profile classifier.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try43 was semantically correct but regressed to `3754 LUT`, `1288 FF`, `II=1`, `81.81 MHz`, and `-4.92 ns` slack because rule decode and service-phase selection became the critical path. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_SELECTED_COMPACT_CLASSIFY`, a compact selected-command classifier, and a focused FPGA-small synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, focused syntax, full CSIM, and FPGA-small CSIM all passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed, but timing/resource result regressed to `3842 LUT`, `1156 FF`, `II=1`, `68.57 MHz`, and `-7.29 ns` slack. |

## Goal

Reduce selected-command classification/control fan-in without adding another
service tick or routing `ERASE`/`ANNIHILATE` through the try43 staged validation
controller.

## Hypothesis

The focused parallel synthesis profile disables `COMMUTE`, `OPERATE`,
`SWITCH`, and `CALL`. Therefore the selected-command scheduler only needs to
distinguish:

```text
LINK
VOID
ERASE
ANNIHILATE
UNSUPPORTED
```

If SELECT computes this compact rule kind directly from port tags and skips the
generic `PARALLEL_SERVICE_CLASSIFY` tick, the hot execute controller may avoid
the full `get_interaction_rule_kind` control cone that dominated try43. This
preserves try41's staged `LINK` decide/write split and keeps try42/try43 gates
disabled in the measured profile.

## Difference From Prior Tries

Try42 specialized output writes and did not change classification. Try43 added
a second classify/validation tick and made timing worse. Try44 does the
opposite: it removes the extra classify service from the focused selected
command path and uses a smaller classifier during SELECT.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_SELECTED_COMPACT_CLASSIFY`
  - default `0`
- Add a focused classifier helper for selected commands:
  - no `CALL`
  - no numeric `OPERATE`
  - no `SWITCH`
  - no `COMMUTE`
  - recognizes only `LINK`, `VOID`, `ERASE`, `ANNIHILATE`, and unsupported
- In selected-command SELECT:
  - when the compact gate is enabled, compute `selected_command_rule_kind`
    immediately with the compact helper;
  - route directly to EXECUTE instead of `PARALLEL_SERVICE_CLASSIFY`.
- Preserve the old split-classify path when the gate is off.
- Do not enable `ENABLE_PARALLEL_DIRECT_NON_LINK_OUTPUTS` or
  `ENABLE_PARALLEL_STAGED_NON_LINK_VALIDATION` in the focused try44 config.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try44 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_PARALLEL_LINK_DECIDE_WRITE_SPLIT=1 -DENABLE_PARALLEL_SELECTED_COMPACT_CLASSIFY=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only src\reducer_core_parallel.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a44
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b44
```

Focused compact-classify CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_singlepop_compact_produced_direct_link_decide_write_split_compact_classify_nostats.cfg --work_dir csyn_t44_parallel_compact_classify
```

## Results

- Host syntax: passed with default full-profile testbench syntax.
- Focused syntax: passed with the try44 compact-classify flag set.
- Host execution: covered by Vitis CSIM runs.
- Full CSIM: passed in work directory `a44`.
- FPGA-small CSIM: passed in work directory `b44`.
- CSYNTH: completed in work directory `csyn_t44_parallel_compact_classify`,
  but failed the architecture quality target because timing regressed sharply.

## HLS Evidence

- Work directory: `csyn_t44_parallel_compact_classify`
- Report paths:
  - `csyn_t44_parallel_compact_classify/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t44_parallel_compact_classify/hls/syn/report/csynth_design_size.rpt`
- LUT: `3842` top estimate, `3684` pipeline estimate.
- FF: `1156` top estimate, `1085` pipeline estimate.
- DSP: `0`.
- BRAM: `0`.
- II: scheduler reached `II=1` for `VITIS_LOOP_1025_3`.
- Timing/slack: estimated clock `14.585 ns`, Fmax `68.57 MHz`,
  slack `-7.29 ns`.
- Final HW-transform design size: `637` instructions.
- Last Vitis log line if frozen: not applicable; synthesis completed.

The critical path passed through the global active-pair load, lane-local queue
right-port storage/load, the candidate active pair, the compact classifier
(`ports_have_same_tag` / selected rule kind), and then the selected-command
execute path. This shows the compact classifier was placed in the wrong
service tick for hardware.

## Conclusion

Try44 is a failed synthesis-resource/timing experiment.

The compact classifier is semantically correct for the focused rule subset, and
both full and FPGA-small CSIM passed. However, computing the compact rule kind
during SELECT couples classification to lane refill and local queue movement.
That creates a longer path than try43 and much worse timing than try41.

The result is useful diploma evidence: reducing the logical rule set is not
enough if the reduced logic is scheduled in a service phase that already owns
queue/refill control. HLS-visible phase placement matters as much as Boolean
complexity.

## Next Recommendation

Do not use try44 as the next base.

Return to the try41 focused scheduler baseline. A better next attempt should
keep classification out of SELECT and either:

- use a compact classifier only inside the existing CLASSIFY service, with no
  extra non-link validation state; or
- replace `selected_command_rule_kind` with a very narrow class field produced
  in a dedicated classify tick, while preserving the queue/refill boundary.

Full-profile CSIM should continue to run on every semantic code change.
Full-profile CSYNTH should be reserved for milestone architecture checkpoints
or full-profile-specific changes, because full synthesis primarily measures the
complete numeric and inline/full runtime cost rather than the focused parallel
scheduler micro-architecture.
