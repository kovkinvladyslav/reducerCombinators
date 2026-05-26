# Try 9: Derive Produced-Buffer Activity From Count

Current status: `csynth_passed`

Branch: `try9`

Architecture: remove the separate produced-buffer active flag and derive pending buffer service from the narrow produced-pair count

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try8 reached `9626 LUT`, `4246 FF`, final `II=1`, but Vitis still reported `produced_pair_buffer_active -> targetBlock` as the critical path. |
| 2026-05-26 | `implemented` | Removed the separate `produced_pair_buffer_active` flag and derived buffer-service activity from the narrow produced-pair count. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM passed; FPGA-small CSIM was blocked before test execution by Windows Application Control on generated `csim.exe`. |
| 2026-05-26 | `csynth_passed` | FPGA-small derived-buffer-activity CSYNTH completed with final loop `II=1`, `9615 LUT`, `4269 FF`, and `2 DSP`. |

## Goal

Reduce the service-selection control path around produced-pair commit without changing queue ownership or interaction-combinator semantics.

## Hypothesis

Try8 showed that narrowing the produced-pair cursor recovers area, but the explicit `produced_pair_buffer_active` flag remains in the critical path. The flag is redundant: the produced buffer is active exactly when its bounded count is nonzero.

If buffer activity is derived from `produced_active_pairs.produced_pair_count != 0` and the separate active flag is removed, HLS should no longer maintain a 1-bit phi with many incoming rule-specific assignments. This may reduce LUT/FF use and could move or shorten the critical path.

## Difference From Prior Tries

Try7 compacted completion metadata. Try8 narrowed produced-buffer count and cursor width. Try9 changes the structure of the produced-buffer service state itself by removing the separate active flag. It does not change staged `CALL`, queue ownership, helper partitioning, or rule semantics.

## Implementation Plan

1. Remove `produced_pair_buffer_active` from the single-lane produced-buffer path.
2. Use `!produced_active_pairs_are_empty(&produced_active_pairs)` to decide:
   - whether the reducer has pending buffer work in the main loop condition;
   - whether to enter the produced-pair flush service phase;
   - whether a staged `CALL` begin emitted pending redexes.
3. After the last produced pair is flushed, call `initialize_produced_active_pairs` so the count becomes zero and buffer activity becomes false.
4. Preserve existing completion behavior:
   - flushed interactions count only after all produced pairs are committed;
   - staged `CALL` does not complete while its state remains active;
   - errors still report immediately.
5. Keep try8's narrowed cursor and compact completion metadata.
6. Add FPGA-small CSIM and synthesis configs for the try9 name.
7. Validate full CSIM, FPGA-small CSIM, and FPGA-small CSYNTH.
8. Compare against try8 for LUT, FF, DSP, final II, timing, and critical-path text.

## Validation Commands

Run from `C:\diploma\interactReduce`.

```powershell
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -DENABLE_DEFERRED_COMPLETION_ACCOUNTING=1 -DENABLE_INLINE_SMALL_RULE_HELPERS=1 -DENABLE_COMPACT_COMPLETION_STATE=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir interactReduce_csim_full_try9
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small_derived_buffer_activity.cfg --work_dir interactReduce_csim_small_derived_buffer_activity_try9
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_fpga_small_derived_buffer_activity.cfg --work_dir interactReduce_csynth_small_derived_buffer_activity_try9
```

## Results

- Host syntax: passed for full profile, FPGA-small try9 flags, and the no-statistics synthesis flag set.
- Host execution: not run as a separate executable; Vitis CSIM was used where the generated executable was allowed by the OS.
- Full CSIM: passed with `hls_config.cfg` in `interactReduce_csim_full_try9`; log reports `Overall Result: PASS` and `CSim done with 0 errors`.
- FPGA-small CSIM: blocked by Windows Application Control before test execution. Vitis generated `csim.exe`, then reported `couldn't execute ".\csim.exe": invalid argument`; direct execution of the generated binary was blocked by policy. This is an external execution-policy failure, not a reducer result.
- CSYNTH: passed with `hls_config_synth_fpga_small_derived_buffer_activity.cfg` in `interactReduce_csynth_small_derived_buffer_activity_try9`.

## HLS Evidence

- Work directory: `interactReduce_csynth_small_derived_buffer_activity_try9`
- Report paths:
  - `interactReduce_csynth_small_derived_buffer_activity_try9/hls/syn/report/csynth.rpt`
  - `interactReduce_csynth_small_derived_buffer_activity_try9/hls/syn/report/reducer_core_csynth.rpt`
  - `interactReduce_csynth_small_derived_buffer_activity_try9/hls/syn/report/csynth_design_size.rpt`
- LUT: `9615`
- FF: `4269`
- DSP: `2`
- BRAM: `0`
- II: target `1`, final `1` for `VITIS_LOOP_138_1`
- Timing/slack: estimated clock `8.510 ns`, summary slack `-1.21`, estimated Fmax `117.51 MHz`
- Design size: `59016` instructions after compile/link, `2076` after HW transform optimizations
- Last Vitis log line if frozen: not applicable; synthesis completed.

Compared with try8:

| Build | LUT | FF | DSP | Main loop II | Timing/slack |
| --- | ---: | ---: | ---: | ---: | ---: |
| Try8 narrow produced-buffer cursor | 9626 | 4246 | 2 | 1 | `-1.21` |
| Try9 derived buffer activity | 9615 | 4269 | 2 | 1 | `-1.21` |

Try9 saved `11 LUT` but added `23 FF` while preserving final `II=1`.

The critical path changed from the try8 `produced_pair_buffer_active` phi to:

```text
pending_completion_active phi -> targetBlock call to reducer_core_Pipeline_VITIS_LOOP_138_1
```

This means deriving buffer activity from the count did remove the named active
buffer flag from the HLS critical path. It did not improve timing, because the
same broad service-selection path now exposes the pending-completion flag.

## Conclusion

Try9 is a small positive diagnostic result, not a major area improvement. It
confirms that `produced_pair_buffer_active` was real service-control state in
the HLS schedule, because removing it moved the named critical path. The cost
is essentially neutral: `II=1` and timing are preserved, LUT changes by only
`-11`, and FF increases by `23`.

For the FPGA architecture, the important result is that produced-buffer activity
can be represented by existing bounded count state. The remaining timing issue
is now the deferred completion service flag, so further progress should target
service-state encoding rather than individual rule datapaths.

## Next Recommendation

The next try should combine the remaining service selectors into one compact
explicit service phase, for example `SERVICE_COMPLETE`, `SERVICE_FLUSH`,
`SERVICE_CALL`, and `SERVICE_DISPATCH`, instead of carrying independent active
flags for pending completion, produced-pair flush, and staged `CALL`. The goal
should be to preserve try9's `II=1` while reducing the `pending_completion_active`
critical path and avoiding a repeat of separate flag fanout.
