# Try 38: Direct Staged LINK Append In Compact Parallel Profile

Current status: `failed_csynth_resources`

Branch: `try38`

Architecture: preserve the try37 compact produced-pair buffer and specialize
the selected-command staged `LINK` append path so it bypasses the generic
produced-pair overflow/status mux in the focused parallel profile.

## Status History

| Date | Status | Evidence |
| --- | --- | --- |
| 2026-05-26 | `planned` | Try37 is the best focused parallel result so far (`3998 LUT`, `1148 FF`, `II=1`, slack `-2.00`) and its critical path runs through staged `LINK` substitution read, append-result handling, and `local_status_code`. |
| 2026-05-26 | `implemented` | Added `ENABLE_PARALLEL_DIRECT_LINK_APPEND`, a focused direct-append path for staged selected-command `LINK`, and a direct-link focused synthesis config. |
| 2026-05-26 | `csim_passed` | Default syntax, focused syntax, full CSIM (`a38`), and FPGA-small CSIM (`b38`) passed. |
| 2026-05-26 | `failed_csynth_resources` | Focused CSYNTH completed with scheduler `II=1`, `3749 LUT`, `1148 FF`, `0 DSP`, `0 BRAM`, and slack `-1.97`; this improves area and slightly improves timing, but still misses timing. |

## Goal

Reduce the remaining staged `LINK` append/status critical path in the focused
parallel scheduler while preserving default full-profile and FPGA-small
semantics.

## Hypothesis

In the focused compact parallel profile, selected-command `LINK` emits at most
one produced pair and the produced-pair buffer is freshly initialized when a
selected command is captured. The generic `append_produced_active_pair` helper
still checks capacity and feeds queue-overflow status. That check is required
for the full reducer, but in this specific focused `LINK` path it is structural
noise. Writing produced pair slot `0` directly and setting the count to `1`
should remove the overflow/status mux from the staged `LINK` append tick.

## Difference From Prior Tries

Try36 added a completion service and worsened service-phase/status muxing.
Try37 narrowed the produced-pair buffer and improved area. Try38 keeps try37
and targets the exact remaining limiter:

```text
staged LINK previous-port read
append produced pair
local_status_code phi/store
```

This is not another broad scheduler phase split and not another buffer-size
experiment. It is a rule-specific append specialization enabled only for the
focused compact parallel profile.

## Implementation Plan

- Add a compile-time gate:
  - `ENABLE_PARALLEL_DIRECT_LINK_APPEND`
  - default `0`
- Use the direct append path only when:
  - `ENABLE_PARALLEL_DIRECT_LINK_APPEND == 1`
  - `ENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER == 1`
  - selected-command staged `LINK` is enabled
- In selected-command `LINK` append sites:
  - write `produced_pairs[0]` directly;
  - set `produced_pair_count = 1`;
  - avoid generic overflow/status handling in the gated path.
- Preserve the old `append_produced_active_pair` behavior when the gate is off.
- Keep try37 gates enabled:
  - single-site refill pop
  - compact produced-pair buffer
  - selected-command staged `LINK`
- Do not enable try36 completion service.

## Validation Commands

Default syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Try38 focused syntax:

```powershell
cd C:\diploma\interactReduce
g++ -std=c++17 -DFPGA_SMALL_PROFILE=1 -DENABLE_PARALLEL_SERVICE_PHASE=1 -DENABLE_REGISTER_LANE_QUEUE=1 -DENABLE_PARALLEL_SELECTED_COMMAND=1 -DENABLE_PARALLEL_STAGED_LINK=1 -DENABLE_PARALLEL_SPLIT_SELECTED_CLASSIFY=1 -DENABLE_PARALLEL_SPLIT_LINK_WRITEBACK=1 -DENABLE_PARALLEL_SPLIT_REFILL=1 -DENABLE_PARALLEL_SINGLE_REFILL_POP=1 -DENABLE_COMPACT_PARALLEL_PRODUCED_BUFFER=1 -DENABLE_PARALLEL_DIRECT_LINK_APPEND=1 -DENABLE_RULE_COMMUTE=0 -DENABLE_RULE_OPERATE=0 -DENABLE_RULE_SWITCH=0 -DENABLE_RULE_CALL=0 -DENABLE_REDUCER_STATISTICS=0 -DENABLE_PARALLEL_REDUCER_STATISTICS=0 -I C:\AMDDesignTools\2025.2\Vitis\include -I src -fsyntax-only test\reducer_test.cpp
```

