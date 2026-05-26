# Try 7: Compact Completion Metadata

Current status: `csynth_passed`

Branch: `try7`

Architecture: keep try6's `II=1` scheduler boundary while removing unnecessary rule-kind metadata from the no-statistics completion path

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try6 preserved final `II=1` and recovered only `299 LUT`/`40 FF`; remaining area is attributed to broad reducer state/control and staged service muxing. |
| 2026-05-26 | `implemented` | Added `ENABLE_COMPACT_COMPLETION_STATE`, compact no-statistics completion metadata, and FPGA-small CSIM/CSYNTH configs. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and FPGA-small compact-completion CSIM both passed. |
| 2026-05-26 | `csynth_passed` | FPGA-small compact-completion CSYNTH completed with final loop `II=1`, `9997 LUT`, `4354 FF`, and `2 DSP`. |

## Goal

Recover area from the `II=1` FPGA-small reducer without changing interaction-combinator semantics or undoing the deferred completion boundary.

## Hypothesis

The FPGA-small synthesis configs disable `ENABLE_REDUCER_STATISTICS`, so deferred completion accounting only needs to increment `executed_steps`. It does not need to preserve the completed `InteractionRuleKind` for rule counters.

If the no-statistics produced-buffer path carries only the minimum completion metadata, then HLS should have less state and fewer mux inputs around the commit/completion service. This should reduce LUT/FF pressure while preserving try6's `II=1`.

## Difference From Prior Tries

Try5 introduced deferred completion accounting and reached `II=1`. Try6 tuned helper-module granularity by inlining tiny rule helpers. Try7 does not change helper inlining, queue ownership, staged `CALL`, or rule execution. It targets the width of runtime metadata carried between service phases, specifically the rule-kind state that is unnecessary when statistics are disabled.

## Implementation Plan

1. Add a compile-time gate, `ENABLE_COMPACT_COMPLETION_STATE`, defaulting to disabled.
2. Under `ENABLE_COMPACT_COMPLETION_STATE && !ENABLE_REDUCER_STATISTICS`, store pending completion as only an active bit.
3. Under the same condition, store produced-buffer metadata as only whether the buffered rule is an active staged `CALL`.
4. Preserve the existing full metadata path when statistics are enabled, so full-profile CSIM and statistics tests keep their current rule counters.
5. Keep try6's existing gates enabled for measurement:
   - `FPGA_SMALL_PROFILE`
   - `ENABLE_STAGED_CALL`
   - `ENABLE_PRODUCED_PAIR_BUFFER`
   - `ENABLE_DEFERRED_COMPLETION_ACCOUNTING`
   - `ENABLE_INLINE_SMALL_RULE_HELPERS`
6. Add FPGA-small CSIM and synthesis configs enabling `ENABLE_COMPACT_COMPLETION_STATE`.
7. Validate full CSIM, FPGA-small CSIM, and FPGA-small CSYNTH.
8. Compare against try6 for LUT, FF, DSP, final II, and timing.

## Validation Commands

Run from `C:\diploma\interactReduce`.

```powershell
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -DENABLE_DEFERRED_COMPLETION_ACCOUNTING=1 -DENABLE_INLINE_SMALL_RULE_HELPERS=1 -DENABLE_COMPACT_COMPLETION_STATE=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir interactReduce_csim_full_try7
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small_compact_completion.cfg --work_dir interactReduce_csim_small_compact_completion_try7
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_fpga_small_compact_completion.cfg --work_dir interactReduce_csynth_small_compact_completion_try7
```

## Results

- Host syntax: passed for full profile, FPGA-small compact-completion flags, and the no-statistics synthesis flag set.
- Host execution: not run as a separate executable; Vitis CSIM was used as the correctness path.
- Full CSIM: passed with `hls_config.cfg` in `interactReduce_csim_full_try7`.
- FPGA-small CSIM: passed with `hls_config_fpga_small_compact_completion.cfg` in `interactReduce_csim_small_compact_completion_try7`.
- CSYNTH: passed with `hls_config_synth_fpga_small_compact_completion.cfg` in `interactReduce_csynth_small_compact_completion_try7`.

## HLS Evidence

- Work directory: `interactReduce_csynth_small_compact_completion_try7`
- Report paths:
  - `interactReduce_csynth_small_compact_completion_try7/hls/syn/report/csynth.rpt`
  - `interactReduce_csynth_small_compact_completion_try7/hls/syn/report/reducer_core_csynth.rpt`
  - `interactReduce_csynth_small_compact_completion_try7/hls/syn/report/csynth_design_size.rpt`
- LUT: `9997`
- FF: `4354`
- DSP: `2`
- BRAM: `0`
- II: target `1`, final `1` for `VITIS_LOOP_139_1`
- Timing/slack: estimated clock `8.510 ns`, slack `-1.21`, estimated Fmax `117.51 MHz`
- Design size: `59037` instructions after compile/link, `2083` after HW transform optimizations
- Last Vitis log line if frozen: not applicable; synthesis completed.

Compared with try6:

| Build | LUT | FF | DSP | Main loop II | Timing/slack |
| --- | ---: | ---: | ---: | ---: | ---: |
| Try6 inline small helpers | 10292 | 4385 | 2 | 1 | `-1.21` |
| Try7 compact completion metadata | 9997 | 4354 | 2 | 1 | `-1.21` |

Try7 saved `295 LUT` and `31 FF` while preserving final `II=1`.

The critical path remains in reducer service-state control, not in the removed
rule-kind metadata. Vitis reports the path as:

```text
produced_pair_buffer_active phi -> targetBlock call to reducer_core_Pipeline_VITIS_LOOP_139_1
```

This is strong evidence that the next area/timing target should be the
produced-buffer active/control fanout rather than the deferred completion
rule-kind field.

## Conclusion

Successful modest area-recovery experiment. Compact completion metadata reduces
LUT/FF use and keeps the scheduler-clean `II=1` result from try5/try6. It does
not improve timing, which means the remaining limiter is the service-control
network around produced-pair buffer activation and pipeline target selection.

## Next Recommendation

The next try should target the `produced_pair_buffer_active` fanout reported in
the critical path. A meaningfully different direction is to replace the
separate `produced_pair_buffer_active` flag plus 32-bit flush index with a
small explicit produced-buffer service cursor, for example `remaining_count` or
a compact service phase field. The goal should be to keep `II=1` while reducing
the broad active-flag mux that now drives the pipeline target block.
