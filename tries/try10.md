# Try 10: Compact Reducer Service Phase

Current status: `csynth_passed`

Branch: `try10`

Architecture: replace separate pending-completion service selection with a compact explicit reducer service phase

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try9 preserved final `II=1` and moved the critical path from `produced_pair_buffer_active` to `pending_completion_active`. |
| 2026-05-26 | `implemented` | Added `ENABLE_COMPACT_SERVICE_PHASE`, service-phase constants, gated reducer service-phase control, and FPGA-small CSIM/CSYNTH configs. |
| 2026-05-26 | `csim_passed` | FPGA-small compact-service-phase CSIM passed with `Overall Result: PASS`; full CSIM was blocked before execution by Windows Application Control. |
| 2026-05-26 | `csynth_passed` | FPGA-small compact-service-phase CSYNTH completed with final loop `II=1`, `9646 LUT`, `4356 FF`, `2 DSP`, and positive estimated slack. |

## Goal

Reduce the remaining service-selection control path in the FPGA-small reducer without changing interaction-combinator semantics.

## Hypothesis

Try9 removed the produced-buffer active flag from the critical path, but the same service-selection structure exposed `pending_completion_active` as the new limiter. The reducer still carries service choice as several independent predicates: pending completion, produced-pair flush, active staged `CALL`, and normal dispatch.

If those mutually exclusive service choices are represented by one compact service-phase register, HLS may reduce phi/mux pressure around the pipeline target block. The intended hardware is a tiny controller: complete one pending interaction, flush one produced pair, service one staged `CALL` step, or dispatch one active pair.

## Difference From Prior Tries

Try7 compacted completion metadata. Try8 narrowed the produced-buffer count and cursor. Try9 removed the separate produced-buffer active flag. Try10 is different because it changes the representation of reducer service selection itself: pending completion, produced-buffer flush, staged `CALL`, and dispatch become explicit phase states instead of separate active-bit checks.

## Implementation Plan

1. Add a compile-time gate, `ENABLE_COMPACT_SERVICE_PHASE`, defaulting to disabled.
2. Add compact service phase constants:
   - `REDUCER_SERVICE_DISPATCH`
   - `REDUCER_SERVICE_COMPLETE`
   - `REDUCER_SERVICE_FLUSH`
   - `REDUCER_SERVICE_CALL`
3. Under `ENABLE_COMPACT_SERVICE_PHASE && ENABLE_DEFERRED_COMPLETION_ACCOUNTING && ENABLE_PRODUCED_PAIR_BUFFER`, replace `pending_completion_active` with a small service-phase register.
4. Use the phase register to select one service per loop tick:
   - `COMPLETE`: increment `executed_steps`;
   - `FLUSH`: push one produced pair into the queue;
   - `CALL`: perform one staged `CALL` service step;
   - `DISPATCH`: pop and execute one active pair when step budget and queue allow.
5. Preserve semantics:
   - rule execution still completes only after produced pairs are flushed;
   - staged `CALL` completes exactly once;
   - errors still stop immediately with the same status mapping;
   - `maximum_steps` still counts completed interactions, not service ticks.
6. Keep try9's gates enabled for measurement:
   - `FPGA_SMALL_PROFILE`
   - `ENABLE_STAGED_CALL`
   - `ENABLE_PRODUCED_PAIR_BUFFER`
   - `ENABLE_DEFERRED_COMPLETION_ACCOUNTING`
   - `ENABLE_INLINE_SMALL_RULE_HELPERS`
   - `ENABLE_COMPACT_COMPLETION_STATE`
7. Add FPGA-small CSIM and synthesis configs enabling `ENABLE_COMPACT_SERVICE_PHASE`.
8. Validate full CSIM, FPGA-small CSIM if Windows allows the generated executable, and FPGA-small CSYNTH.
9. Compare against try9 for LUT, FF, DSP, final II, timing, and critical-path text.

## Validation Commands

Run from `C:\diploma\interactReduce`.