Full CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config.cfg --work_dir a38
```

FPGA-small CSIM:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\vitis-run.bat' --mode hls --csim --config C:\diploma\interactReduce\hls_config_fpga_small.cfg --work_dir b38
```

Focused direct-LINK-append CSYNTH:

```powershell
cd C:\diploma\interactReduce
& 'C:\AMDDesignTools\2025.2\Vitis\bin\v++.bat' -c --mode hls --config C:\diploma\interactReduce\hls_config_synth_parallel_fpga_small_structural_phased_link_regqueue_selected_command_staged_link_split_classify_split_writeback_split_refill_singlepop_compact_produced_direct_link_nostats.cfg --work_dir csyn_t38_parallel_direct_link_append
```

## Results

- Host syntax: PASS
- Try38 focused syntax: PASS
- Host execution: focused all-test executable is not applicable because the
  focused synthesis flags deliberately disable `COMMUTE`, `OPERATE`, `SWITCH`,
  and `CALL` while the full testbench still expects them.
- Full CSIM: PASS (`a38`, `Overall Result: PASS`)
- FPGA-small CSIM: PASS (`b38`, `Overall Result: PASS`)
- CSYNTH: completed, kept scheduler `II=1`, reduced LUTs and design size, but
  still failed timing acceptance.

## HLS Evidence

- Work directory: `csyn_t38_parallel_direct_link_append`
- Report paths:
  - `csyn_t38_parallel_direct_link_append/hls/syn/report/csynth.rpt`
  - `csyn_t38_parallel_direct_link_append/hls/syn/report/reducer_core_parallel_csynth.rpt`
  - `csyn_t38_parallel_direct_link_append/hls/syn/report/reducer_core_parallel_Pipeline_VITIS_LOOP_899_3_csynth.rpt`
  - `csyn_t38_parallel_direct_link_append/hls/syn/report/csynth_design_size.rpt`
- LUT: `3749` top-level total, `3591` in the scheduler pipeline
- FF: `1148` top-level total, `1077` in the scheduler pipeline
- DSP: `0`
- BRAM: `0`
- II: scheduler loop `VITIS_LOOP_899_3` reached `II=1`; Vitis printed
  `All loop constraints were satisfied`
- Timing/slack: top/pipeline timing slack `-1.97`; estimated Fmax
  `107.85 MHz`; estimated clock period `9.272 ns`
- Critical path evidence: Vitis still reports the same family of path:
  `substitution_memory_0_load -> selected_command_link_previous_port ->
  icmp_ln1620 -> local_status_code phi/store`. The append helper is no longer
  present in that critical path, so direct append removed one source of
  structural pressure but did not eliminate the staged `LINK` read/status
  dependency.
- Design-size evidence: final HW-transform design size is `574` instructions,
  down from try37's `592`. The selected-command direct append halves the
  remaining top-level `append_produced_active_pair` contribution from `490`
  instruction-equivalent work in try37's selected `LINK`/non-link mix to `245`
  at top level; the remaining `490 (2 calls)` contribution is from non-link
  `ERASE`/`ANNIHILATE`.
- Last Vitis log line if frozen: not applicable; synthesis completed.

## Conclusion

Try38 is the best focused parallel scheduler result so far by LUT and timing,
but it is still not accepted because timing misses the configured effective
clock budget.

| Build | LUT | FF | DSP | BRAM | Main loop II | Slack | Fmax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Try35 single-site refill pop | 4003 | 2114 | 0 | 0 | 1 | `-2.05` | `107.01 MHz` |
| Try37 compact produced buffer | 3998 | 1148 | 0 | 0 | 1 | `-2.00` | `107.48 MHz` |
| Try38 direct staged `LINK` append | 3749 | 1148 | 0 | 0 | 1 | `-1.97` | `107.85 MHz` |

The result validates the rule-specific append specialization: area drops
without losing `II=1`, and the generic append helper disappears from the
reported critical path. The remaining limiter is more fundamental: the staged
`LINK` state still carries a substitution-memory read through
`selected_command_link_previous_port`, comparison against the unbound marker,
and `local_status_code` update.

## Next Recommendation

Keep try38 as the focused parallel base. The next try should stop optimizing
append mechanics and instead split the staged `LINK` previous-port comparison
from the status/control update:

- preserve compact produced buffer and direct staged `LINK` append;
- add a separate staged `LINK` compare/decode phase after the substitution read,
  or store an `is_unbound` bit during the read phase;
- avoid another generic status or completion service unless the report directly
  points at that state again.
