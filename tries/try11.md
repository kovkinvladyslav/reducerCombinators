# Try 11: Produced-Flush Activation Helper

Current status: `csynth_passed`

Branch: `try11`

Architecture: keep the compact service phase and factor repeated produced-buffer activation assignments

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try10 preserved final `II=1`, improved estimated clock to `7.287 ns`, and reached `+0.01` slack, but added `87 FF` over try9. |
| 2026-05-26 | `implemented` | Added a forced-inline `activate_produced_pair_flush` helper and replaced repeated produced-buffer activation assignments. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and FPGA-small compact-service-phase CSIM both passed with `Overall Result: PASS`. |
| 2026-05-26 | `csynth_passed` | FPGA-small flush-activation-helper CSYNTH completed with the same resources and timing as try10. |

## Goal

Recover some of try10's FF/control overhead while preserving the explicit compact service phase that produced positive timing slack.

## Hypothesis

Try10 made reducer service selection explicit, but it also duplicated the same produced-buffer activation pattern after every buffered rule:

- reset the produced-pair flush cursor;
- set the service phase to `REDUCER_SERVICE_FLUSH`;
- record whether the buffered producer is a staged `CALL` or a normal rule.

If this repeated update pattern is factored into a tiny forced-inline helper, HLS may share or simplify the phase/update mux network without losing the service-phase boundary. This is an area/control cleanup experiment, not a semantic rewrite.

## Difference From Prior Tries

Try10 introduced the compact service phase and proved it can clear the FPGA-small timing estimate. Try11 keeps that architecture. It does not return to independent active flags, does not change `CALL` staging, and does not change the queue commit service. It only factors repeated produced-buffer activation assignments after rule execution.

## Implementation Plan

1. Add a gated inline helper for the compact-service-phase produced-buffer path.
2. The helper should set:
   - `produced_pair_flush_index = 0`;
   - `reducer_service_phase = REDUCER_SERVICE_FLUSH`;
   - compact buffered metadata (`buffered_rule_is_call`) or full metadata (`buffered_rule_kind`) depending on the existing statistics/compact gates.
3. Replace repeated produced-buffer activation blocks in `reducer_core.cpp` with the helper.
4. Preserve all try10 semantics:
   - produced pairs still flush before an interaction completes;
   - staged `CALL` still completes exactly once;
   - `maximum_steps` counts completed interactions, not service ticks;
   - errors still stop immediately.
5. Keep try10's configs as the baseline and add try11-named configs only if a new flag is needed. This try does not need a new behavior flag because it is source factoring under existing gates.
6. Validate syntax, FPGA-small CSIM, and FPGA-small CSYNTH.
7. Treat full-profile CSIM as desirable but record Application Control blocking if the generated executable is blocked again.
8. Compare against try10 for LUT, FF, DSP, final II, timing, and whether positive slack remains.

## Validation Commands

Run from `C:\diploma\interactReduce`.

```powershell
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -DENABLE_DEFERRED_COMPLETION_ACCOUNTING=1 -DENABLE_INLINE_SMALL_RULE_HELPERS=1 -DENABLE_COMPACT_COMPLETION_STATE=1 -DENABLE_COMPACT_SERVICE_PHASE=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -DENABLE_DEFERRED_COMPLETION_ACCOUNTING=1 -DENABLE_INLINE_SMALL_RULE_HELPERS=1 -DENABLE_COMPACT_COMPLETION_STATE=1 -DENABLE_COMPACT_SERVICE_PHASE=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir interactReduce_csim_full_try11
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small_compact_service_phase.cfg --work_dir interactReduce_csim_small_flush_activation_try11
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_fpga_small_compact_service_phase.cfg --work_dir interactReduce_csynth_small_flush_activation_try11
```

## Results

- Host syntax: passed for full profile, FPGA-small try11 flags, and the no-statistics synthesis flag set.
- Host execution: not run as a separate host executable.
- Full CSIM: passed with `hls_config.cfg` in `interactReduce_csim_full_try11`; log reports `Overall Result: PASS` and `CSim done with 0 errors`.
- FPGA-small CSIM: passed with `hls_config_fpga_small_compact_service_phase.cfg` in `interactReduce_csim_small_flush_activation_try11`; log reports `Overall Result: PASS` and `CSim done with 0 errors`.
- CSYNTH: passed with `hls_config_synth_fpga_small_compact_service_phase.cfg` in `interactReduce_csynth_small_flush_activation_try11`.

## HLS Evidence

- Work directory: `interactReduce_csynth_small_flush_activation_try11`
- Report paths:
  - `interactReduce_csynth_small_flush_activation_try11/hls/syn/report/csynth.rpt`
  - `interactReduce_csynth_small_flush_activation_try11/hls/syn/report/reducer_core_csynth.rpt`
  - `interactReduce_csynth_small_flush_activation_try11/hls/syn/report/csynth_design_size.rpt`
- LUT: `9646`
- FF: `4356`
- DSP: `2`
- BRAM: `0`
- II: target `1`, final `1` for `VITIS_LOOP_169_1`
- Timing/slack: estimated clock `7.287 ns`, summary slack `+0.01`, estimated Fmax `137.23 MHz`
- Design size: `59391` instructions after compile/link, `2078` after HW transform optimizations
- Last Vitis log line if frozen: not applicable; synthesis completed.

Compared with try10:

| Build | LUT | FF | DSP | Main loop II | Estimated clock | Timing/slack |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Try10 compact service phase | 9646 | 4356 | 2 | 1 | `7.287 ns` | `+0.01` |
| Try11 produced-flush activation helper | 9646 | 4356 | 2 | 1 | `7.287 ns` | `+0.01` |

Try11 changed source structure but did not change synthesized area or timing.
The report confirms that `activate_produced_pair_flush` was inlined into
`reducer_core`, so the neutral result is HLS evidence that the repeated
activation assignments were already optimized equivalently in try10.

## Conclusion

Try11 is a neutral cleanup result. It preserves full and FPGA-small correctness,
preserves try10's positive timing estimate, and does not change resources.

This is useful evidence: the `87 FF` increase from try10 is not caused by
duplicated produced-buffer activation code. It appears to be the structural cost
of carrying explicit service-phase state and its phase registers.

## Next Recommendation

Keep the helper only if source clarity is valued; it is synthesis-neutral. The
next try should target a different area source, such as service metadata that is
only needed when statistics are enabled, or a focused full-profile synthesis
checkpoint now that the FPGA-small control baseline has positive timing slack.
