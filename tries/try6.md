# Try 6: Selective Small-Rule Inlining

Current status: `csynth_passed`

Branch: `try6`

Architecture: preserve try5 deferred completion while selectively inlining tiny buffered rules for area recovery

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try5 reached final loop `II=1`, but LUT/FF increased to `10591 LUT` and `4425 FF` because rule helpers became separate RTL modules. |
| 2026-05-26 | `implemented` | Added `ENABLE_INLINE_SMALL_RULE_HELPERS`, forced inline on `LINK`, `ERASE`, and `ANNIHILATE` buffered helpers, and added FPGA-small CSIM/CSYNTH configs. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and FPGA-small inline-small-rules CSIM both passed. |
| 2026-05-26 | `csynth_passed` | FPGA-small inline-small-rules CSYNTH completed with final loop `II=1`, `10292 LUT`, `4385 FF`, and `2 DSP`. |

## Goal

Recover some of try5's LUT/FF growth while preserving the deferred completion boundary that made the reducer loop scheduler-clean.

## Hypothesis

Try5 made the phase boundary real by preventing completion accounting from sharing a cycle with rule execution. Vitis then emitted several rule helpers as separate modules. This improved II and timing, but small helpers such as `LINK`, `ERASE`, and `ANNIHILATE` may not need a separate module boundary because they are constant-size rewrites with simple local datapaths.

If only tiny buffered rules are forced inline while larger rules remain separate service modules, area may drop without returning to try4's broad critical path. This is an area-recovery experiment, not a semantic change.

## Difference From Prior Tries

Try5 deferred completion accounting and reached `II=1`. Try6 keeps that boundary intact. It does not change queue ownership, `CALL` staging, or completion timing. It only tests whether helper-module granularity can be tuned after the scheduler boundary is fixed.

## Implementation Plan

1. Add a compile-time gate, `ENABLE_INLINE_SMALL_RULE_HELPERS`, defaulting to disabled.
2. Add explicit `#pragma HLS INLINE` inside the small buffered helpers:
   - `apply_link_rule_buffered`
   - `apply_erase_rule_buffered`
   - `apply_annihilate_rule_buffered`
3. Leave larger helpers non-forced:
   - `begin_staged_call_buffered`
   - `step_staged_call_buffered`
   - `apply_operate_rule_buffered`
   - `apply_switch_rule_buffered`
   - `apply_commute_rule_buffered`
4. Add FPGA-small CSIM and synthesis configs enabling:
   - `FPGA_SMALL_PROFILE`
   - `ENABLE_STAGED_CALL`
   - `ENABLE_PRODUCED_PAIR_BUFFER`
   - `ENABLE_DEFERRED_COMPLETION_ACCOUNTING`
   - `ENABLE_INLINE_SMALL_RULE_HELPERS`
5. Validate against full CSIM, FPGA-small CSIM, and FPGA-small CSYNTH.
6. Compare against try5 for LUT, FF, II, timing, and whether all loop constraints remain satisfied.

## Validation Commands

Run from `C:\diploma\interactReduce`.

```powershell
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -DENABLE_DEFERRED_COMPLETION_ACCOUNTING=1 -DENABLE_INLINE_SMALL_RULE_HELPERS=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir interactReduce_csim_full_try6
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small_inline_small_rules.cfg --work_dir interactReduce_csim_small_inline_small_rules_try6
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_fpga_small_inline_small_rules.cfg --work_dir interactReduce_csynth_small_inline_small_rules_try6
```

## Results

- Host syntax: passed for full profile and FPGA-small inline-small-rules flags.
- Host execution: not run as a separate binary; Vitis CSIM was used as the correctness path.
- Full CSIM: passed with `hls_config.cfg` in `interactReduce_csim_full_try6`.
- FPGA-small CSIM: passed with `hls_config_fpga_small_inline_small_rules.cfg` in `interactReduce_csim_small_inline_small_rules_try6`.
- CSYNTH: passed with `hls_config_synth_fpga_small_inline_small_rules.cfg` in `interactReduce_csynth_small_inline_small_rules_try6`.

## HLS Evidence

- Work directory: `interactReduce_csynth_small_inline_small_rules_try6`
- Report paths:
  - `interactReduce_csynth_small_inline_small_rules_try6/hls/syn/report/csynth.rpt`
  - `interactReduce_csynth_small_inline_small_rules_try6/hls/syn/report/reducer_core_csynth.rpt`
  - `interactReduce_csynth_small_inline_small_rules_try6/hls/syn/report/csynth_design_size.rpt`
- LUT: `10292`
- FF: `4385`
- DSP: `2`
- BRAM: `0`
- II: target `1`, final `1` for `VITIS_LOOP_129_1`
- Timing/slack: estimated clock `8.510 ns`, slack `-1.21`, estimated Fmax `117.51 MHz`
- Design size: `58823` instructions after compile/link, `2084` after HW transform optimizations
- Last Vitis log line if frozen: not applicable; synthesis completed.

Compared with try5:

| Build | LUT | FF | DSP | Main loop II | Timing/slack |
| --- | ---: | ---: | ---: | ---: | ---: |
| Try5 deferred completion | 10591 | 4425 | 2 | 1 | `-1.21` |
| Try6 inline small helpers | 10292 | 4385 | 2 | 1 | `-1.21` |

Try6 saved `299 LUT` and `40 FF` while preserving final `II=1`.
The small helpers no longer appear as standalone modules in the top synthesis
summary, which confirms the inline pragmas were applied. Larger helpers remain
visible as separate modules, including `begin_staged_call_buffered`,
`step_staged_call_buffered`, `apply_operate_rule_buffered`,
`apply_switch_rule_buffered`, and `apply_commute_rule_buffered`.

## Conclusion

Successful modest area-recovery experiment. Selectively inlining the tiny
buffered rules reduces LUT/FF use without losing the try5 scheduler result.
The result is useful but not a full area solution: timing slack is unchanged,
and the remaining area is still dominated by broader reducer state/control,
staged `CALL`, and numeric/medium-rule helper logic.

## Next Recommendation

Keep try6 as the current FPGA-small queue/scheduler baseline. The next try
should not continue blindly inlining tiny helpers, because the easy module
overhead has already been removed. A better next experiment is to reduce the
wide pending/produced-control state or pack/narrow completion state so the
`II=1` architecture keeps its throughput while recovering more area.
