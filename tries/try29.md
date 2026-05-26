# Try 29: Selected Command With LINK Restored

Current status: `failed_csynth_resources`

Branch: `try29`

Architecture: return to the try26 selected-command scheduler boundary, but
restore `LINK` in the focused structural parallel synthesis profile.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try26 is the best focused parallel-control baseline: `3170 LUT`, `1503 FF`, `0 DSP`, `0 BRAM`, `II=2`, slack `+0.12`, but it disables `LINK`. Try27 and try28 show that adding more command slots inside the hot pipeline hurts timing/resources. |
| 2026-05-26 | `implemented` | Added the focused selected-command synthesis config with `LINK` enabled and allocation-heavy rules/statistics disabled. |
| 2026-05-26 | `implemented` | Default and focused syntax checks passed. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and FPGA-small CSIM both completed with `Overall Result: PASS`. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed, but timing failed: `3558 LUT`, `1422 FF`, `0 DSP`, `0 BRAM`, scheduler loop `II=2`, slack `-2.00`, estimated Fmax `107.48 MHz`. |

## Goal

Measure whether the best current selected-command scheduler can carry `LINK`
substitution-memory behavior in the focused structural parallel profile.

## Hypothesis

The no-`LINK` selected-command profile improves control ownership, but it is
not a complete structural interaction-combinator subset. `LINK` is the key
substitution-memory rule, and earlier tries showed that it is a real HLS
limiter.

If try26's single selected-command boundary isolates execute/commit ownership
well enough, restoring `LINK` may synthesize with acceptable area/timing, even
if the loop II worsens. If it regresses sharply, the next implementation should
stage `LINK` itself instead of carrying its substitution read/write path inside
the selected-command execute phase.

## Difference From Prior Tries

Try20 disabled `LINK` to isolate substitution-memory cost in the phased
scheduler.

Try21 through try28 kept `LINK` disabled while optimizing local queues and
selected-command control.

Try29 is different because it moves back toward the real structural rule set:
`VOID`, `LINK`, `ERASE`, and `ANNIHILATE`, while preserving the try26
single-command scheduler boundary.

## Implementation Plan

- Add a focused synthesis config:
  - `hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_nostats.cfg`
- Use the try26 architecture gates:
  - `ENABLE_PARALLEL_SERVICE_PHASE=1`
  - `ENABLE_REGISTER_LANE_QUEUE=1`
  - `ENABLE_PARALLEL_SELECTED_COMMAND=1`
- Keep `LINK` enabled by not defining `ENABLE_RULE_LINK=0`.
- Keep allocation-heavy and numeric/control rules disabled:
  - `COMMUTE`
  - `OPERATE`
  - `SWITCH`
  - `CALL`
- Disable reducer and parallel statistics for the synthesis datapath
  measurement.
- Do not change runtime code unless syntax or synthesis exposes a real issue.

## Validation Commands

Host syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Focused selected-command with `LINK` syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir csim_t29_full
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir csim_t29_small
```

Focused selected-command with `LINK` CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_nostats.cfg --work_dir csyn_t29_parallel_structural_phased_link_regqueue_selected_command
```

## Results

Fill this after implementation.

- Host syntax: PASS
- Focused syntax: PASS
- Host execution:
- Full CSIM: PASS (`csim_t29_full`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS (`csim_t29_small`, `Overall Result: PASS`)
- CSYNTH: completed, but failed timing/resource acceptance for the focused
  parallel baseline.

## HLS Evidence

- Work directory:
  `csyn_t29_parallel_structural_phased_link_regqueue_selected_command`
- Report paths:
  - `csyn_t29_parallel_structural_phased_link_regqueue_selected_command/hls/syn/report/csynth.rpt`
  - `csyn_t29_parallel_structural_phased_link_regqueue_selected_command/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t29_parallel_structural_phased_link_regqueue_selected_command/hls/syn/report/csynth_design_size.rpt`
- LUT: `3558`
- FF: `1422`
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_773_3` reached `II=2`, with violation type
  `Memory Dependency`
- Timing/slack: top/pipeline slack `-2.00`; estimated Fmax `107.48 MHz`
- Critical path evidence: Vitis reports the path through the restored `LINK`
  datapath, starting at `substitution_memory_0_load`, passing through the
  `previous_port` sparse mux and comparison in `lane_interaction.hpp`, and
  ending in `local_status_code` mux/store logic.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try29 is a useful negative result. The try26 selected-command boundary can
carry `LINK` functionally and still keeps the focused scheduler at `II=2`, but
the restored substitution-memory datapath fails timing. Compared with try26,
adding `LINK` increases LUT use from `3170` to `3558` and moves slack from
`+0.12` to `-2.00`, while preserving `0 BRAM` because the register-backed lane
queues remain active.

This shows that the next bottleneck is not generic command buffering. It is the
specific `LINK` substitution-memory read/previous-port/status path inside the
execute phase.

## Next Recommendation

Do not add more selected-command or waiting-command state inside the same parent
pipeline. The next meaningfully different try should stage `LINK` itself under
the try26 selected-command boundary:

- split `LINK` into read/decide/write service ticks;
- keep selected-command ownership for local/global queue counters;
- preserve full-profile semantics as the reference path;
- measure whether removing the substitution-memory compare/status chain from
  the execute phase restores positive timing with `LINK` enabled.