```powershell
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -DENABLE_DEFERRED_COMPLETION_ACCOUNTING=1 -DENABLE_INLINE_SMALL_RULE_HELPERS=1 -DENABLE_COMPACT_COMPLETION_STATE=1 -DENABLE_COMPACT_SERVICE_PHASE=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir interactReduce_csim_full_try10
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small_compact_service_phase.cfg --work_dir interactReduce_csim_small_compact_service_phase_try10
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_fpga_small_compact_service_phase.cfg --work_dir interactReduce_csynth_small_compact_service_phase_try10
```

## Results

- Host syntax: passed for full profile, FPGA-small try10 flags, and the no-statistics synthesis flag set.
- Host execution: not run as a separate host executable.
- Full CSIM: blocked by Windows Application Control before test execution. Vitis generated `csim.exe`, then reported `couldn't execute ".\csim.exe": invalid argument`; direct execution reported `An Application Control policy has blocked this file`. The compact service phase is disabled in the default full profile, so full-profile syntax is the available guard for default behavior in this try.
- FPGA-small CSIM: passed with `hls_config_fpga_small_compact_service_phase.cfg` in `interactReduce_csim_small_compact_service_phase_try10`; log reports `Overall Result: PASS` and `CSim done with 0 errors`.
- CSYNTH: passed with `hls_config_synth_fpga_small_compact_service_phase.cfg` in `interactReduce_csynth_small_compact_service_phase_try10`.

## HLS Evidence

- Work directory: `interactReduce_csynth_small_compact_service_phase_try10`
- Report paths:
  - `interactReduce_csynth_small_compact_service_phase_try10/hls/syn/report/csynth.rpt`
  - `interactReduce_csynth_small_compact_service_phase_try10/hls/syn/report/reducer_core_csynth.rpt`
  - `interactReduce_csynth_small_compact_service_phase_try10/hls/syn/report/csynth_design_size.rpt`
- LUT: `9646`
- FF: `4356`
- DSP: `2`
- BRAM: `0`
- II: target `1`, final `1` for `VITIS_LOOP_142_1`
- Timing/slack: estimated clock `7.287 ns`, summary slack `+0.01`, estimated Fmax `137.23 MHz`
- Design size: `59407` instructions after compile/link, `2078` after HW transform optimizations
- Last Vitis log line if frozen: not applicable; synthesis completed.

Compared with try9:

| Build | LUT | FF | DSP | Main loop II | Estimated clock | Timing/slack |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Try9 derived buffer activity | 9615 | 4269 | 2 | 1 | `8.510 ns` | `-1.21` |
| Try10 compact service phase | 9646 | 4356 | 2 | 1 | `7.287 ns` | `+0.01` |

Try10 added `31 LUT` and `87 FF` while preserving final `II=1`, but it improved
the estimated clock by `1.223 ns` and cleared the previous negative timing
estimate.

The top synthesis summary no longer reports a timing issue for
`reducer_core`. The loop pipeline has positive slack:

```text
reducer_core_Pipeline_VITIS_LOOP_142_1: slack +2.01
VITIS_LOOP_142_1: II 1, slack +7.30
top reducer_core: slack +0.01
```

This supports the try10 hypothesis: replacing independent service-active
predicates with a compact phase register made service selection clearer to HLS.

## Conclusion

Try10 is a successful scheduler/control experiment. The compact service phase
is the first try in this sequence to preserve `II=1` and produce non-negative
estimated top-level timing slack on the FPGA-small synthesis target.

The tradeoff is small but visible area growth: `31 LUT` and `87 FF` over try9.
This is acceptable as a diploma result because it demonstrates the hardware
cost of making reducer service selection explicit, and it gives a measurable
timing benefit.

This is not yet the final runtime architecture. The next work should verify
whether the phase register can be simplified or whether the small FF increase
is the necessary price for timing closure.

## Next Recommendation

Keep the compact service phase as the current FPGA-small control baseline. The
next try should avoid undoing the explicit phase boundary. A useful follow-up is
to reduce phase-update fanout or factor repeated produced-buffer setup code so
the design can try to recover some of the `87 FF` increase while keeping
positive timing slack.
