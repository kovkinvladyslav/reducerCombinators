# Try 8: Narrow Produced-Buffer Cursor

Current status: `csynth_passed`

Branch: `try8`

Architecture: narrow produced-pair buffer count and flush cursor after try7 identified produced-buffer state as the remaining control-path pressure

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try7 reached `9997 LUT`, `4354 FF`, final `II=1`, and Vitis still reported `produced_pair_buffer_active -> targetBlock` in the critical path. |
| 2026-05-26 | `implemented` | Added `ProducedPairCount`, narrowed produced-pair count and flush cursor to 5 bits, and added FPGA-small CSIM/CSYNTH configs. |
| 2026-05-26 | `csim_passed` | Full-profile CSIM and FPGA-small narrow-buffer-cursor CSIM both passed. |
| 2026-05-26 | `csynth_passed` | FPGA-small narrow-buffer-cursor CSYNTH completed with final loop `II=1`, `9626 LUT`, `4246 FF`, and `2 DSP`. |

## Goal

Reduce produced-pair buffer state width while preserving the single-owner queue commit service and the `II=1` scheduler result.

## Hypothesis

The produced-pair buffer can hold at most `MAXIMUM_PRODUCED_ACTIVE_PAIRS_PER_INTERACTION`, currently `17`, but its count and flush index are stored as `ap_uint<32>`. Those over-wide fields feed comparisons, address selection, and active-state updates around the commit service.

If the produced-pair count and flush cursor are narrowed to a small compile-time width, HLS should reduce LUT/FF use in the produced-buffer control network. This may also reduce pressure around the `produced_pair_buffer_active` path identified in try7.

## Difference From Prior Tries

Try5 introduced deferred completion. Try6 inlined tiny helpers. Try7 removed unnecessary rule-kind metadata in no-statistics builds. Try8 targets a different state source: the width of the produced-pair buffer cursor and count. It does not change queue ownership, staged `CALL`, helper partitioning, or completion semantics.

## Implementation Plan

1. Add a compile-time constant for produced-pair counter width.
2. Change `ProducedActivePairs::produced_pair_count` from `ap_uint<32>` to the narrow produced-pair count type.
3. Change the reducer's `produced_pair_flush_index` to the same narrow type.
4. Change `append_produced_active_pair` and `flush_one_produced_active_pair` to use the narrow type internally.
5. Preserve behavior:
   - maximum produced pair capacity remains unchanged;
   - queue overflow status remains unchanged;
   - completed interactions still count only after produced pairs flush;
   - full-profile CSIM remains the semantic guard.
6. Add FPGA-small CSIM and synthesis configs using try7 gates plus the narrowed cursor implementation.
7. Compare against try7 for LUT, FF, DSP, final II, and timing.

## Validation Commands

Run from `C:\diploma\interactReduce`.

```powershell
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_STAGED_CALL=1 -DENABLE_PRODUCED_PAIR_BUFFER=1 -DENABLE_DEFERRED_COMPLETION_ACCOUNTING=1 -DENABLE_INLINE_SMALL_RULE_HELPERS=1 -DENABLE_COMPACT_COMPLETION_STATE=1 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir interactReduce_csim_full_try8
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small_narrow_buffer_cursor.cfg --work_dir interactReduce_csim_small_narrow_buffer_cursor_try8
```

```powershell
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_fpga_small_narrow_buffer_cursor.cfg --work_dir interactReduce_csynth_small_narrow_buffer_cursor_try8
```

## Results

- Host syntax: passed for full profile, FPGA-small try8 flags, and the no-statistics synthesis flag set.
- Host execution: not run as a separate executable; Vitis CSIM was used as the correctness path.
- Full CSIM: passed with `hls_config.cfg` in `interactReduce_csim_full_try8`.
- FPGA-small CSIM: passed with `hls_config_fpga_small_narrow_buffer_cursor.cfg` in `interactReduce_csim_small_narrow_buffer_cursor_try8`.
- CSYNTH: passed with `hls_config_synth_fpga_small_narrow_buffer_cursor.cfg` in `interactReduce_csynth_small_narrow_buffer_cursor_try8`.

## HLS Evidence

- Work directory: `interactReduce_csynth_small_narrow_buffer_cursor_try8`
- Report paths:
  - `interactReduce_csynth_small_narrow_buffer_cursor_try8/hls/syn/report/csynth.rpt`
  - `interactReduce_csynth_small_narrow_buffer_cursor_try8/hls/syn/report/reducer_core_csynth.rpt`
  - `interactReduce_csynth_small_narrow_buffer_cursor_try8/hls/syn/report/csynth_design_size.rpt`
- LUT: `9626`
- FF: `4246`
- DSP: `2`
- BRAM: `0`
- II: target `1`, final `1` for `VITIS_LOOP_139_1`
- Timing/slack: estimated clock `8.510 ns`, slack `-1.21`, estimated Fmax `117.51 MHz`
- Design size: `59105` instructions after compile/link, `2083` after HW transform optimizations
- Last Vitis log line if frozen: not applicable; synthesis completed.

Compared with try7:

| Build | LUT | FF | DSP | Main loop II | Timing/slack |
| --- | ---: | ---: | ---: | ---: | ---: |
| Try7 compact completion metadata | 9997 | 4354 | 2 | 1 | `-1.21` |
| Try8 narrow produced-buffer cursor | 9626 | 4246 | 2 | 1 | `-1.21` |

Try8 saved `371 LUT` and `108 FF` while preserving final `II=1`.

The report confirms the produced-pair cursor width changed: the flush index and
produced-pair count now appear as 5-bit values in the detailed utilization
report. For example, `produced_pair_flush_index` and
`produced_active_pairs_produced_pair_count` are listed with `Bits = 5`.

The critical path remains:

```text
produced_pair_buffer_active phi -> targetBlock call to reducer_core_Pipeline_VITIS_LOOP_139_1
```

Therefore narrowing the cursor recovered area but did not remove the active-flag
control-path limiter.

## Conclusion

Successful area-recovery experiment. Narrowing the produced-buffer cursor is a
clean FPGA-oriented improvement: it preserves semantics and final `II=1`, and
it recovers more area than the previous compact-completion metadata step.

It does not improve timing. The active produced-buffer flag still fans into the
pipeline target block, so the remaining timing issue is structural service
selection rather than the cursor width.

## Next Recommendation

The next try should change the structure of the active produced-buffer flag
itself. A good candidate is to derive buffer activity from a compact remaining
count or service phase, so HLS does not maintain a separate
`produced_pair_buffer_active` phi with many incoming rule-specific values.
